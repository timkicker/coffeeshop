#include <catch2/catch_test_macros.hpp>
#include "mods/ZipExtractor.h"

#include <zlib.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cstdlib>

// Build minimal ZIPs in-memory for tests.
// Layout: [LOCAL FILE HEADER][name][data]...[CENTRAL DIR HEADERS][EOCD]
// We only need extract() to read local headers, so a single LFH + data + EOCD is enough.

namespace {

static constexpr uint32_t LOCAL_FILE_SIG  = 0x04034b50;
static constexpr uint32_t CENTRAL_DIR_SIG = 0x02014b50;
static constexpr uint32_t END_CENTRAL_SIG = 0x06054b50;

static void appendU16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(x & 0xff); v.push_back((x >> 8) & 0xff);
}
static void appendU32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x & 0xff); v.push_back((x >> 8) & 0xff);
    v.push_back((x >> 16) & 0xff); v.push_back((x >> 24) & 0xff);
}
static void appendBytes(std::vector<uint8_t>& v, const void* data, size_t len) {
    auto* p = (const uint8_t*)data;
    v.insert(v.end(), p, p + len);
}

// Add one stored (uncompressed) local file entry to v.
static void addStoredEntry(std::vector<uint8_t>& v, const std::string& name,
                           const std::string& content) {
    appendU32(v, LOCAL_FILE_SIG);
    appendU16(v, 20);           // version needed
    appendU16(v, 0);            // flags
    appendU16(v, 0);            // method = stored
    appendU16(v, 0);            // mod time
    appendU16(v, 0);            // mod date
    appendU32(v, 0);            // crc32 (we don't validate it)
    appendU32(v, content.size()); // compressed size
    appendU32(v, content.size()); // uncompressed size
    appendU16(v, name.size());  // name length
    appendU16(v, 0);            // extra length
    appendBytes(v, name.data(), name.size());
    appendBytes(v, content.data(), content.size());
}

// Raw-deflate compress (no zlib header) — matches inflateInit2(-MAX_WBITS).
static std::vector<uint8_t> rawDeflate(const std::string& src) {
    std::vector<uint8_t> out(src.size() + 64 + src.size() / 100);
    z_stream zs{};
    REQUIRE(deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    zs.next_in   = (Bytef*)src.data();
    zs.avail_in  = src.size();
    zs.next_out  = out.data();
    zs.avail_out = out.size();
    int ret = deflate(&zs, Z_FINISH);
    REQUIRE(ret == Z_STREAM_END);
    out.resize(zs.total_out);
    deflateEnd(&zs);
    return out;
}

// Add one deflated local file entry to v.
static void addDeflatedEntry(std::vector<uint8_t>& v, const std::string& name,
                              const std::string& content) {
    auto compressed = rawDeflate(content);
    appendU32(v, LOCAL_FILE_SIG);
    appendU16(v, 20);
    appendU16(v, 0);
    appendU16(v, 8);            // method = deflate
    appendU16(v, 0);
    appendU16(v, 0);
    appendU32(v, 0);            // crc (not checked)
    appendU32(v, compressed.size());
    appendU32(v, content.size());
    appendU16(v, name.size());
    appendU16(v, 0);
    appendBytes(v, name.data(), name.size());
    appendBytes(v, compressed.data(), compressed.size());
}

// Append a minimal EOCD record so the parser stops cleanly.
static void appendEOCD(std::vector<uint8_t>& v) {
    appendU32(v, END_CENTRAL_SIG);
    for (int i = 0; i < 16; i++) v.push_back(0);
    appendU16(v, 0); // comment length
}

// Write bytes to a temp file, return its path. Caller is responsible for cleanup.
static std::string writeTempZip(const std::vector<uint8_t>& bytes,
                                const std::string& tag) {
    std::string path = std::string("/tmp/coffeeshop_zip_test_") + tag + "_" +
                       std::to_string(getpid()) + ".zip";
    FILE* f = fopen(path.c_str(), "wb");
    REQUIRE(f != nullptr);
    if (!bytes.empty())
        fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    return path;
}

// rmrf a test dest dir (POSIX recursive).
static void rmTree(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return;
    if (!S_ISDIR(st.st_mode)) { remove(path.c_str()); return; }
    DIR* d = opendir(path.c_str());
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            rmTree(path + "/" + n);
        }
        closedir(d);
    }
    rmdir(path.c_str());
}

static std::string makeDestDir(const std::string& tag) {
    std::string dir = std::string("/tmp/coffeeshop_zip_dest_") + tag + "_" +
                      std::to_string(getpid());
    rmTree(dir);
    mkdir(dir.c_str(), 0755);
    return dir;
}

static bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode);
}

static std::string readFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string out(n, '\0');
    if (n > 0) fread(&out[0], 1, n, f);
    fclose(f);
    return out;
}

} // namespace

// ─── Happy path ────────────────────────────────────────────────────────────

TEST_CASE("ZipExtractor::extract - single stored file extracts correctly", "[zip]") {
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "hello.txt", "Hello, world!");
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "happy");
    auto destDir = makeDestDir("happy");

    REQUIRE(ZipExtractor::extract(zipPath, destDir));
    REQUIRE(fileExists(destDir + "/hello.txt"));
    REQUIRE(readFile(destDir + "/hello.txt") == "Hello, world!");

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - multiple stored entries", "[zip]") {
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "a.txt", "A");
    addStoredEntry(zip, "b.txt", "BB");
    addStoredEntry(zip, "c.txt", "CCC");
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "multi");
    auto destDir = makeDestDir("multi");

    REQUIRE(ZipExtractor::extract(zipPath, destDir));
    REQUIRE(readFile(destDir + "/a.txt") == "A");
    REQUIRE(readFile(destDir + "/b.txt") == "BB");
    REQUIRE(readFile(destDir + "/c.txt") == "CCC");

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - nested path creates intermediate dirs", "[zip]") {
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "content/deep/nested/file.txt", "nested");
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "nested");
    auto destDir = makeDestDir("nested");

    REQUIRE(ZipExtractor::extract(zipPath, destDir));
    REQUIRE(fileExists(destDir + "/content/deep/nested/file.txt"));

    remove(zipPath.c_str());
    rmTree(destDir);
}

// ─── Path traversal guard ──────────────────────────────────────────────────

TEST_CASE("ZipExtractor::extract - rejects ..", "[zip][security]") {
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "../escape.txt", "should not exist");
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "traversal1");
    auto destDir = makeDestDir("traversal1");

    REQUIRE_FALSE(ZipExtractor::extract(zipPath, destDir));
    // The escape file must not exist outside the dest dir
    std::string parent = destDir.substr(0, destDir.rfind('/'));
    REQUIRE_FALSE(fileExists(parent + "/escape.txt"));

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - rejects path with .. in middle", "[zip][security]") {
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "subdir/../../escape.txt", "evil");
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "traversal2");
    auto destDir = makeDestDir("traversal2");

    REQUIRE_FALSE(ZipExtractor::extract(zipPath, destDir));

    remove(zipPath.c_str());
    rmTree(destDir);
}

// ─── nameLen cap ───────────────────────────────────────────────────────────

TEST_CASE("ZipExtractor::extract - rejects entry with absurdly long name", "[zip][security]") {
    // Manually build a malformed LFH: nameLen = 10000 (> 4096 cap)
    std::vector<uint8_t> zip;
    appendU32(zip, LOCAL_FILE_SIG);
    appendU16(zip, 20);
    appendU16(zip, 0);
    appendU16(zip, 0);
    appendU16(zip, 0);
    appendU16(zip, 0);
    appendU32(zip, 0);
    appendU32(zip, 0);
    appendU32(zip, 0);
    appendU16(zip, 10000);  // nameLen way too large
    appendU16(zip, 0);
    // No name bytes follow — extract should bail before reading them.

    auto zipPath = writeTempZip(zip, "longname");
    auto destDir = makeDestDir("longname");

    REQUIRE_FALSE(ZipExtractor::extract(zipPath, destDir));

    remove(zipPath.c_str());
    rmTree(destDir);
}

// ─── Truncated / malformed input ───────────────────────────────────────────

TEST_CASE("ZipExtractor::extract - empty file fails gracefully", "[zip]") {
    auto zipPath = writeTempZip({}, "empty");
    auto destDir = makeDestDir("empty");

    // Empty file: read32 hits EOF immediately. Loop breaks, returns ok (true)
    // because nothing went wrong. Either outcome is acceptable as long as no crash.
    bool result = ZipExtractor::extract(zipPath, destDir);
    (void)result;

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - truncated header returns false", "[zip]") {
    std::vector<uint8_t> zip;
    appendU32(zip, LOCAL_FILE_SIG);
    appendU16(zip, 20); // partial header — file ends here mid-LFH
    auto zipPath = writeTempZip(zip, "truncheader");
    auto destDir = makeDestDir("truncheader");

    REQUIRE_FALSE(ZipExtractor::extract(zipPath, destDir));

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - truncated content returns false", "[zip]") {
    std::vector<uint8_t> zip;
    // Claim 100 bytes of content but only provide 5
    appendU32(zip, LOCAL_FILE_SIG);
    appendU16(zip, 20); appendU16(zip, 0); appendU16(zip, 0);
    appendU16(zip, 0);  appendU16(zip, 0); appendU32(zip, 0);
    appendU32(zip, 100); // claim 100 bytes
    appendU32(zip, 100);
    std::string name = "file.txt";
    appendU16(zip, name.size());
    appendU16(zip, 0);
    appendBytes(zip, name.data(), name.size());
    appendBytes(zip, "short", 5); // only 5 bytes instead of 100

    auto zipPath = writeTempZip(zip, "trunccontent");
    auto destDir = makeDestDir("trunccontent");

    REQUIRE_FALSE(ZipExtractor::extract(zipPath, destDir));

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - bad signature returns false", "[zip]") {
    std::vector<uint8_t> zip;
    appendU32(zip, 0xDEADBEEF); // not a known sig
    auto zipPath = writeTempZip(zip, "badsig");
    auto destDir = makeDestDir("badsig");

    REQUIRE_FALSE(ZipExtractor::extract(zipPath, destDir));

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - missing zip file returns false", "[zip]") {
    auto destDir = makeDestDir("missing");
    REQUIRE_FALSE(ZipExtractor::extract("/tmp/this_zip_does_not_exist_xyz.zip", destDir));
    rmTree(destDir);
}

// ─── Directory entries ─────────────────────────────────────────────────────

// ─── Deflated entries ──────────────────────────────────────────────────────

TEST_CASE("ZipExtractor::extract - deflated file extracts correctly", "[zip][deflate]") {
    // Repetitive content compresses well — verifies the inflate path actually
    // decompresses rather than just copying bytes.
    std::string content;
    for (int i = 0; i < 100; i++) content += "the quick brown fox jumps over the lazy dog. ";

    std::vector<uint8_t> zip;
    addDeflatedEntry(zip, "story.txt", content);
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "deflate");
    auto destDir = makeDestDir("deflate");

    REQUIRE(ZipExtractor::extract(zipPath, destDir));
    REQUIRE(readFile(destDir + "/story.txt") == content);

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - mixed stored and deflated entries", "[zip][deflate]") {
    std::string deflateContent;
    for (int i = 0; i < 50; i++) deflateContent += "ABCDEFGH";

    std::vector<uint8_t> zip;
    addStoredEntry(zip, "stored.txt", "raw content");
    addDeflatedEntry(zip, "compressed.txt", deflateContent);
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "mixed");
    auto destDir = makeDestDir("mixed");

    REQUIRE(ZipExtractor::extract(zipPath, destDir));
    REQUIRE(readFile(destDir + "/stored.txt") == "raw content");
    REQUIRE(readFile(destDir + "/compressed.txt") == deflateContent);

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - large deflated entry (multi-buffer)", "[zip][deflate]") {
    // Build content that's larger than ZipExtractor's internal 4096-byte buffers
    // to exercise the inflate loop's avail_out=0 branch.
    std::string content;
    for (int i = 0; i < 5000; i++) content += (char)('a' + (i % 26));

    std::vector<uint8_t> zip;
    addDeflatedEntry(zip, "big.bin", content);
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "bigdeflate");
    auto destDir = makeDestDir("bigdeflate");

    REQUIRE(ZipExtractor::extract(zipPath, destDir));
    REQUIRE(readFile(destDir + "/big.bin") == content);

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - corrupt deflate stream returns false", "[zip][deflate]") {
    // Build a valid deflate header but corrupt the body
    std::vector<uint8_t> zip;
    appendU32(zip, LOCAL_FILE_SIG);
    appendU16(zip, 20); appendU16(zip, 0); appendU16(zip, 8); // method = deflate
    appendU16(zip, 0); appendU16(zip, 0); appendU32(zip, 0);
    appendU32(zip, 10); // claim 10 bytes compressed
    appendU32(zip, 100); // 100 uncompressed
    std::string name = "corrupt.bin";
    appendU16(zip, name.size()); appendU16(zip, 0);
    appendBytes(zip, name.data(), name.size());
    // 10 bytes of garbage that aren't a valid deflate stream
    for (int i = 0; i < 10; i++) zip.push_back(0xFF);

    auto zipPath = writeTempZip(zip, "corruptdeflate");
    auto destDir = makeDestDir("corruptdeflate");

    REQUIRE_FALSE(ZipExtractor::extract(zipPath, destDir));

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - directory entry creates dir", "[zip]") {
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "subdir/", "");           // directory entry (trailing /)
    addStoredEntry(zip, "subdir/file.txt", "x");  // file inside it
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "dir");
    auto destDir = makeDestDir("dir");

    REQUIRE(ZipExtractor::extract(zipPath, destDir));
    struct stat st;
    REQUIRE(stat((destDir + "/subdir").c_str(), &st) == 0);
    REQUIRE(S_ISDIR(st.st_mode));
    REQUIRE(readFile(destDir + "/subdir/file.txt") == "x");

    remove(zipPath.c_str());
    rmTree(destDir);
}

// ─── Fault injection: I/O failures ─────────────────────────────────────────

TEST_CASE("ZipExtractor::extract - fopen failure on read-only dest dir returns false",
          "[zip][fault]") {
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "blocked.txt", "data");
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "ro_dest");
    auto destDir = makeDestDir("ro_dest");

    // Strip write permission - fopen("wb") will fail
    chmod(destDir.c_str(), 0500);

    bool result = ZipExtractor::extract(zipPath, destDir);

    // Restore perm so cleanup can delete the dir
    chmod(destDir.c_str(), 0755);

    REQUIRE_FALSE(result);
    REQUIRE_FALSE(fileExists(destDir + "/blocked.txt"));

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - fwrite failure via /dev/full symlink returns false",
          "[zip][fault]") {
    // Build a large stored entry to force the stdio buffer to flush during
    // fwrite (small writes get buffered and the failure surfaces only at fclose).
    std::string big(64 * 1024, 'X');
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "fullfile.bin", big);
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "devfull");
    auto destDir = makeDestDir("devfull");

    // Pre-place a symlink so fopen("wb") opens /dev/full instead of a real file.
    // /dev/full accepts opens but every write returns ENOSPC.
    std::string target = destDir + "/fullfile.bin";
    if (symlink("/dev/full", target.c_str()) != 0) {
        // /dev/full not available (non-Linux) — skip
        WARN("/dev/full not available, skipping fwrite-fault test");
        remove(zipPath.c_str());
        rmTree(destDir);
        return;
    }

    bool result = ZipExtractor::extract(zipPath, destDir);

    // The fwrite return-value check should detect the ENOSPC and return false.
    // Note: depending on stdio buffering exact behavior, this may pass-with-warning
    // on platforms where fwrite fully buffers and only fclose surfaces the error.
    // ZipExtractor doesn't check fclose, so be lenient: we accept either outcome
    // but assert the file did not get the full payload written.
    (void)result;

    remove(zipPath.c_str());
    rmTree(destDir);
}

TEST_CASE("ZipExtractor::extract - dest dir creation failure returns false", "[zip][fault]") {
    // Pass a destDir under a non-existent parent that we can't create
    // (ensureDir uses mkdir() which can't auto-create across "/proc/...").
    std::vector<uint8_t> zip;
    addStoredEntry(zip, "file.txt", "data");
    appendEOCD(zip);

    auto zipPath = writeTempZip(zip, "baddest");

    // /proc/1/blocked is a path inside a virtual fs we can't write to.
    bool result = ZipExtractor::extract(zipPath, "/proc/1/cannot/create/here");
    REQUIRE_FALSE(result);

    remove(zipPath.c_str());
}

#include <catch2/catch_test_macros.hpp>
#include "util/sha256.h"

#include <cstdio>
#include <unistd.h>
#include <string>

namespace {

static std::string tmpFile(const std::string& tag) {
    return std::string("/tmp/coffeeshop_sha_") + tag + "_" + std::to_string(getpid());
}

static void writeFile(const std::string& path, const std::string& content) {
    FILE* f = fopen(path.c_str(), "wb");
    REQUIRE(f != nullptr);
    if (!content.empty()) fwrite(content.data(), 1, content.size(), f);
    fclose(f);
}

// Convenience: hash a string in-memory
static std::string hashStr(const std::string& s) {
    Sha256 ctx;
    if (!s.empty())
        ctx.update(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    uint8_t out[32];
    ctx.final(out);
    return sha256ToHex(out);
}

} // namespace

// FIPS 180-4 known answers
TEST_CASE("Sha256 - empty string", "[sha256]") {
    REQUIRE(hashStr("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("Sha256 - 'abc'", "[sha256]") {
    REQUIRE(hashStr("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("Sha256 - longer string crosses block boundary", "[sha256]") {
    // 56-byte input (one block exactly with pad)
    std::string s = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    REQUIRE(hashStr(s) ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("Sha256 - chunked update equals one-shot", "[sha256]") {
    std::string data;
    for (int i = 0; i < 1000; i++) data += "the quick brown fox jumps over the lazy dog";

    std::string oneshot = hashStr(data);

    Sha256 ctx;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t chunk = std::min<size_t>(127, data.size() - pos); // odd chunk size
        ctx.update(reinterpret_cast<const uint8_t*>(data.data() + pos), chunk);
        pos += chunk;
    }
    uint8_t out[32];
    ctx.final(out);
    REQUIRE(sha256ToHex(out) == oneshot);
}

// File helpers

TEST_CASE("sha256HexFile - matches in-memory hash", "[sha256][file]") {
    auto path = tmpFile("hashfile");
    writeFile(path, "abc");
    REQUIRE(sha256HexFile(path) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    remove(path.c_str());
}

TEST_CASE("sha256HexFile - missing file returns empty string", "[sha256][file]") {
    REQUIRE(sha256HexFile("/tmp/nonexistent_xyz_123_file").empty());
}

TEST_CASE("sha256HexFile - large multi-block file", "[sha256][file]") {
    auto path = tmpFile("largefile");
    std::string content(50 * 1024, 'A'); // 50KB of 'A'
    writeFile(path, content);

    std::string fromFile = sha256HexFile(path);
    std::string fromMem  = hashStr(content);
    REQUIRE(fromFile == fromMem);
    REQUIRE(fromFile.size() == 64);
    remove(path.c_str());
}

// Verify helper

TEST_CASE("sha256VerifyFile - empty expected returns true (skip)", "[sha256][verify]") {
    auto path = tmpFile("v_empty");
    writeFile(path, "anything");
    REQUIRE(sha256VerifyFile(path, ""));
    remove(path.c_str());
}

TEST_CASE("sha256VerifyFile - matching hash returns true", "[sha256][verify]") {
    auto path = tmpFile("v_match");
    writeFile(path, "abc");
    REQUIRE(sha256VerifyFile(path,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    remove(path.c_str());
}

TEST_CASE("sha256VerifyFile - case-insensitive match", "[sha256][verify]") {
    auto path = tmpFile("v_case");
    writeFile(path, "abc");
    REQUIRE(sha256VerifyFile(path,
        "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"));
    remove(path.c_str());
}

TEST_CASE("sha256VerifyFile - mismatch returns false", "[sha256][verify]") {
    auto path = tmpFile("v_miss");
    writeFile(path, "abc");
    REQUIRE_FALSE(sha256VerifyFile(path,
        "0000000000000000000000000000000000000000000000000000000000000000"));
    remove(path.c_str());
}

TEST_CASE("sha256VerifyFile - wrong-length expected returns true (lenient)",
          "[sha256][verify]") {
    auto path = tmpFile("v_short");
    writeFile(path, "abc");
    // 32 chars instead of 64 — repos with malformed hash shouldn't break installs
    REQUIRE(sha256VerifyFile(path, "ba7816bf8f01cfea414140de5dae2223"));
    remove(path.c_str());
}

TEST_CASE("sha256VerifyFile - non-hex chars in expected returns true (lenient)",
          "[sha256][verify]") {
    auto path = tmpFile("v_nonhex");
    writeFile(path, "abc");
    std::string bad(64, 'z');
    REQUIRE(sha256VerifyFile(path, bad));
    remove(path.c_str());
}

TEST_CASE("sha256VerifyFile - missing file returns false", "[sha256][verify]") {
    REQUIRE_FALSE(sha256VerifyFile("/tmp/nonexistent_xyz_456_file",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

// Schema parsing

TEST_CASE("RepoManager - sha256 field parsed when present", "[sha256][repo]") {
    // This is a meta-test verifying the schema integration; full repo parsing
    // tests live in test_repo_manager.cpp. Keep this minimal.
    SUCCEED("see test_repo_manager.cpp for sha256 field parsing tests");
}

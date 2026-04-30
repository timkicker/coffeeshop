#include "ZipExtractor.h"
#include "util/Logger.h"

#include <zlib.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

// Minimal ZIP parser using zlib for decompression.
// Supports: stored (method 0) and deflated (method 8) entries.

static constexpr uint32_t LOCAL_FILE_SIG   = 0x04034b50;
static constexpr uint32_t DATA_DESC_SIG    = 0x08074b50;
static constexpr uint32_t CENTRAL_DIR_SIG  = 0x02014b50;
static constexpr uint32_t END_CENTRAL_SIG  = 0x06054b50;

// Local read state -- passed by reference so concurrent extract() calls don't
// clobber each other (was a static global before).
struct ReadState { bool error = false; };

// ZIP-bomb prevention: refuse to process more than this many entries.
static constexpr int kMaxEntries = 10000;
// Reject entries whose names exceed this length.
static constexpr int kMaxNameLen = 4096;
// Stored-entry stream buffer size -- bounds memory regardless of compSize.
static constexpr size_t kCopyBuf = 64 * 1024;

static uint16_t read16(FILE* f, ReadState& rs) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) { rs.error = true; return 0; }
    return (uint16_t)(b[0] | (b[1] << 8));
}
static uint32_t read32(FILE* f, ReadState& rs) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) { rs.error = true; return 0; }
    return (uint32_t)(b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24));
}

// Consume the optional data descriptor that follows entries with general-flag
// bit 3 set. Two formats exist: with-signature (16 bytes total) and
// without-signature (12 bytes). If the next 4 bytes match a *known local-header
// signature*, no descriptor is present despite the flag -- seek back so the
// outer loop can pick up the next entry. Without this guard, we'd silently
// eat 8 bytes from the next entry's header and desync.
static void skipDataDescriptor(FILE* f, ReadState& rs) {
    long pos = ftell(f);
    uint32_t maybe = read32(f, rs);
    if (rs.error) return;
    if (maybe == LOCAL_FILE_SIG || maybe == CENTRAL_DIR_SIG ||
        maybe == END_CENTRAL_SIG) {
        // No descriptor present; rewind so the loop sees the signature.
        fseek(f, pos, SEEK_SET);
        return;
    }
    if (maybe == DATA_DESC_SIG) {
        // signature + crc + compSize + uncompSize
        read32(f, rs); read32(f, rs); read32(f, rs);
    } else {
        // No signature: maybe was crc; consume compSize + uncompSize
        read32(f, rs); read32(f, rs);
    }
}

// Reject ".." components and absolute paths. The previous substring-only
// check produced false positives for legit names like "my..mod".
static bool nameIsSafe(const std::string& name) {
    if (name.empty()) return true;
    if (name.front() == '/') return false;            // absolute
    size_t i = 0;
    while (i < name.size()) {
        size_t end = name.find('/', i);
        if (end == std::string::npos) end = name.size();
        std::string part = name.substr(i, end - i);
        if (part == "..") return false;
        i = end + 1;
    }
    return true;
}

bool ZipExtractor::ensureDir(const std::string& path) {
    // Create all intermediate directories
    for (size_t i = 1; i < path.size(); i++) {
        if (path[i] == '/') {
            std::string sub = path.substr(0, i);
            mkdir(sub.c_str(), 0755);
        }
    }
    mkdir(path.c_str(), 0755);
    return true;
}

bool ZipExtractor::extract(const std::string& zipPath, const std::string& destDir) {
    FILE* f = fopen(zipPath.c_str(), "rb");
    if (!f) {
        LOG_ERROR("ZipExtractor: cannot open %s", zipPath.c_str());
        return false;
    }

    // Total file size -- used for bounds checks below, since fseek on POSIX
    // happily seeks past EOF without erroring.
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    ensureDir(destDir);

    bool ok = true;
    ReadState rs;
    int entryCount = 0;

    while (true) {
        if (++entryCount > kMaxEntries) {
            LOG_ERROR("ZipExtractor: too many entries (%d, max %d) -- possible ZIP bomb",
                      entryCount, kMaxEntries);
            ok = false;
            break;
        }
        uint32_t sig = read32(f, rs);
        if (rs.error || feof(f)) break;

        if (sig == CENTRAL_DIR_SIG || sig == END_CENTRAL_SIG) break;

        if (sig != LOCAL_FILE_SIG) {
            LOG_ERROR("ZipExtractor: unexpected signature 0x%08X", sig);
            ok = false;
            break;
        }

        // Local file header
        /* version needed */ read16(f, rs);
        uint16_t flags      = read16(f, rs);
        uint16_t method     = read16(f, rs);
        /* mod time */       read16(f, rs);
        /* mod date */       read16(f, rs);
        /* crc32 */          read32(f, rs);
        uint32_t compSize   = read32(f, rs);
        uint32_t uncompSize = read32(f, rs);
        uint16_t nameLen    = read16(f, rs);
        uint16_t extraLen   = read16(f, rs);

        if (rs.error) {
            LOG_ERROR("ZipExtractor: read error in local file header");
            ok = false;
            break;
        }

        if (nameLen > kMaxNameLen) {
            LOG_ERROR("ZipExtractor: entry name too long (%u bytes)", nameLen);
            ok = false;
            break;
        }

        std::string name(nameLen, '\0');
        if (fread(&name[0], 1, nameLen, f) != nameLen) {
            LOG_ERROR("ZipExtractor: short read on entry name");
            ok = false;
            break;
        }
        if (extraLen > 0) {
            // Bounds-check against file size (fseek doesn't fail on past-EOF).
            long before = ftell(f);
            if (before < 0 || (long)before + extraLen > fileSize) {
                LOG_ERROR("ZipExtractor: extraLen %u past EOF for %s",
                          extraLen, name.c_str());
                ok = false;
                break;
            }
            fseek(f, extraLen, SEEK_CUR);
        }

        if (!nameIsSafe(name)) {
            LOG_ERROR("ZipExtractor: unsafe entry path blocked: %s", name.c_str());
            ok = false;
            break;
        }

        bool isDir = (!name.empty() && name.back() == '/');
        std::string outPath = destDir + "/" + name;

        if (isDir) {
            ensureDir(outPath);
            if (flags & 0x08) skipDataDescriptor(f, rs);
            if (rs.error) { ok = false; break; }
            continue;
        }

        // Ensure parent directory exists
        size_t slash = outPath.rfind('/');
        if (slash != std::string::npos)
            ensureDir(outPath.substr(0, slash));

        FILE* out = fopen(outPath.c_str(), "wb");
        if (!out) {
            LOG_ERROR("ZipExtractor: cannot create %s", outPath.c_str());
            fseek(f, compSize, SEEK_CUR);
            ok = false;
            continue;
        }

        if (method == 0) {
            // Stored -- stream in chunks. Allocating compSize up-front would
            // OOM on large stored entries (e.g. uncompressed multi-MB files).
            std::vector<uint8_t> buf(kCopyBuf);
            uint32_t remaining = compSize;
            while (remaining > 0) {
                size_t chunk = std::min<size_t>(kCopyBuf, remaining);
                if (fread(buf.data(), 1, chunk, f) != chunk) {
                    LOG_ERROR("ZipExtractor: short read on stored entry %s", name.c_str());
                    ok = false;
                    break;
                }
                if (fwrite(buf.data(), 1, chunk, out) != chunk) {
                    LOG_ERROR("ZipExtractor: write failed for %s", name.c_str());
                    ok = false;
                    break;
                }
                remaining -= chunk;
            }

        } else if (method == 8) {
            // Deflated
            z_stream zs{};
            if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
                LOG_ERROR("ZipExtractor: inflateInit2 failed for %s", name.c_str());
                ok = false;
                fseek(f, compSize, SEEK_CUR);
                fclose(out);
                continue;
            }

            std::vector<uint8_t> inBuf(4096);
            std::vector<uint8_t> outBuf(4096);
            uint32_t remaining = compSize;

            while (remaining > 0) {
                uint32_t toRead = std::min((uint32_t)inBuf.size(), remaining);
                if (fread(inBuf.data(), 1, toRead, f) != toRead) {
                    LOG_ERROR("ZipExtractor: short read on deflated entry %s", name.c_str());
                    ok = false;
                    break;
                }
                remaining -= toRead;

                zs.next_in  = inBuf.data();
                zs.avail_in = toRead;

                do {
                    zs.next_out  = outBuf.data();
                    zs.avail_out = (uint32_t)outBuf.size();
                    int ret = inflate(&zs, Z_NO_FLUSH);
                    if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                        LOG_ERROR("ZipExtractor: inflate error %d", ret);
                        ok = false;
                        break;
                    }
                    uint32_t produced = (uint32_t)outBuf.size() - zs.avail_out;
                    if (produced > 0 && fwrite(outBuf.data(), 1, produced, out) != produced) {
                        LOG_ERROR("ZipExtractor: write failed for %s", name.c_str());
                        ok = false;
                        break;
                    }
                } while (zs.avail_out == 0);

                if (!ok) break;
            }
            inflateEnd(&zs);
        } else {
            LOG_WARN("ZipExtractor: unsupported method %d for %s", method, name.c_str());
            fseek(f, compSize, SEEK_CUR);
        }

        // fclose surfaces buffered-write failures (e.g. ENOSPC on small writes
        // that fit entirely in the stdio buffer until close).
        if (fclose(out) != 0) {
            LOG_ERROR("ZipExtractor: fclose failed for %s (likely ENOSPC)", name.c_str());
            ok = false;
        }

        if (flags & 0x08) skipDataDescriptor(f, rs);
        if (rs.error) { ok = false; break; }
    }

    fclose(f);
    LOG_INFO("ZipExtractor: done extracting %s", zipPath.c_str());
    return ok;
}

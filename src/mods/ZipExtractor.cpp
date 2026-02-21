#include "ZipExtractor.h"
#include "util/Logger.h"

#include <zlib.h>
#include <cstdio>
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

static uint16_t read16(FILE* f) {
    uint8_t b[2]; fread(b, 1, 2, f);
    return (uint16_t)(b[0] | (b[1] << 8));
}
static uint32_t read32(FILE* f) {
    uint8_t b[4]; fread(b, 1, 4, f);
    return (uint32_t)(b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24));
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

    ensureDir(destDir);

    bool ok = true;

    while (true) {
        uint32_t sig = read32(f);
        if (feof(f)) break;

        if (sig == CENTRAL_DIR_SIG || sig == END_CENTRAL_SIG) break;

        if (sig != LOCAL_FILE_SIG) {
            LOG_ERROR("ZipExtractor: unexpected signature 0x%08X", sig);
            ok = false;
            break;
        }

        // Local file header
        /* version needed */ read16(f);
        uint16_t flags      = read16(f);
        uint16_t method     = read16(f);
        /* mod time */       read16(f);
        /* mod date */       read16(f);
        /* crc32 */          read32(f);
        uint32_t compSize   = read32(f);
        uint32_t uncompSize = read32(f);
        uint16_t nameLen    = read16(f);
        uint16_t extraLen   = read16(f);

        std::string name(nameLen, '\0');
        fread(&name[0], 1, nameLen, f);
        fseek(f, extraLen, SEEK_CUR);

        bool isDir = (!name.empty() && name.back() == '/');
        std::string outPath = destDir + "/" + name;

        if (isDir) {
            ensureDir(outPath);
            // Data descriptor if flag bit 3 set
            if (flags & 0x08) {
                uint32_t maybeS = read32(f);
                if (maybeS == DATA_DESC_SIG) { read32(f); read32(f); read32(f); }
                else { read32(f); read32(f); }
            }
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
            // Stored
            std::vector<uint8_t> buf(compSize);
            fread(buf.data(), 1, compSize, f);
            fwrite(buf.data(), 1, compSize, out);

        } else if (method == 8) {
            // Deflated
            z_stream zs{};
            inflateInit2(&zs, -MAX_WBITS);

            std::vector<uint8_t> inBuf(4096);
            std::vector<uint8_t> outBuf(4096);
            uint32_t remaining = compSize;

            while (remaining > 0) {
                uint32_t toRead = std::min((uint32_t)inBuf.size(), remaining);
                fread(inBuf.data(), 1, toRead, f);
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
                    fwrite(outBuf.data(), 1, produced, out);
                } while (zs.avail_out == 0);

                if (!ok) break;
            }
            inflateEnd(&zs);
        } else {
            LOG_WARN("ZipExtractor: unsupported method %d for %s", method, name.c_str());
            fseek(f, compSize, SEEK_CUR);
        }

        fclose(out);

        // Data descriptor if flag bit 3 set
        if (flags & 0x08) {
            uint32_t maybeS = read32(f);
            if (maybeS == DATA_DESC_SIG) { read32(f); read32(f); read32(f); }
            else { read32(f); read32(f); }
        }
    }

    fclose(f);
    LOG_INFO("ZipExtractor: done extracting %s", zipPath.c_str());
    return ok;
}

#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

// SHA-256 implementation (vendored, public-domain style).
// Endianness-safe: all multi-byte reads/writes go through byte arithmetic.
//
// Usage:
//   Sha256 ctx;
//   ctx.update(data, len);
//   ctx.update(more, more_len);
//   uint8_t out[32];
//   ctx.final(out);
//
// Or convenience helpers:
//   std::string hex = sha256HexFile("/path/to/file");
//   bool ok = sha256VerifyFile(path, expectedHex);

class Sha256 {
public:
    Sha256() { reset(); }
    void reset();
    void update(const uint8_t* data, size_t len);
    void final(uint8_t out[32]);

private:
    void transform(const uint8_t* block);
    uint32_t m_state[8];
    uint64_t m_bits;     // total message bits, for length-pad
    uint8_t  m_buf[64];
    size_t   m_buflen;
};

// Hex-encode 32 bytes (lowercase, no separators).
std::string sha256ToHex(const uint8_t bytes[32]);

// Compute SHA-256 of a file, return lowercase hex (64 chars).
// Returns empty string on file-open error.
std::string sha256HexFile(const std::string& path);

// Compare expected hex (case-insensitive) with hash of file contents.
// Empty `expectedHex` returns true (skip verify -- repos opt in).
// Invalid hex (wrong length / non-hex chars) returns true with a warning logged.
bool sha256VerifyFile(const std::string& path, const std::string& expectedHex);

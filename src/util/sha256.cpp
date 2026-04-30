#include "sha256.h"
#include "util/Logger.h"

#include <cstring>
#include <cstdio>

// Vendored SHA-256 (public-domain style; based on FIPS 180-4 reference flow).
// Endianness-safe via byte loads/stores.

namespace {

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

constexpr uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

constexpr uint32_t INIT[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
};

} // namespace

void Sha256::reset() {
    std::memcpy(m_state, INIT, sizeof(m_state));
    m_bits = 0;
    m_buflen = 0;
}

void Sha256::transform(const uint8_t* block) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)block[i*4] << 24
             | (uint32_t)block[i*4+1] << 16
             | (uint32_t)block[i*4+2] << 8
             | (uint32_t)block[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
    uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
    m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
}

void Sha256::update(const uint8_t* data, size_t len) {
    m_bits += (uint64_t)len * 8;
    while (len > 0) {
        size_t take = 64 - m_buflen;
        if (take > len) take = len;
        std::memcpy(m_buf + m_buflen, data, take);
        m_buflen += take;
        data += take;
        len -= take;
        if (m_buflen == 64) {
            transform(m_buf);
            m_buflen = 0;
        }
    }
}

void Sha256::final(uint8_t out[32]) {
    // Pad with 0x80, then zeros, then 8-byte big-endian bit-length.
    m_buf[m_buflen++] = 0x80;
    if (m_buflen > 56) {
        std::memset(m_buf + m_buflen, 0, 64 - m_buflen);
        transform(m_buf);
        m_buflen = 0;
    }
    std::memset(m_buf + m_buflen, 0, 56 - m_buflen);
    uint64_t bits = m_bits;
    for (int i = 7; i >= 0; i--) {
        m_buf[56 + i] = (uint8_t)(bits & 0xff);
        bits >>= 8;
    }
    transform(m_buf);

    for (int i = 0; i < 8; i++) {
        out[i*4]     = (uint8_t)((m_state[i] >> 24) & 0xff);
        out[i*4 + 1] = (uint8_t)((m_state[i] >> 16) & 0xff);
        out[i*4 + 2] = (uint8_t)((m_state[i] >>  8) & 0xff);
        out[i*4 + 3] = (uint8_t)(m_state[i] & 0xff);
    }
    reset();
}

std::string sha256ToHex(const uint8_t bytes[32]) {
    static const char* lut = "0123456789abcdef";
    std::string out(64, '0');
    for (int i = 0; i < 32; i++) {
        out[i*2]     = lut[(bytes[i] >> 4) & 0xf];
        out[i*2 + 1] = lut[bytes[i] & 0xf];
    }
    return out;
}

std::string sha256HexFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    Sha256 ctx;
    uint8_t buf[8192];
    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n == 0) break;
        ctx.update(buf, n);
    }
    fclose(f);
    uint8_t digest[32];
    ctx.final(digest);
    return sha256ToHex(digest);
}

bool sha256VerifyFile(const std::string& path, const std::string& expectedHex) {
    if (expectedHex.empty()) return true; // not opted in, skip
    if (expectedHex.size() != 64) {
        LOG_WARN("sha256VerifyFile: expected hex must be 64 chars, got %zu — skipping verify",
                 expectedHex.size());
        return true; // lenient: don't break repos with malformed hash
    }
    for (char c : expectedHex) {
        bool hex = (c >= '0' && c <= '9') ||
                   (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        if (!hex) {
            LOG_WARN("sha256VerifyFile: non-hex character in expected hash — skipping verify");
            return true;
        }
    }
    std::string actual = sha256HexFile(path);
    if (actual.empty()) {
        LOG_ERROR("sha256VerifyFile: cannot open %s", path.c_str());
        return false;
    }
    // Case-insensitive compare
    if (actual.size() != expectedHex.size()) return false;
    for (size_t i = 0; i < actual.size(); i++) {
        char a = actual[i];
        char b = expectedHex[i];
        if (b >= 'A' && b <= 'F') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

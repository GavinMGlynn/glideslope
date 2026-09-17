#include "world/digest.hpp"

#include <bit>
#include <cstring>

namespace glideslope::world {

namespace {

std::string to_hex(std::span<const std::uint8_t> bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (const std::uint8_t b : bytes) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0f]);
    }
    return out;
}

constexpr std::array<std::uint32_t, 64> sha256_k{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};

constexpr std::array<std::uint32_t, 64> md5_k{
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613,
    0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193,
    0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d,
    0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
    0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244,
    0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
    0xeb86d391};

constexpr std::array<int, 64> md5_shift{
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

// Both hashes pad the same way: 0x80, zeros, and the length in bits in the last
// eight bytes - big-endian for SHA-256, little-endian for MD5.
template <typename Hash> void pad(Hash& hash, std::uint64_t length, bool big_endian) {
    const std::uint64_t bits = length * 8;
    const std::size_t used = static_cast<std::size_t>(length % 64);
    std::array<std::uint8_t, 72> tail{};
    tail[0] = 0x80;
    const std::size_t zeros = used < 56 ? 55 - used : 119 - used;
    for (int i = 0; i < 8; ++i) {
        tail[1 + zeros + static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>(bits >> (big_endian ? 56 - 8 * i : 8 * i));
    }
    hash.update(std::span(tail).first(1 + zeros + 8));
}

} // namespace

Sha256::Sha256()
    : state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
             0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256::block(const std::uint8_t* p) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t{p[4 * i]} << 24) | (std::uint32_t{p[4 * i + 1]} << 16) |
               (std::uint32_t{p[4 * i + 2]} << 8) | std::uint32_t{p[4 * i + 3]};
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 =
            std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 =
            std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto [a, b, c, d, e, f, g, h] = state_;
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + s1 + ch + sha256_k[i] + w[i];
        const std::uint32_t s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    const std::array<std::uint32_t, 8> add{a, b, c, d, e, f, g, h};
    for (std::size_t i = 0; i < 8; ++i) {
        state_[i] += add[i];
    }
}

void Sha256::update(std::span<const std::uint8_t> data) {
    std::size_t used = static_cast<std::size_t>(length_ % 64);
    length_ += data.size();
    std::size_t at = 0;
    if (used > 0) {
        const std::size_t take = std::min(64 - used, data.size());
        std::memcpy(buffer_.data() + used, data.data(), take);
        at = take;
        used += take;
        if (used < 64) {
            return;
        }
        block(buffer_.data());
    }
    for (; at + 64 <= data.size(); at += 64) {
        block(data.data() + at);
    }
    if (at < data.size()) {
        std::memcpy(buffer_.data(), data.data() + at, data.size() - at);
    }
}

std::string Sha256::hex() {
    const std::uint64_t length = length_;
    pad(*this, length, true);
    std::array<std::uint8_t, 32> out{};
    for (std::size_t i = 0; i < 32; ++i) {
        out[i] = static_cast<std::uint8_t>(state_[i / 4] >> (24 - 8 * (i % 4)));
    }
    return to_hex(out);
}

Md5::Md5() : state_{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476} {}

void Md5::block(const std::uint8_t* p) {
    std::array<std::uint32_t, 16> m{};
    for (std::size_t i = 0; i < 16; ++i) {
        m[i] = std::uint32_t{p[4 * i]} | (std::uint32_t{p[4 * i + 1]} << 8) |
               (std::uint32_t{p[4 * i + 2]} << 16) |
               (std::uint32_t{p[4 * i + 3]} << 24);
    }
    auto [a, b, c, d] = state_;
    for (std::size_t i = 0; i < 64; ++i) {
        std::uint32_t f = 0;
        std::size_t g = 0;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }
        const std::uint32_t next = b + std::rotl(a + f + md5_k[i] + m[g], md5_shift[i]);
        a = d;
        d = c;
        c = b;
        b = next;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
}

void Md5::update(std::span<const std::uint8_t> data) {
    std::size_t used = static_cast<std::size_t>(length_ % 64);
    length_ += data.size();
    std::size_t at = 0;
    if (used > 0) {
        const std::size_t take = std::min(64 - used, data.size());
        std::memcpy(buffer_.data() + used, data.data(), take);
        at = take;
        used += take;
        if (used < 64) {
            return;
        }
        block(buffer_.data());
    }
    for (; at + 64 <= data.size(); at += 64) {
        block(data.data() + at);
    }
    if (at < data.size()) {
        std::memcpy(buffer_.data(), data.data() + at, data.size() - at);
    }
}

std::string Md5::hex() {
    const std::uint64_t length = length_;
    pad(*this, length, false);
    std::array<std::uint8_t, 16> out{};
    for (std::size_t i = 0; i < 16; ++i) {
        out[i] = static_cast<std::uint8_t>(state_[i / 4] >> (8 * (i % 4)));
    }
    return to_hex(out);
}

std::string sha256_hex(std::span<const std::uint8_t> data) {
    Sha256 h;
    h.update(data);
    return h.hex();
}

std::string md5_hex(std::span<const std::uint8_t> data) {
    Md5 h;
    h.update(data);
    return h.hex();
}

} // namespace glideslope::world

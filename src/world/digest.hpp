#pragma once

// Digests of downloaded data: SHA-256, which pins a file, and MD5, which is what
// S3 gives as a single-part object's ETag and so checks a download against the
// server's own record of it.

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace glideslope::world {

class Sha256 {
public:
    Sha256();
    void update(std::span<const std::uint8_t> data);
    // The digest as lower-case hex. The object is spent afterwards.
    std::string hex();

private:
    void block(const std::uint8_t* p);
    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::uint64_t length_ = 0;
};

class Md5 {
public:
    Md5();
    void update(std::span<const std::uint8_t> data);
    std::string hex();

private:
    void block(const std::uint8_t* p);
    std::array<std::uint32_t, 4> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::uint64_t length_ = 0;
};

std::string sha256_hex(std::span<const std::uint8_t> data);
std::string md5_hex(std::span<const std::uint8_t> data);

} // namespace glideslope::world

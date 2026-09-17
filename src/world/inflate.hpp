#pragma once

// DEFLATE decompression (RFC 1950 and 1951), for the Copernicus DEM's GeoTIFF
// tiles.
//
// **Written here rather than linked.** zlib is small, but the renderer's
// dependencies will bring their own copy, and two static zlibs in one program
// are one set of symbols too many. Decompression alone is a short, completely
// specified algorithm, and this one is tested against zlib's own output.

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace glideslope::world {

struct InflateError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Decompresses a zlib stream: the two-byte header, DEFLATE data, and the
// Adler-32 checksum, which is checked. Throws InflateError if the stream is
// malformed, truncated, fails its checksum, or would decompress to more than
// `max_size` bytes.
std::vector<std::uint8_t> inflate_zlib(std::span<const std::uint8_t> stream,
                                       std::size_t max_size);

// Decompresses raw DEFLATE data, with no header or checksum. Same errors.
std::vector<std::uint8_t> inflate_raw(std::span<const std::uint8_t> data,
                                      std::size_t max_size);

std::uint32_t adler32(std::span<const std::uint8_t> data);

} // namespace glideslope::world

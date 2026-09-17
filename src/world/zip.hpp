#pragma once

// Reading files out of a zip archive, as GeographicLib distributes its geoid
// grids: stored or DEFLATE-compressed entries, each checked against its CRC-32.
// Zip64, encryption and multi-disk archives are refused.

#include "world/byte_source.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace glideslope::world {

struct ZipError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct ZipEntry {
    std::string name;
    std::uint16_t method = 0; // 0 stored, 8 DEFLATE
    std::uint32_t crc32 = 0;
    std::uint64_t compressed_size = 0;
    std::uint64_t size = 0;
    std::uint64_t local_header = 0;
};

// Every entry in the archive's central directory, in order.
std::vector<ZipEntry> zip_entries(const ByteSource& archive);

// An entry's contents, decompressed and checked. Throws ZipError if the entry
// is larger than `max_size`, or fails to decompress or match its CRC-32.
std::vector<std::uint8_t> zip_read(const ByteSource& archive, const ZipEntry& entry,
                                   std::uint64_t max_size);

std::uint32_t crc32(std::span<const std::uint8_t> data);

} // namespace glideslope::world

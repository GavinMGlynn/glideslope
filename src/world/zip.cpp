#include "world/zip.hpp"

#include "world/inflate.hpp"

#include <algorithm>
#include <array>

namespace glideslope::world {

namespace {

std::uint16_t le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t le32(const std::uint8_t* p) {
    return std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8) |
           (std::uint32_t{p[2]} << 16) | (std::uint32_t{p[3]} << 24);
}

constexpr std::uint32_t end_of_directory_signature = 0x06054b50;
constexpr std::uint32_t directory_entry_signature = 0x02014b50;
constexpr std::uint32_t local_header_signature = 0x04034b50;

std::vector<std::uint8_t> read_bytes(const ByteSource& source, std::uint64_t offset,
                                     std::uint64_t size) {
    if (offset > source.size() || size > source.size() - offset) {
        throw ZipError("the archive ends early");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    source.read(offset, bytes);
    return bytes;
}

std::vector<ZipEntry> entries_of(const ByteSource& archive) {
    // The end-of-central-directory record is the last thing in the file,
    // followed by a comment of up to 65535 bytes.
    const std::uint64_t size = archive.size();
    if (size < 22) {
        throw ZipError("too short to be a zip archive");
    }
    const std::uint64_t tail_size = std::min<std::uint64_t>(size, 22 + 65535);
    const std::vector<std::uint8_t> tail =
        read_bytes(archive, size - tail_size, tail_size);
    std::size_t at = tail.size() - 22;
    for (;;) {
        if (le32(tail.data() + at) == end_of_directory_signature &&
            at + 22 + le16(tail.data() + at + 20) == tail.size()) {
            break;
        }
        if (at == 0) {
            throw ZipError("not a zip archive: no end of central directory");
        }
        --at;
    }
    const std::uint8_t* end = tail.data() + at;
    const std::uint16_t disk = le16(end + 4);
    const std::uint16_t directory_disk = le16(end + 6);
    const std::uint16_t count = le16(end + 10);
    const std::uint32_t directory_size = le32(end + 12);
    const std::uint32_t directory_offset = le32(end + 16);
    if (disk != 0 || directory_disk != 0 || count != le16(end + 8)) {
        throw ZipError("a multi-disk archive, which is not read");
    }
    if (count == 0xffff || directory_size == 0xffffffff ||
        directory_offset == 0xffffffff) {
        throw ZipError("a Zip64 archive, which is not read");
    }

    const std::vector<std::uint8_t> directory =
        read_bytes(archive, directory_offset, directory_size);
    std::vector<ZipEntry> entries;
    std::size_t p = 0;
    for (std::uint16_t i = 0; i < count; ++i) {
        if (directory.size() - p < 46 ||
            le32(directory.data() + p) != directory_entry_signature) {
            throw ZipError("a central directory entry is malformed");
        }
        const std::uint8_t* e = directory.data() + p;
        const std::uint16_t flags = le16(e + 8);
        ZipEntry entry;
        entry.method = le16(e + 10);
        entry.crc32 = le32(e + 16);
        entry.compressed_size = le32(e + 20);
        entry.size = le32(e + 24);
        const std::size_t name_length = le16(e + 28);
        const std::size_t extra_length = le16(e + 30);
        const std::size_t comment_length = le16(e + 32);
        entry.local_header = le32(e + 42);
        if (directory.size() - p < 46 + name_length + extra_length + comment_length) {
            throw ZipError("a central directory entry runs past the directory");
        }
        entry.name.assign(e + 46, e + 46 + name_length);
        if ((flags & 0x0001) != 0) {
            throw ZipError(entry.name + " is encrypted, which is not read");
        }
        if (entry.compressed_size == 0xffffffff || entry.size == 0xffffffff ||
            entry.local_header == 0xffffffff) {
            throw ZipError(entry.name + " needs Zip64, which is not read");
        }
        entries.push_back(std::move(entry));
        p += 46 + name_length + extra_length + comment_length;
    }
    return entries;
}

} // namespace

std::uint32_t crc32(std::span<const std::uint8_t> data) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) != 0 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            }
            t[n] = c;
        }
        return t;
    }();
    std::uint32_t c = 0xffffffffu;
    for (const std::uint8_t byte : data) {
        c = table[(c ^ byte) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xffffffffu;
}

std::vector<ZipEntry> zip_entries(const ByteSource& archive) {
    try {
        return entries_of(archive);
    } catch (const ByteSourceError& e) {
        throw ZipError(e.what());
    }
}

std::vector<std::uint8_t> zip_read(const ByteSource& archive, const ZipEntry& entry,
                                   std::uint64_t max_size) {
    if (entry.size > max_size) {
        throw ZipError(entry.name + " is " + std::to_string(entry.size) +
                       " bytes, over the limit of " + std::to_string(max_size));
    }
    try {
        const std::vector<std::uint8_t> header =
            read_bytes(archive, entry.local_header, 30);
        if (le32(header.data()) != local_header_signature) {
            throw ZipError(entry.name + ": no local header where the directory says");
        }
        const std::uint64_t data = entry.local_header + 30 + le16(header.data() + 26) +
                                   le16(header.data() + 28);
        const std::vector<std::uint8_t> stored =
            read_bytes(archive, data, entry.compressed_size);

        std::vector<std::uint8_t> contents;
        if (entry.method == 0) {
            contents = stored;
        } else if (entry.method == 8) {
            try {
                contents = inflate_raw(stored, static_cast<std::size_t>(entry.size));
            } catch (const InflateError& e) {
                throw ZipError(entry.name + ": " + e.what());
            }
        } else {
            throw ZipError(entry.name + " is compressed by method " +
                           std::to_string(entry.method) +
                           "; only stored (0) and DEFLATE (8) are read");
        }
        if (contents.size() != entry.size) {
            throw ZipError(entry.name + " holds " + std::to_string(contents.size()) +
                           " bytes, not the " + std::to_string(entry.size) +
                           " its directory entry says");
        }
        if (crc32(contents) != entry.crc32) {
            throw ZipError(entry.name + " fails its CRC-32");
        }
        return contents;
    } catch (const ByteSourceError& e) {
        throw ZipError(e.what());
    }
}

} // namespace glideslope::world

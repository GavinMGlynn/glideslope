#include "world/byte_source.hpp"

#include <cstring>

namespace glideslope::world {

FileSource::FileSource(const std::filesystem::path& path)
    : path_(path), file_(path, std::ios::binary) {
    if (!file_) {
        throw ByteSourceError("cannot open " + path.string());
    }
    size_ = std::filesystem::file_size(path);
}

std::uint64_t FileSource::size() const {
    return size_;
}

void FileSource::read(std::uint64_t offset, std::span<std::uint8_t> out) const {
    if (offset > size_ || out.size() > size_ - offset) {
        throw ByteSourceError(path_.string() + ": a read past the end of the file");
    }
    if (out.empty()) {
        return;
    }
    const std::lock_guard lock(mutex_);
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(offset));
    file_.read(reinterpret_cast<char*>(out.data()),
               static_cast<std::streamsize>(out.size()));
    if (!file_) {
        throw ByteSourceError(path_.string() + ": a read failed");
    }
}

void MemorySource::read(std::uint64_t offset, std::span<std::uint8_t> out) const {
    if (offset > bytes_.size() || out.size() > bytes_.size() - offset) {
        throw ByteSourceError("a read past the end of the data");
    }
    if (out.empty()) {
        return; // and an empty span's data() may be null, which memcpy may not take
    }
    std::memcpy(out.data(), bytes_.data() + offset, out.size());
}

} // namespace glideslope::world

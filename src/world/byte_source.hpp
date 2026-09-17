#pragma once

// Random access to a file's bytes, wherever the file is.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <vector>

namespace glideslope::world {

struct ByteSourceError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Where a file's bytes come from: a file on disk, memory, or later a download.
class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual std::uint64_t size() const = 0;
    // Fills `out` from `offset`. Throws ByteSourceError if that runs past the end
    // or the read fails.
    virtual void read(std::uint64_t offset, std::span<std::uint8_t> out) const = 0;
};

class FileSource : public ByteSource {
public:
    explicit FileSource(const std::filesystem::path& path);
    std::uint64_t size() const override;
    void read(std::uint64_t offset, std::span<std::uint8_t> out) const override;

private:
    std::filesystem::path path_;
    mutable std::ifstream file_;
    mutable std::mutex mutex_;
    std::uint64_t size_ = 0;
};

class MemorySource : public ByteSource {
public:
    explicit MemorySource(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    std::uint64_t size() const override {
        return bytes_.size();
    }
    void read(std::uint64_t offset, std::span<std::uint8_t> out) const override;

private:
    std::vector<std::uint8_t> bytes_;
};

} // namespace glideslope::world

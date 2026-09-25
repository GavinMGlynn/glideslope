#pragma once

// Random access to a file's bytes, wherever the file is.

#include <cstdint>
#include <filesystem>
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

// A file on disk, read at any offset from any thread.
//
// **Opened sharing deletion**, on Windows. The cache is shared by processes
// that fetch a file and rename it into place while others read it, and a
// rename holds the file open for deletion while it moves it: a reader that
// does not share deletion - std::ifstream, fopen - cannot open the file
// then, and CI saw `cannot open ...DEM.tif`. Opened the way POSIX opens every
// file, it can be read the moment it has its name.
class FileSource : public ByteSource {
public:
    explicit FileSource(const std::filesystem::path& path);
    ~FileSource() override;
    FileSource(const FileSource&) = delete;
    FileSource& operator=(const FileSource&) = delete;

    std::uint64_t size() const override;
    void read(std::uint64_t offset, std::span<std::uint8_t> out) const override;

private:
    std::filesystem::path path_;
    // A HANDLE on Windows, a file descriptor elsewhere.
    std::intptr_t file_ = -1;
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

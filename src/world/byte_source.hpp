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
//
// **A refusal that passes is waited out**, on Windows: see file_is_there.
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

// **How many times a refusal that passes is asked again**, a millisecond
// apart, before it is taken for a refusal that stays.
//
// On Windows a name whose file is delete-pending - deleted, or replaced by a
// rename, while a handle to it is still open - answers every open and every
// look at it with ERROR_ACCESS_DENIED (NTSTATUS STATUS_DELETE_PENDING) until
// the last handle closes and the name is free; and a file open without
// sharing what is asked answers ERROR_SHARING_VIOLATION until it is closed.
// Another process sharing the cache - an older build that replaced files, a
// virus scanner, an indexer - can make either for a moment, and CI saw
// `exists: Access is denied` of a DEM tile under 3200 threads at once. POSIX
// has neither state, so nothing there is asked again.
inline constexpr int transient_refusal_tries = 5000;

// **Whether a file or directory is at `path`**, as std::filesystem::exists,
// except that on Windows a refusal that passes (above) is asked again, up to
// transient_refusal_tries times, and a delete-pending name that frees itself
// is not there. Throws ByteSourceError if it cannot be told.
bool file_is_there(const std::filesystem::path& path);

// How many refusals that pass file_is_there and FileSource have waited out in
// this process, all threads together: always 0 but on Windows. A test waits
// on it to know a reader is waiting.
std::uint64_t transient_refusals_waited();

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

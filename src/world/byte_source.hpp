#pragma once

// Random access to a file's bytes, wherever the file is.

#include <chrono>
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

// **How long a refusal that passes is asked again**, a millisecond apart - or
// as near to that as the platform sleeps: Windows' timer ticks every 15.6 ms
// unless something has asked it for finer - before it is taken for a refusal
// that stays. Bounded by time rather than by tries, since the tries a
// millisecond's sleep buys differ fifteenfold between machines; and bounded
// at all because ERROR_ACCESS_DENIED is also what a file its ACL denies
// answers, which no wait changes.
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
inline constexpr std::chrono::milliseconds transient_refusal_wait{3000};

// **Whether a file or directory is at `path`**, as std::filesystem::exists,
// except that on Windows a refusal that passes (above) is asked again for up
// to transient_refusal_wait, and a delete-pending name that frees itself is
// not there. Throws ByteSourceError if it cannot be told.
bool file_is_there(const std::filesystem::path& path);

// How many refusals that pass file_is_there, FileSource and
// move_into_place_unless_there have waited out in this process, all threads
// together: always 0 but on Windows. A test waits on it to know a call is
// waiting.
std::uint64_t transient_refusals_waited();

// The calls move_into_place_unless_there moves a file with on POSIX, each
// answering 0 or -1 with errno set, as the C library's do. A test replaces
// them to refuse as a filesystem without them refuses. Not used on Windows.
struct PosixMoves {
    int (*link)(const char* from, const char* to);
    // renameat2 with RENAME_NOREPLACE on Linux, renamex_np with RENAME_EXCL
    // on macOS; elsewhere it refuses with ENOSYS.
    int (*rename_exclusive)(const char* from, const char* to);
    int (*rename)(const char* from, const char* to);
};
const PosixMoves& posix_moves();

// **`from` moved to `to` unless something is at `to` already**, in one step
// that cannot replace it: true if it was moved, false - with `from` left
// where it is - if something was there.
//
// On Windows, MoveFileExW without MOVEFILE_REPLACE_EXISTING; a refusal that
// passes is waited out, and a name that was only delete-pending is taken once
// it is free. On POSIX, a second name made with link() and the first removed.
// Where the filesystem has no hard links - link refuses with EPERM,
// EOPNOTSUPP, ENOTSUP or ENOSYS: vfat, exFAT, SMB, some FUSE filesystems -
// the exclusive rename; where it has not that either (EINVAL, or the same
// codes), rename(), which replaces, and on POSIX harmlessly: a replaced
// file's name is free at once, and whoever has it open reads on. Throws
// ByteSourceError on any other failure, and if the first name cannot be
// removed after link() made the second.
bool move_into_place_unless_there(const std::filesystem::path& from,
                                  const std::filesystem::path& to,
                                  const PosixMoves& moves = posix_moves());

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

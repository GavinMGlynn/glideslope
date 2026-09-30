#pragma once

// Random access to a file's bytes, wherever the file is.

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace glideslope::world {

struct ByteSourceError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// **Which file a name held**: its device, or volume, and its number on it -
// st_dev and st_ino on POSIX, the volume serial number and file index on
// Windows. Two names with one identity are one file; a file moved into place
// over a name gives the name another.
struct FileIdentity {
    std::uint64_t device = 0;
    std::uint64_t file = 0;
    bool operator==(const FileIdentity&) const = default;
};

// Where a file's bytes come from: a file on disk, memory, or later a download.
class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual std::uint64_t size() const = 0;
    // Fills `out` from `offset`. Throws ByteSourceError if that runs past the end
    // or the read fails.
    virtual void read(std::uint64_t offset, std::span<std::uint8_t> out) const = 0;
    // The file read, if the bytes are a file's.
    virtual std::optional<FileIdentity> identity() const {
        return std::nullopt;
    }
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
    std::optional<FileIdentity> identity() const override {
        return identity_;
    }

private:
    std::filesystem::path path_;
    // A HANDLE on Windows, a file descriptor elsewhere.
    std::intptr_t file_ = -1;
    std::uint64_t size_ = 0;
    FileIdentity identity_;
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
//
// **The move is made durable**: on Windows with MOVEFILE_WRITE_THROUGH, on
// POSIX by syncing the directory after it. The directory's sync is asked and
// not insisted on - some filesystems refuse to sync a directory - since what
// a power cut can then lose is the new name, and a name lost is a file fetched
// again, never a file cut short: its bytes were written with write_durably.
bool move_into_place_unless_there(const std::filesystem::path& from,
                                  const std::filesystem::path& to,
                                  const PosixMoves& moves = posix_moves());

// **`bytes` written to `path`, and on the disk before this returns**: the
// file made or emptied, written, and flushed through the operating system's
// cache - fsync on POSIX (F_FULLFSYNC on Apple's, whose fsync leaves the data
// in the drive's own cache), FlushFileBuffers on Windows. A file moved into place
// after this is whole after a power cut too: without it, the rename can reach
// the disk before the data does, and a cache is left holding a file cut short
// under its final name. Throws ByteSourceError if any of it fails.
void write_durably(const std::filesystem::path& path, std::span<const std::uint8_t> bytes);

// **The file at `path` taken away, its name free at once**: renamed to a name
// of its own beside it and that removed, so on Windows a reader still holding
// it (FileSource shares deletion) does not leave the name delete-pending for
// the fetch that follows. On Windows a refusal that passes is waited out, as
// every move here waits it out.
//
// **Only the file that was read**: given `read`, the identity of the file
// found wanting, a file at the name that is another - put in place by another
// process that found the same file wanting and fetched it again first - is
// left where it is. What remains is the moment between looking and renaming:
// a file put in place in it is taken away, and fetched again.
//
// True if a file was taken away; false if nothing was there, or another file
// than `read` is. Throws ByteSourceError if the file is there and cannot be
// moved. A renamed file that cannot then be removed is left under its own
// name, which nothing asks for.
bool take_away(const std::filesystem::path& path,
               const std::optional<FileIdentity>& read = std::nullopt);

// The identity of the file at `path`, or nothing if nothing is there. Throws
// ByteSourceError if it cannot be told.
std::optional<FileIdentity> identity_of(const std::filesystem::path& path);

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

#include "world/byte_source.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <random>
#include <string>
#include <system_error>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace glideslope::world {

namespace {
std::atomic<std::uint64_t> waited{0};

[[noreturn]] void refuse_move(const std::filesystem::path& from, const std::string& why) {
    throw ByteSourceError("cannot move " + from.string() + " into place: " + why);
}
} // namespace

std::uint64_t transient_refusals_waited() {
    return waited.load();
}

#if defined(_WIN32)

namespace {
HANDLE handle(std::intptr_t file) {
    return reinterpret_cast<HANDLE>(file);
}

// A refusal that passes: see transient_refusal_wait.
bool passes(DWORD error) {
    return error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION ||
           error == ERROR_DELETE_PENDING;
}

FileIdentity identity_from(const BY_HANDLE_FILE_INFORMATION& info) {
    return {info.dwVolumeSerialNumber,
            (std::uint64_t{info.nFileIndexHigh} << 32) | info.nFileIndexLow};
}

bool not_there(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
           error == ERROR_INVALID_NAME || error == ERROR_BAD_NETPATH;
}

// What a call answered last - 0, or a Windows error - and how often and for
// how long it was asked.
struct Settled {
    DWORD error = 0;
    int tries = 0;
    std::chrono::milliseconds took{0};

    std::string said() const {
        return "Windows error " + std::to_string(error) + " after " +
               std::to_string(tries) + (tries == 1 ? " try" : " tries") + " in " +
               std::to_string(took.count()) + " ms";
    }
};

// `attempt` asked again, a millisecond apart, while it answers a refusal
// that passes, until transient_refusal_wait has gone by since `start`.
template <class Attempt>
Settled settle(Attempt attempt,
               std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now()) {
    for (int tries = 1;; ++tries) {
        const DWORD error = attempt();
        const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (error == 0 || !passes(error) || took >= transient_refusal_wait) {
            return {error, tries, took};
        }
        ++waited;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
} // namespace

bool file_is_there(const std::filesystem::path& path) {
    const Settled s = settle([&]() -> DWORD {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES
                   ? DWORD{0}
                   : GetLastError();
    });
    if (s.error == 0) {
        return true;
    }
    if (not_there(s.error)) {
        return false;
    }
    throw ByteSourceError("cannot tell whether " + path.string() + " is there: " +
                          s.said());
}

FileSource::FileSource(const std::filesystem::path& path) : path_(path) {
    HANDLE h = INVALID_HANDLE_VALUE;
    const Settled s = settle([&]() -> DWORD {
        // FILE_SHARE_DELETE is the point of opening it here: see the header.
        h = CreateFileW(path.c_str(), GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        // Kept before anything else can call into Windows and change it.
        return h != INVALID_HANDLE_VALUE ? DWORD{0} : GetLastError();
    });
    if (s.error != 0) {
        throw ByteSourceError("cannot open " + path.string() + ": " + s.said());
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        throw ByteSourceError("cannot find the size of " + path.string());
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(h, &info)) {
        CloseHandle(h);
        throw ByteSourceError("cannot tell which file " + path.string() + " is");
    }
    file_ = reinterpret_cast<std::intptr_t>(h);
    size_ = static_cast<std::uint64_t>(size.QuadPart);
    identity_ = identity_from(info);
}

FileSource::~FileSource() {
    CloseHandle(handle(file_));
}

const PosixMoves& posix_moves() {
    static const PosixMoves none{nullptr, nullptr, nullptr};
    return none;
}

bool move_into_place_unless_there(const std::filesystem::path& from,
                                  const std::filesystem::path& to,
                                  [[maybe_unused]] const PosixMoves& moves) {
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        // No MOVEFILE_REPLACE_EXISTING: a free name is taken, one in use
        // refuses.
        const Settled s = settle(
            [&]() -> DWORD {
                return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH)
                           ? DWORD{0}
                                                                : GetLastError();
            },
            start);
        if (s.error == 0) {
            return true;
        }
        if (s.error != ERROR_ALREADY_EXISTS && s.error != ERROR_FILE_EXISTS) {
            refuse_move(from, s.said());
        }
        // Taken - unless what took it was delete-pending and has gone since,
        // which file_is_there waits out; then the name is free to try again.
        bool taken = true;
        try {
            taken = file_is_there(to);
        } catch (const ByteSourceError& e) {
            refuse_move(from, e.what());
        }
        if (taken) {
            return false;
        }
        if (std::chrono::steady_clock::now() - start >= transient_refusal_wait) {
            refuse_move(from, to.string() + " is neither there nor free after " +
                                  std::to_string(transient_refusal_wait.count()) +
                                  " ms");
        }
    }
}

void write_durably(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        throw ByteSourceError("cannot write " + path.string() + ": Windows error " +
                              std::to_string(GetLastError()));
    }
    std::size_t done = 0;
    while (done < bytes.size()) {
        const auto want = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size() - done, std::size_t{1} << 30));
        DWORD wrote = 0;
        if (!WriteFile(h, bytes.data() + done, want, &wrote, nullptr)) {
            const DWORD error = GetLastError();
            CloseHandle(h);
            throw ByteSourceError("cannot write " + path.string() + ": Windows error " +
                                  std::to_string(error));
        }
        if (wrote == 0) {
            CloseHandle(h);
            throw ByteSourceError("cannot write " + path.string() + ": Windows wrote " +
                                  "nothing of the " + std::to_string(bytes.size() - done) +
                                  " bytes left, and said no error");
        }
        done += wrote;
    }
    if (!FlushFileBuffers(h)) {
        const DWORD error = GetLastError();
        CloseHandle(h);
        throw ByteSourceError("cannot flush " + path.string() + " to the disk: Windows error " +
                              std::to_string(error));
    }
    if (!CloseHandle(h)) {
        throw ByteSourceError("cannot close " + path.string() + ": Windows error " +
                              std::to_string(GetLastError()));
    }
}


std::optional<FileIdentity> identity_of(const std::filesystem::path& path) {
    HANDLE h = INVALID_HANDLE_VALUE;
    const Settled s = settle([&]() -> DWORD {
        h = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        return h != INVALID_HANDLE_VALUE ? DWORD{0} : GetLastError();
    });
    if (s.error != 0) {
        if (not_there(s.error)) {
            return std::nullopt;
        }
        throw ByteSourceError("cannot tell which file " + path.string() + " is: " +
                              s.said());
    }
    BY_HANDLE_FILE_INFORMATION info{};
    const bool told = GetFileInformationByHandle(h, &info) != 0;
    CloseHandle(h);
    if (!told) {
        throw ByteSourceError("cannot tell which file " + path.string() + " is");
    }
    return identity_from(info);
}

namespace {
// `path` renamed to `aside`: true, or false if nothing is at `path`. A
// refusal that passes is waited out, as every move here waits it out.
bool rename_aside(const std::filesystem::path& path, const std::filesystem::path& aside) {
    const Settled s = settle([&]() -> DWORD {
        return MoveFileExW(path.c_str(), aside.c_str(), MOVEFILE_WRITE_THROUGH)
                   ? DWORD{0}
                   : GetLastError();
    });
    if (s.error == 0) {
        return true;
    }
    if (not_there(s.error)) {
        return false;
    }
    throw ByteSourceError("cannot take " + path.string() + " away: " + s.said());
}
} // namespace

#else

std::optional<FileIdentity> identity_of(const std::filesystem::path& path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) {
        const int error = errno;
        if (error == ENOENT || error == ENOTDIR) {
            return std::nullopt;
        }
        throw ByteSourceError("cannot tell which file " + path.string() + " is: " +
                              std::strerror(error));
    }
    return FileIdentity{static_cast<std::uint64_t>(st.st_dev),
                        static_cast<std::uint64_t>(st.st_ino)};
}

namespace {
bool rename_aside(const std::filesystem::path& path, const std::filesystem::path& aside) {
    if (::rename(path.c_str(), aside.c_str()) == 0) {
        return true;
    }
    const int error = errno;
    if (error == ENOENT) {
        return false;
    }
    throw ByteSourceError("cannot take " + path.string() + " away: " + std::strerror(error));
}
} // namespace

bool file_is_there(const std::filesystem::path& path) {
    std::error_code error;
    const bool there = std::filesystem::exists(path, error);
    if (error) {
        throw ByteSourceError("cannot tell whether " + path.string() +
                              " is there: " + error.message());
    }
    return there;
}

FileSource::FileSource(const std::filesystem::path& path) : path_(path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        // Kept before anything else can call into the C library and change it.
        const int error = errno;
        throw ByteSourceError("cannot open " + path.string() + ": " +
                              std::strerror(error));
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        throw ByteSourceError("cannot find the size of " + path.string());
    }
    file_ = fd;
    size_ = static_cast<std::uint64_t>(st.st_size);
    identity_ = {static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino)};
}

FileSource::~FileSource() {
    ::close(static_cast<int>(file_));
}

void write_durably(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        const int error = errno;
        throw ByteSourceError("cannot write " + path.string() + ": " + std::strerror(error));
    }
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t wrote = ::write(fd, bytes.data() + done, bytes.size() - done);
        if (wrote < 0 && errno == EINTR) {
            continue;
        }
        if (wrote <= 0) {
            const int error = wrote < 0 ? errno : EIO;
            ::close(fd);
            throw ByteSourceError("cannot write " + path.string() + ": " +
                                  std::strerror(error));
        }
        done += static_cast<std::size_t>(wrote);
    }
#if defined(__APPLE__)
    // Apple's fsync reaches the drive but not past its own cache; F_FULLFSYNC
    // asks the drive to write that too, and a filesystem that cannot is fsynced.
    const bool flushed = ::fcntl(fd, F_FULLFSYNC) == 0 || ::fsync(fd) == 0;
#else
    const bool flushed = ::fsync(fd) == 0;
#endif
    if (!flushed) {
        const int error = errno;
        ::close(fd);
        throw ByteSourceError("cannot flush " + path.string() + " to the disk: " +
                              std::strerror(error));
    }
    if (::close(fd) != 0) {
        const int error = errno;
        throw ByteSourceError("cannot close " + path.string() + ": " + std::strerror(error));
    }
}

namespace {
// The directory holding `path` synced, so a name just made in it outlasts a
// power cut. Asked, not insisted on: see move_into_place_unless_there.
void sync_directory_of(const std::filesystem::path& path) {
    std::filesystem::path directory = path.parent_path();
    if (directory.empty()) {
        directory = ".";
    }
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        return;
    }
    (void)::fsync(fd);
    ::close(fd);
}
} // namespace

namespace {
int exclusive_rename(const char* from, const char* to) {
#if defined(__linux__)
    return ::renameat2(AT_FDCWD, from, AT_FDCWD, to, RENAME_NOREPLACE);
#elif defined(__APPLE__)
    return ::renamex_np(from, to, RENAME_EXCL);
#else
    (void)from;
    (void)to;
    errno = ENOSYS;
    return -1;
#endif
}

int plain_rename(const char* from, const char* to) {
    return std::rename(from, to);
}

// What a filesystem without the call answers.
bool unsupported(int error) {
    // EOPNOTSUPP and ENOTSUP are one value on Linux and two on macOS.
    for (const int code : {EPERM, EOPNOTSUPP, ENOTSUP, ENOSYS}) {
        if (error == code) {
            return true;
        }
    }
    return false;
}
} // namespace

const PosixMoves& posix_moves() {
    static const PosixMoves real{::link, exclusive_rename, plain_rename};
    return real;
}

bool move_into_place_unless_there(const std::filesystem::path& from,
                                  const std::filesystem::path& to,
                                  const PosixMoves& moves) {
    if (moves.link(from.c_str(), to.c_str()) == 0) {
        if (::unlink(from.c_str()) != 0) {
            const int error = errno;
            throw ByteSourceError(to.string() + " is in place, but its temporary name " +
                                  from.string() + " cannot be removed: " +
                                  std::strerror(error));
        }
        sync_directory_of(to);
        return true;
    }
    int error = errno;
    if (error == EEXIST) {
        return false;
    }
    if (!unsupported(error)) {
        refuse_move(from, std::string("link: ") + std::strerror(error));
    }
    if (moves.rename_exclusive(from.c_str(), to.c_str()) == 0) {
        sync_directory_of(to);
        return true;
    }
    error = errno;
    if (error == EEXIST) {
        return false;
    }
    if (!unsupported(error) && error != EINVAL) {
        refuse_move(from, std::string("an exclusive rename: ") + std::strerror(error));
    }
    if (moves.rename(from.c_str(), to.c_str()) == 0) {
        sync_directory_of(to);
        return true;
    }
    error = errno;
    refuse_move(from, std::string("rename: ") + std::strerror(error));
}

#endif

bool take_away(const std::filesystem::path& path, const std::optional<FileIdentity>& read) {
    if (read) {
        const std::optional<FileIdentity> there = identity_of(path);
        if (!there || *there != *read) {
            return false;
        }
    }
    static std::atomic<unsigned long long> taken{0};
    std::filesystem::path aside = path;
    aside += ".taken-away." + std::to_string(std::random_device{}()) + "-" +
             std::to_string(taken++);
    if (!rename_aside(path, aside)) {
        return false;
    }
    std::error_code error;
    std::filesystem::remove(aside, error);
    return true;
}

std::uint64_t FileSource::size() const {
    return size_;
}

void FileSource::read(std::uint64_t offset, std::span<std::uint8_t> out) const {
    if (offset > size_ || out.size() > size_ - offset) {
        throw ByteSourceError(path_.string() + ": a read past the end of the file");
    }
    // Every read says where it reads from, so reads from many threads at once
    // need nothing between them.
    std::size_t done = 0;
    while (done < out.size()) {
        const std::uint64_t at = offset + done;
        const std::size_t want =
            std::min<std::size_t>(out.size() - done, std::size_t{1} << 30);
#if defined(_WIN32)
        OVERLAPPED where{};
        where.Offset = static_cast<DWORD>(at & 0xffffffffu);
        where.OffsetHigh = static_cast<DWORD>(at >> 32);
        DWORD got = 0;
        if (!ReadFile(handle(file_), out.data() + done, static_cast<DWORD>(want), &got,
                      &where) ||
            got == 0) {
            throw ByteSourceError(path_.string() + ": a read failed");
        }
#else
        const ssize_t got = ::pread(static_cast<int>(file_), out.data() + done, want,
                                    static_cast<off_t>(at));
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got <= 0) {
            throw ByteSourceError(path_.string() + ": a read failed");
        }
#endif
        done += static_cast<std::size_t>(got);
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

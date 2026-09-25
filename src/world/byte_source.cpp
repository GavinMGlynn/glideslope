#include "world/byte_source.hpp"

#include <algorithm>
#include <cstring>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace glideslope::world {

#if defined(_WIN32)

namespace {
HANDLE handle(std::intptr_t file) {
    return reinterpret_cast<HANDLE>(file);
}
} // namespace

FileSource::FileSource(const std::filesystem::path& path) : path_(path) {
    // FILE_SHARE_DELETE is the point of opening it here: see the header.
    const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        throw ByteSourceError("cannot open " + path.string() + ": Windows error " +
                              std::to_string(GetLastError()));
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        throw ByteSourceError("cannot find the size of " + path.string());
    }
    file_ = reinterpret_cast<std::intptr_t>(h);
    size_ = static_cast<std::uint64_t>(size.QuadPart);
}

FileSource::~FileSource() {
    CloseHandle(handle(file_));
}

#else

FileSource::FileSource(const std::filesystem::path& path) : path_(path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw ByteSourceError("cannot open " + path.string() + ": " +
                              std::strerror(errno));
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        throw ByteSourceError("cannot find the size of " + path.string());
    }
    file_ = fd;
    size_ = static_cast<std::uint64_t>(st.st_size);
}

FileSource::~FileSource() {
    ::close(static_cast<int>(file_));
}

#endif

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

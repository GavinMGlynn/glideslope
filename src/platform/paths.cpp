#include "platform/paths.hpp"

#include <stdexcept>
#include <string>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#endif

namespace glideslope::platform {

namespace {

std::filesystem::path executable_path() {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(),
                                           static_cast<DWORD>(buffer.size()));
        if (n == 0) {
            throw std::runtime_error("GetModuleFileNameW failed");
        }
        if (n < buffer.size()) {
            buffer.resize(n);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        throw std::runtime_error("_NSGetExecutablePath failed");
    }
    buffer.resize(std::char_traits<char>::length(buffer.c_str()));
    return std::filesystem::canonical(buffer);
#else
    std::error_code error;
    const std::filesystem::path path =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) {
        throw std::runtime_error("could not read /proc/self/exe: " + error.message());
    }
    return path;
#endif
}

} // namespace

std::filesystem::path executable_directory() {
    return executable_path().parent_path();
}

std::filesystem::path data_directory() {
    return executable_directory() / "data";
}

} // namespace glideslope::platform

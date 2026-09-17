#include "platform/paths.hpp"

#include <cstdlib>
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

namespace {

std::filesystem::path environment_path(const char* name) {
#if defined(_WIN32)
    const std::wstring wide(name, name + std::char_traits<char>::length(name));
    const DWORD n = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
    if (n == 0) {
        return {};
    }
    std::wstring value(n, L'\0');
    const DWORD written = GetEnvironmentVariableW(wide.c_str(), value.data(), n);
    value.resize(written);
    return std::filesystem::path(value);
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::filesystem::path() : std::filesystem::path(value);
#endif
}

} // namespace

std::filesystem::path cache_directory() {
    if (const auto set = environment_path("GLIDESLOPE_CACHE"); !set.empty()) {
        return set;
    }
#if defined(_WIN32)
    const auto local = environment_path("LOCALAPPDATA");
    if (local.empty()) {
        throw std::runtime_error("LOCALAPPDATA is not set");
    }
    return local / "glideslope" / "cache";
#else
    const auto home = environment_path("HOME");
#if defined(__APPLE__)
    if (home.empty()) {
        throw std::runtime_error("HOME is not set");
    }
    return home / "Library" / "Caches" / "glideslope";
#else
    if (const auto xdg = environment_path("XDG_CACHE_HOME"); xdg.is_absolute()) {
        return xdg / "glideslope";
    }
    if (home.empty()) {
        throw std::runtime_error("neither XDG_CACHE_HOME nor HOME is set");
    }
    return home / ".cache" / "glideslope";
#endif
#endif
}

} // namespace glideslope::platform

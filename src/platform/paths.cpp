#include "platform/paths.hpp"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iterator>
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

std::filesystem::path config_directory() {
#if defined(_WIN32)
    const auto roaming = environment_path("APPDATA");
    if (roaming.empty()) {
        throw std::runtime_error("APPDATA is not set");
    }
    return roaming / "glideslope";
#else
    const auto home = environment_path("HOME");
#if defined(__APPLE__)
    if (home.empty()) {
        throw std::runtime_error("HOME is not set");
    }
    return home / "Library" / "Application Support" / "glideslope";
#else
    if (const auto xdg = environment_path("XDG_CONFIG_HOME"); xdg.is_absolute()) {
        return xdg / "glideslope";
    }
    if (home.empty()) {
        throw std::runtime_error("neither XDG_CONFIG_HOME nor HOME is set");
    }
    return home / ".config" / "glideslope";
#endif
#endif
}

namespace {

// A key the user keeps: an environment variable first, then a file of their
// own. Whitespace either end is not part of it - a file written by an editor
// ends with a newline - and nothing else is touched.
std::string secret(const char* variable, const char* file) {
    const auto trim = [](std::string s) {
        const auto space = [](unsigned char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        };
        std::size_t from = 0;
        while (from < s.size() && space(static_cast<unsigned char>(s[from]))) {
            ++from;
        }
        std::size_t to = s.size();
        while (to > from && space(static_cast<unsigned char>(s[to - 1]))) {
            --to;
        }
        return s.substr(from, to - from);
    };
#if defined(_WIN32)
    const std::wstring wide(variable,
                            variable + std::char_traits<char>::length(variable));
    const DWORD n = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
    if (n != 0) {
        std::wstring value(n, L'\0');
        const DWORD written = GetEnvironmentVariableW(wide.c_str(), value.data(), n);
        value.resize(written);
        // Windows keeps its environment in UTF-16; a key is handed on as
        // UTF-8, which is what every URL it goes into wants. Narrowing each
        // unit by itself would be wrong for anything outside ASCII, and a
        // key is not promised to be ASCII.
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                                              static_cast<int>(value.size()),
                                              nullptr, 0, nullptr, nullptr);
        std::string narrow(static_cast<std::size_t>(bytes < 0 ? 0 : bytes), '\0');
        if (bytes > 0) {
            WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                                static_cast<int>(value.size()), narrow.data(),
                                bytes, nullptr, nullptr);
        }
        if (!trim(narrow).empty()) {
            return trim(narrow);
        }
    }
#else
    if (const char* value = std::getenv(variable); value != nullptr) {
        if (!trim(value).empty()) {
            return trim(value);
        }
    }
#endif
    std::error_code error;
    const std::filesystem::path where = [&]() -> std::filesystem::path {
        try {
            return config_directory() / file;
        } catch (const std::runtime_error&) {
            return {}; // no home: the user has no key here either
        }
    }();
    if (where.empty() || !std::filesystem::exists(where, error)) {
        return {};
    }
    std::ifstream in(where, std::ios::binary);
    if (!in) {
        return {};
    }
    return trim(std::string((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>()));
}

} // namespace

std::string cesium_ion_token() {
    return secret("GLIDESLOPE_CESIUM_ION_TOKEN", "cesium-ion-token");
}

std::string google_maps_key() {
    return secret("GLIDESLOPE_GOOGLE_MAPS_KEY", "google-maps-key");
}

} // namespace glideslope::platform

#pragma once

// **What a program says goes to a file**, for a test's `--stdout`: a test
// running it beside others, whose standard output feeds the next one's input,
// reads it there. MSVC deprecates `freopen` (C4996, an error here) for
// `freopen_s`, which C++ does not have; each platform gets its own.

#include <cstdio>
#include <string>

namespace glideslope::platform {

// False when the file could not be opened.
inline bool stdout_to_file(const std::string& path) {
#if defined(_WIN32)
    std::FILE* reopened = nullptr;
    return freopen_s(&reopened, path.c_str(), "w", stdout) == 0 && reopened != nullptr;
#else
    return std::freopen(path.c_str(), "w", stdout) != nullptr;
#endif
}

} // namespace glideslope::platform

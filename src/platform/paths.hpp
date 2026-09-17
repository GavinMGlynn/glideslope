#pragma once

#include <filesystem>

namespace glideslope::platform {

// The directory holding the running program, resolved through any symbolic
// links. Throws std::runtime_error if the operating system will not say.
std::filesystem::path executable_directory();

// Where the program's data lives: data/ beside the program. The build tree and
// an unpacked package are laid out the same way, so this holds in both.
std::filesystem::path data_directory();

} // namespace glideslope::platform

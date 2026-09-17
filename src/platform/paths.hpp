#pragma once

#include <filesystem>

namespace glideslope::platform {

// The directory holding the running program, resolved through any symbolic
// links. Throws std::runtime_error if the operating system will not say.
std::filesystem::path executable_directory();

// Where the program's data lives: data/ beside the program. The build tree and
// an unpacked package are laid out the same way, so this holds in both.
std::filesystem::path data_directory();

// Where downloaded data is kept between runs: GLIDESLOPE_CACHE if it is set,
// otherwise the user's cache directory - %LOCALAPPDATA%\glideslope\cache on
// Windows, ~/Library/Caches/glideslope on macOS, and $XDG_CACHE_HOME/glideslope
// or ~/.cache/glideslope elsewhere. Not created. Throws std::runtime_error if
// no home directory can be found.
std::filesystem::path cache_directory();

} // namespace glideslope::platform

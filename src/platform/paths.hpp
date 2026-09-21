#pragma once

#include <filesystem>
#include <string>

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

// **Where Cesium Native keeps its cache**, which is a single SQLite database.
// `GLIDESLOPE_CESIUM_CACHE` names the file; without it, `cesium-cache.sqlite`
// in the cache directory.
//
// It is named apart from everything else in that directory because two
// programs sharing one of these files tread on each other:
// `CesiumAsync::SqliteCache` turns on WAL but never sets a busy timeout, so a
// second writer is refused at once rather than waiting - "database is locked"
// - and the entry is simply not stored. Tests that run at once each name
// their own.
std::filesystem::path cesium_cache_file();

// Where the user's own settings live - %APPDATA%\glideslope on Windows,
// ~/Library/Application Support/glideslope on macOS, and
// $XDG_CONFIG_HOME/glideslope or ~/.config/glideslope elsewhere. Not created.
// Throws std::runtime_error if no home directory can be found.
std::filesystem::path config_directory();

// The user's own Cesium ion token, or empty if they have none.
//
// **A key belongs to the user and is never in the repository.** It is read at
// run time, from GLIDESLOPE_CESIUM_ION_TOKEN if that is set, and otherwise
// from the file `cesium-ion-token` in the config directory above. Leading and
// trailing space is taken off; anything else is the token as given. A missing
// file, an unreadable one and an empty one are all "none", because a provider
// that needs one says so rather than failing.
std::string cesium_ion_token();

// The user's own Google Maps Platform key, the same way: from
// GLIDESLOPE_GOOGLE_MAPS_KEY, or the file `google-maps-key`.
std::string google_maps_key();

} // namespace glideslope::platform

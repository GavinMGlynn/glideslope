#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string_view>
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

// **Where Cesium ion's API is**: https://api.cesium.com, unless
// `GLIDESLOPE_CESIUM_ION_API` names somewhere else - a test's stand-in on the
// loopback, which answers where the terrain is and then never sends it. The
// user's token goes wherever this names, so nothing but a person or a test
// setting that variable ever changes it. No trailing slash. Throws
// std::runtime_error for anything but https, or http on the loopback: the
// token is in each request's query, and is not sent in the clear.
std::string cesium_ion_api();

// The user's own Google Maps Platform key, the same way: from
// GLIDESLOPE_GOOGLE_MAPS_KEY, or the file `google-maps-key`.
std::string google_maps_key();

// The user's own keys for the language models that plan: OpenAI's, from
// GLIDESLOPE_OPENAI_KEY or the file `openai-key`, and Anthropic's, from
// GLIDESLOPE_ANTHROPIC_KEY or the file `anthropic-key`.
std::string openai_key();
std::string anthropic_key();

// **Where the weather services are asked instead of their own hosts, for a
// test.** GLIDESLOPE_WEATHER_SERVICE, a scheme and host such as
// `http://127.0.0.1:1`, put in place of `https://aviationweather.gov` and
// `https://api.open-meteo.com` in every weather request; empty when it is not
// set, and then the services' own hosts are asked. It exists so a test can
// build "no weather to be had" on purpose, rather than wait for a service to
// have a bad minute.
std::string weather_service();

// **The default server, as a line anybody can send you.** `--online` reads
// it: one line naming a host, a port and the server's public key, which is
// what a client needs and all it needs (`REQUIREMENTS.md` 6.6).
//
//   glideslope.example.org 47801 49cf887b...8305fb75
//
// Separated by spaces, in that order. Blank lines and lines beginning with
// `#` are skipped, so the file can say where it came from.
//
// **The key in it is not a secret** - it is the half a server prints at
// startup for exactly this purpose - so unlike a token this file may be
// passed around, posted, or committed to somebody else's repository.
struct DefaultServer {
    std::string host;
    std::uint16_t port = 0;
    std::string key_hex;
};

// The `server.txt` in the config directory, or nothing if there is none, it
// cannot be read, or no line of it is one. From GLIDESLOPE_SERVER_TXT if that
// names a file instead.
std::optional<DefaultServer> default_server();

// The same, read from `text` rather than from a file. Nothing if no line of
// it names a host, a port and a key.
std::optional<DefaultServer> read_default_server(std::string_view text);

} // namespace glideslope::platform

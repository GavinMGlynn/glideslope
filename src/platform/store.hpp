#pragma once

// What a server keeps between runs.
//
// **The one thing that must survive a restart is the server's key.** A client
// is given it out of band - written in a `server.txt`, pasted into a command
// line - so a server that minted a fresh one every time it started would
// break every client it had at each restart. `--store FILE` is where it is
// kept, and `REQUIREMENTS.md` 6.6 says that file is SQLite.
//
// **It is a table of names and values and nothing more.** A session's state,
// when there is one to keep, goes in its own table beside it; this is not a
// schema for the whole server, it is the smallest thing that keeps a key.
//
// **SQLite rather than a line in a file** because the requirement says so,
// because more will be kept here, and because it is already built: Cesium
// Native's dependencies bring it.

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

struct sqlite3;

namespace glideslope::platform {

struct StoreError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Store {
public:
    // Opens `file`, making it if it is not there. Throws StoreError naming
    // what SQLite said if it cannot.
    explicit Store(const std::filesystem::path& file);
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    Store(Store&&) noexcept;
    Store& operator=(Store&&) noexcept;

    // What is kept under `name`, or nothing.
    std::optional<std::string> get(std::string_view name) const;
    // Keeps `value` under `name`, replacing whatever was there.
    void set(std::string_view name, std::string_view value);

    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path file_;
    sqlite3* db_ = nullptr;
};

} // namespace glideslope::platform

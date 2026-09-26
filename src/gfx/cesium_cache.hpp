#pragma once

// **Cesium Native's cache of what it fetches, safe to share.**
//
// `CesiumAsync::SqliteCache` keeps what Cesium Native fetches in one SQLite
// file, and two programs using that file at once lost entries, both ways:
//
//   - **It sets no busy timeout**, so a write that meets another program's
//     is refused at once - "database is locked" - rather than waiting its
//     turn, and the entry is not stored.
//   - **A busy timeout alone does not help a lookup.** `getEntry` steps its
//     lookup and, with it still open, updates the entry's last-used time: a
//     write from inside a read transaction, which SQLite refuses at once,
//     busy handler or none, when another program holds the lock or has
//     written since the read began. The hit is then reported as a miss, and
//     fetched again. And the lookup is never finished, so the read stays
//     open until the next one.
//
// The cache opened here fixes both, from outside Cesium Native and without
// changing it. Its connection waits for another writer, up to
// cesium_cache_wait, from the moment it is opened - which covers the table
// and WAL that opening a new file writes. Each call into it is one
// transaction that takes the write lock before the call, which is where
// SQLite does wait, and after it no statement is left open. So any number
// of programs can share one cache file, and each stores and finds everything.

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace CesiumAsync {
class ICacheDatabase;
}

namespace glideslope::gfx {

// How long a write to the cache waits for another program's to finish before
// it is refused. A write holds the file for as long as one statement takes,
// which is well under a millisecond; this is for a machine far slower than
// that, not for a program that holds the file and never lets go.
inline constexpr std::chrono::milliseconds cesium_cache_wait{10000};

// The cache in `file`, created if there is none. Throws std::runtime_error if
// it cannot be opened.
std::shared_ptr<CesiumAsync::ICacheDatabase>
open_cesium_cache(const std::filesystem::path& file);

// How many times, in this process, a cache opened here found another
// program writing its file and waited. A test that means two programs to
// collide reads this to know that they did.
std::uint64_t cesium_cache_waits();

} // namespace glideslope::gfx

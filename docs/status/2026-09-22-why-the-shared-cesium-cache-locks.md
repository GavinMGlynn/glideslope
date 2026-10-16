### Why the shared Cesium cache locks, 2026-09-22

Read out of Cesium Native rather than guessed at. **It is not readers
blocking writers**, which was the obvious guess: `SqliteCache` already turns
on WAL - `PRAGMA journal_mode=WAL` at `SqliteCache.cpp:58`, run at line 214 -
and that is the part that would otherwise hurt.

**It is that no busy timeout is ever set.** The connection is opened with
`sqlite3_open` and there is no `sqlite3_busy_timeout` or
`sqlite3_busy_handler` anywhere in the file - the word "busy" does not appear
in it. SQLite's default timeout is zero, so a second *writer* - a second
process sharing the cache, which WAL does not serialise - gets `SQLITE_BUSY`
on its first attempt instead of waiting a moment, and the store fails. That
is what "database is locked" in our test logs has been all along.

Their fix is one line after the open, and it is drafted as a third issue in
`docs/cesium-issues.md`.

**Ours was to stop the tests sharing one file**, and that is done.
`GLIDESLOPE_CESIUM_CACHE` names the database, `platform::cesium_cache_file()`
reads it, and `tests/cmake/client.cmake` - which all thirteen client tests
include - gives each a name of its own made from the script running it and
the things that tell its runs apart. They stay in the downloads directory, so
each test still finds its own cache on the next run.

**The verification is enforced from that one place too.** A locked cache
loses nothing but the caching, which is why it went unnoticed for so long, so
`glideslope_judge_leaks` now fails any run that reports one. Every client
test calls it, directly or through `glideslope_client`.

**It costs nothing, measured rather than assumed.** The worry was that
isolating the caches would mean fetching the same tiles for each test: 323
tests in 973 s, against 977 to 1,006 s for the same suite before. Writes now
succeed instead of failing and the tile being fetched again, which appears to
pay for the loss of sharing.

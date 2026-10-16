### Programs sharing one Cesium cache each store everything, 2026-09-27 — tail in progress

**What is not yet shown first**: the verification asks for the whole suite
at `-j4` ten times over with no locked cache, and **one whole run and part of
a second were done here**, not ten. The machine was shared with other
agents' suites, and one run took 64 minutes; ten were stopped as
unreasonable, at the coordinator's asking. That is one run with no locked
cache, and nine of the ten whole-suite runs at `-j4` still to do - CI's
shards do not count toward them. The fix and its test are in, and pass on
Linux and Windows.

**Why names were not enough.** Each rendering test was given a cache file
of its own on 2026-09-22, and the lock came back as tests were added: the
names are made from the things a script's cases differ by, and three times a
new case differed by something the name did not carry (the view, the
imagery, riding along against taking over) - a rule kept by hand, in one
script, against every test anyone adds. Which pair collided on 2026-09-22 was
not recorded and could not be recovered; on today's tree every program that
opens a Cesium cache is the client run through `tests/cmake/client.cmake` or
`ion_stalled.cmake`, and the watch below found no two sharing one. So the fix
is not a better name but a cache that two programs can share, which makes a
name that collides cost a wait instead of a lost entry.

**Two faults in `CesiumAsync::SqliteCache`, not one**, read out of Cesium
Native and then each shown by the test below:

- *No busy timeout*, as found on 2026-09-22: a write that meets another
  program's is refused at once. With nothing waiting, the two writers stored
  0 and 63 of their 200 entries, and Cesium Native logged "database is
  locked" 337 times.
- *A busy timeout alone does not save a lookup.* `getEntry` steps its lookup
  and, with the statement still open, updates the entry's last-used time
  (`SqliteCache.cpp:422`) - a write from inside a read transaction, which
  SQLite refuses at once, without calling any busy handler, when another
  program holds the lock or has written since the read began. The hit is
  returned as a miss and fetched again. And the lookup is never reset, so
  the read stays open until the next one. With a busy handler set and each
  statement reset after the call, both writers stored all 200 and one read
  back only 191.

**The fix is outside Cesium Native** (`src/gfx/cesium_cache.{hpp,cpp}`; the
vendored code is untouched). `gfx::open_cesium_cache` registers an
`sqlite3_auto_extension` - the documented way to reach a connection opened
inside code that is not ours - which, only on a thread opening or calling
into one of these caches, gives the connection a busy handler that waits up
to `cesium_cache_wait` (2 s) for another writer. The cache it returns wraps
`SqliteCache`: each call runs inside `BEGIN IMMEDIATE ... COMMIT`, which
takes the write lock with no transaction open - the one place SQLite does
wait - and afterwards resets any statement left open; a `COMMIT` that fails
is rolled back and the call reported as not made. If the extension cannot be
registered, or never sees the connection open, opening the cache throws
rather than quietly refusing a second writer again.

**A program that stops mid-write costs one wait, not one per tile** (from
the review of PR #36). A `BEGIN IMMEDIATE` that does not get the lock within
the 2 s is not followed by the call: a lookup is a miss and a store is not
stored, as if there were no cache. For `cesium_cache_rest` (30 s) after it
every call is skipped the same way without waiting, and then the cache is
tried again. Without this, a peer hung holding the file would have stalled
each tile's lookup for the whole wait in turn. The pause is a parameter of
`open_cesium_cache`, 30 s by default, so a test can shorten it.

**Tested by
`a_cesium_cache_held_by_a_hung_program_costs_one_wait_and_is_used_again_after_its_pause`**
(`glideslope_cache_collision pause`, with a 3 s pause). A connection of the
test's own holds the file. The first store waits once, 2,004 ms, and is not
stored; three more are skipped in 0 ms each, with no wait counted; the
holder lets go, and a store made at once - the file free, but inside the
pause - is skipped too, so it is the pause doing it, not the lock; once the
pause has passed, a store is stored and read back. Each call is timed on the
steady clock and the cache's count of waits read around it. 5 s. **Seen to
fail** with the pause removed: each store in the pause waited 2 s again
(2,004, 2,001, 2,004 ms), and the store with the file free was stored. `TerrainTiles` opens its cache through it.
The per-test names in `client.cmake` are kept, so each test finds only its
own fetches, but they are no longer what keeps a run from being refused.

**The collision is built, not hoped for**:
`two_programs_writing_one_cesium_cache_at_once_both_store_everything`
(`tests/cmake/cesium_cache_collision.cmake`, `tests/tools/cache_collision.cpp`).
Two writer processes each store a seed and read it back, so each holds the
stale read that broke the lookups; a third process then takes the file's
write lock and writes a row, so every read before it is stale, and does not
let go until both writers are waiting on it - each writer's thread watching
the cache's count of waits says so the moment its first store waits. The
writers first check, with a connection that does not wait, that the file
really is held. Then each stores 200 entries against the other, reading each
back between stores, and a fresh open counts all 201 of each. It fails if
either's first store did not wait (so a run that did not collide is red, not
green), if anything is missing, or if "database is locked" appears at all.
It takes 0.3 s, with a 300 s `TIMEOUT` for a program that dies without
saying so. **No failure hangs it**: `hold` and `write`, failing or throwing,
leave `NAME.failed` in the directory, and every wait gives up the moment one
is there. Seen: with writer b made to fail its seed, all three exited in
0.09 s ("b could not store its seed", then the holder and writer a "gave up
waiting"), where before the holder and writer a would have waited for ever.
**Seen to fail** both ways above: with the busy handler
removed (0 and 63 of 200 stored), and with the handler but no transaction
round each call (191 of 200 read back).

**The suite runs, with the sharing watched rather than assumed**: a script
beside each run read every process's open files ten times a second and
logged any `.sqlite` under the build open by two processes, or twice by one.

- *Run 1*: all 585 tests in 3,855 s, 581 passed. The four that failed were
  the fixed-port take-over and impaired-network tests, "cannot listen on
  port 24790" - the pre-push hook's quick tests were running the same tests
  in the same build at the same moment (a first push, whose timings came
  from a run that had found no binaries, so "quick" was nearly everything).
  **That also showed a way the lock happens with every name unique**: the
  watch caught four cache files - `terrain_mismatch` open, `client_autopilot`,
  `client_memory`, `frame_weather` - each open in two clients at once, 62
  times, the same test's client from each of the two ctest runs. None logged
  "database is locked", and every rendering test passed the leak judge that
  fails on it. Two runs of ctest in one build - a push during a suite, two
  agents sharing a build - is enough, and no naming rule can prevent it.
- *Run 2*: stopped at 208 of 585 after 2,447 s. No cache file open twice, no
  locked cache; one failure,
  `an_aircraft_taken_back_from_the_ai_and_wrecked_flies_again_as_its_players`,
  a network test, not looked into here.

`grep` of both logs for "database is locked": none. "locked" appears only in
the names of the two `..._leaves_its_cache_unlocked_...` tests, which passed.

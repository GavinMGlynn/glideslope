### A `--terrain ion` run that times out is gone, and leaves its cache free, 2026-09-26 — tail done

**Cause: two waits with no end, one behind the other.**

- **At exit, the terrain waited for every transfer in flight**
  (`~TerrainTiles`: the tileset's asynchronous destruction, then every
  counted request). A tile server that answers and then sends its body a byte
  at a time never trips a stall timeout - libcurl's is "under 1 byte/s for
  60 s", WinHTTP's and NSURLSession's are per read - so the transfer never
  ended and neither did the program. It had already written its frame.
- **A SIGTERM could not reach it.** SDL installs its own SIGTERM and SIGINT
  handlers, which only queue a quit event; the client read events between
  frames, and a shot's frame spends up to 45 s inside the terrain's settling
  wait, then the exit above. So `timeout -s TERM` fired, the signal was
  swallowed, and the five runs found alive on 2026-09-22 were still holding
  the GPU and their Cesium caches a day and a half later.

**The lock was the live process.** Cesium's cache is WAL, and the OS drops a
process's SQLite locks when it goes, so "leaves no cache lock" is "is gone"
plus the file being writable afterwards.

**What changed.**

- `HttpRequest::abandon` (`platform/http.hpp`): a flag that gives a transfer
  up at once, with an `HttpError`, whatever it is waiting on. libcurl through
  its progress callback, which it calls about once a second even when no
  bytes are arriving; WinHTTP by closing the request handle from a small
  watcher thread, which Microsoft documents as the way to end a synchronous
  request from elsewhere; NSURLSession by cancelling the task, waited on a
  tenth of a second at a time.
- `TerrainTiles` raises its own flag as the first thing its destructor does,
  so every tile transfer still going ends as a failed request and the waits
  after it end.
- `platform/stop.hpp`: the client catches SIGTERM and SIGINT itself (Ctrl+C
  and Ctrl+Break on Windows), with `SDL_HINT_NO_SIGNAL_HANDLERS` so SDL
  installs none. The handler only raises a flag, installed with
  `SA_RESETHAND | SA_RESTART`, so the same signal sent a second time ends the
  program the system's way whatever it is stuck in. The frame loop, the
  terrain's settling wait, `heights_at`'s wait and the `--mismatch` run's
  loop over cells all give way to it, and Cesium ion's endpoint request
  abandons on it.
- **The Windows console closing is left to the system**, not caught:
  Windows ends the process as soon as the handler for CTRL_CLOSE_EVENT
  returns, whatever it answers, so raising the flag there would only race
  the main thread; waiting in the handler for the main thread to finish was
  the other choice, and would hold a closing console for as long as a
  transfer took to give up.
- **WinHTTP's abandonment leaves a race, named in the code.** After the
  watcher closes the request handle, `perform` asks before each later call
  (QueryHeaders, each ReadData) and throws instead of using it. The watcher
  can still close it between that question and the call; the call then uses
  a handle value WinHTTP may already have given to another thread's new
  request. Holding the lock across the call would close the gap only for
  calls that do not block, and ReadData is the one that does. The window is
  one call wide, and opens only while a program ends or a terrain closes. A stopped run says
  "stopped, as it was told to" and exits 128 + the signal: 143 for SIGTERM.
- `GLIDESLOPE_CESIUM_ION_API` (`platform::cesium_ion_api`) names where
  Cesium ion's API is, https://api.cesium.com unless set - the way a test
  points the ion provider at a stand-in. The token goes in the query of
  every request to it, so anything but https, or http on the loopback, is
  refused: `a_cesium_ion_api_that_would_send_the_token_in_the_clear_is_refused`
  walks ten names, five taken and five refused - among them hosts that only
  begin like the loopback - and went red with the refusal switched off.

**Verified** against `glideslope_ion_stall`, a stand-in for Cesium ion on the
loopback (`tests/tools/ion_stall.cpp`): it answers assets 1 and 2's endpoints
naming itself, then answers every tile, layer.json and Bing metadata request
with a 200, a length of 100 MB and a byte every 250 ms for ever, and writes a
file when the first such transfer starts so the test can wait for that event.
`tests/cmake/ion_stalled.cmake`, run with a token that is not one and no
network:

- `a_terrain_ion_run_whose_tiles_never_arrive_ends_by_itself_and_leaves_its_cache_unlocked_on_<driver>`
  (every platform): must be gone within 180 s - the settling wait's 45 s, and
  135 s for starting and ending a debug, sanitized build on a slow runner.
  Measured in linux-debug: **46 s**, exit 0.
- `a_terrain_ion_run_whose_tiles_never_arrive_is_gone_soon_after_sigterm_and_leaves_its_cache_unlocked_on_<driver>`
  (Linux and macOS): SIGTERM once a transfer is held, from a shell; must be
  gone within 30 s of it, with exit 143 **and** "glideslope: stopped, as it
  was told to" on standard error - a process the signal simply killed is
  reported as 143 too, and would prove nothing about an orderly stop.
  Measured in linux-debug: **1 s** (whole seconds, from `date +%s`). With
  only `catch_stop_signals()` taken out and SDL's handlers still kept off,
  it went red: "gone 0 s after SIGTERM with 143, but killed by it rather
  than stopping in order".
- The first test tells a run still alive at its limit from one that crashed
  on the way, and reports which.
- Each of the two then has `glideslope_ion_stall write` open the cache with no busy
  timeout and take and commit a write transaction, which must succeed at
  once; and the stand-in must say it answered both endpoints and held at
  least one transfer (2 each time here), so the run really was stuck in one.

**Seen failing**: with `catch_stop_signals`, the SDL hint and the destructor's
`closing->store(true)` taken out, and everything else - the stand-in and the
API override - left in, both went red: the SIGTERM run was "still alive 30 s
after SIGTERM, and killed"; the other wrote its frame 45 s in and was then
still alive at 180 s, when the test killed it.

**On Windows** (`tools/windows_build.sh`, windows-debug, MSVC) the first
test passed on direct3d12 in 50 s and on vulkan in 51 s - WinHTTP's
abandonment ending the held transfers. The HTTP tests passed but one:
`an_https_get_fetches_a_pinned_file_byte_for_byte` hit ctest's 900 s limit
in that run, beside two network tests that skipped for want of a network;
run again on its own, from the same build, it passed in 5 s.

**Not covered by a test**: the `--mismatch` run giving way to a stop. On
Windows there is no SIGTERM - a timed-out run
there is ended by TerminateProcess, which nothing outlives - so only the
first test runs there. The Windows Ctrl+C and Ctrl+Break handler is written but
not exercised. The real ion provider still draws with the change
(`the_ion_terrain_draws_with_its_attribution_or_says_why_not_on_vulkan`,
17 s), as do the HTTP tests.

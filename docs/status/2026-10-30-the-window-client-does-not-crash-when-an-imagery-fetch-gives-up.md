### The window client does not crash when an imagery fetch gives up, 2026-10-30

**What is still not true first.**
- **The fix is ours, around Cesium Native's bug, not in it.** Cesium Native's
  `CachingAssetAccessor` (v0.64.0, `CachingAssetAccessor.cpp` line 267) still
  calls through a null response when a stale entry's revalidation gets no
  answer, on `main` too (read on GitHub, 2026-10-30). The issue is drafted,
  not posted: `docs/cesium-issues.md`, Issue 4.
- **The 2026-10-10 crash is taken to be this one, not proved to be.** It was
  the same signal, at the same moment - exit, after a Sentinel-2 tile was
  "given up, unfinished" - and went away with `--imagery off`, which is what
  this cause predicts; its cache is long gone, so it cannot be replayed.

**The cause.** A segfault seen on 2026-10-30 in a linux-release run of
`the_client_with_the_window_joins_the_server_server_txt_names`, right after
`.../WGS84/7/88/234.jpg: given up, unfinished`, passing on a rerun. The tile
was in the development machine's Cesium cache, and stale: EOX's tiles are
cached for a week (`max-age=604800`), and 561 of the cache's 643 EOX tiles
were past it. Cesium Native asks for a stale tile again, with
If-Modified-Since, and on the answer reads
`pCompletedRequest->response()->statusCode()`, having checked only that the
request was there. A fetch that fails - here, given up because the client
was closing - is a request with no response, Cesium Native's own convention
for a failure everywhere else. In a release build that is a segfault; in the
sanitized build, UBSan's "member access within null pointer of type 'const
struct IAssetResponse'" at that line. CI never saw it: its cache is new, so
nothing is stale. The rerun passed because the run before had stored the
tile fresh again (its expiry a week after 12:19 that day).

**Reproduced, every run.** The sanitized client over a copy of the real
cache with every entry made stale, and every fetch refused (a proxy on a
port nothing listens on): 1 of 1 runs ended at that line, exit 1, after four
refused revalidations. With the fix: 33 refused, the frame written, exit 0.

**The fix.** `src/gfx/fetches.cpp` (new): the platform accessor, moved out of
`terrain_tiles.cpp` unchanged, and `cached_fetches`, which puts the cache
over an accessor that answers every GET that came back with no response with
a 504 and nothing in it - the status for a gateway that heard nothing back.
The cache does not store a 504, and every reader above it takes it for a
failed fetch, as it took the missing response: the imagery is drawn on the
DEM's colours, as before.

**Verification.**
- `a_stale_cached_tile_whose_fetch_gets_no_answer_is_answered_as_a_failure_and_ends_nothing`
  (`tests/tools/cache_revalidation.cpp`): the client's own chain asked once
  for each of 2 failures (refused; given up) by 4 cache states (nothing
  cached; stale with an ETag; with Last-Modified; with neither) - 8 of 8
  cases, counted and asserted. Each first shows the network alone answered
  nothing, then that the cached fetch came back 504 and left the stale entry
  as it was. **Seen red** with the 504 layer taken out of `cached_fetches`:
  UBSan's report at `CachingAssetAccessor.cpp:267`, exit 1. Reverted. 0.1 s.
- `a_window_client_whose_stale_cached_imagery_is_refused_when_asked_again_draws_its_frame_and_exits_0`
  (`tests/cmake/imagery_stale_refused.cmake`): the client itself. A shot with
  its imagery refused names the tiles it asked for; each is stored in its
  cache, an hour stale; the same shot again asks for them again, is
  refused, writes its frame and exits 0. It fails if none was asked again;
  not all are, since which tiles a shot asks for follows the order its
  refusals land in (locally 45 of 45, then 44 of 48). 8 s. **Seen red** with
  the 504 layer taken out: "the second shot exited 1", UBSan's report at
  `CachingAssetAccessor.cpp:267`. Reverted.

**CI costs are estimates, not CI measurements.** The chain's test is priced
at 1 s in every table (0.1 s locally). The shot test is priced at
`a_window_client_whose_imagery_cannot_be_fetched_...`'s cost in each table
that lists it (windows-debug at the 66 s measured on the development machine) - locally it took 7.5 to 20.7 s against that test's 11.9 - and
is absent from macos-debug's, which lists no shot test of that kind. They
should be measured again from a green run.

**Built on Windows** (windows-debug, MSVC, `tools/windows_build.sh`, the
WinHTTP path of the moved accessor): both new tests pass there - the chain's
in 19.8 s, the shot in 65.3 s.

**The plan.** The 2026-10-10 item is left open as it was, its link to this
cause not shown; this crash is a ticked item of its own.

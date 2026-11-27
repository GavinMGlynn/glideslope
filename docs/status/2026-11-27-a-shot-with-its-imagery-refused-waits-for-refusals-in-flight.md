### A shot with its imagery refused waits for the refusals still in flight - 2026-11-27, open (time-gated)

**What is still not true first.** The item's last part is time-gated: the
test back on pull requests must pass every CI run for a week. It lands
unticked and can close on 2026-12-04 at the earliest, once a week of CI runs
has been read.

**The cause.** Cesium Native's load progress (cesium-native v0.64.0,
`TilesetViewGroup::finishFrame`) counts the tiles in its load queues, the
tiles kicked, the tiles already loading, and the raster overlay tiles of the
tiles *drawn*. A tile whose geometry has loaded and whose own imagery is in
flight (`RasterOverlayTile::LoadState::Loading`) is in none of those:
`Tile::needsWorkerThreadLoading` asks only about overlay tiles still
`Unloaded`, so it is queued nowhere, and with every fetch refused no
ancestor's imagery has loaded to stand in, so it is not renderable and its
parent is drawn in its place - whose imagery, failed and settled, counts as
attached. Progress reads 100% with a refusal still on its way, the client's
wait for its tiles ended, and the shot was the coarser frame. That is CI's
two failures (runs 40733120388: 24 tiles to level 7 against 26; 40811196271:
21 to level 6 against 26 to 7), both without a give-up: the late refusal,
had it landed, would have wedged and been given up, and the frame drawn
whole. Locally, refusals land fast enough that every run wedged before the
shot, which is why 32 runs here never showed it. A Cesium forum thread
reports the same of a WMS overlay ("ComputeLoadProgress often returns 100
before the tiles have fully loaded").

**The fix** (`gfx/terrain_tiles.cpp`). The shot's wait also waits for
`imagery_in_flight` - the overlay tiles in `Loading` anywhere in the tree -
to be 0. State changes only on the main thread, so a count after an update
is what that update saw. An answer that never finishes (a body a byte at a
time) is waited for 120 s after everything else had loaded and no longer,
said so; the refused-imagery tests fail on that message, since a refusal
answers at once.

**Built every run, not hoped for**: `--test-imagery-late`
(`TerrainOptions::test_imagery_late`, `HeldImagery::late`). The imagery's
first fetches are gathered until everything else stands still; those at the
shallowest level - the topmost tile's - go and fail, and every other tile's
imagery is held in flight until the wait is at 100% without it, when it goes
and the client says "the wait for the tiles was at 100% with N of the
imagery's tiles in flight". Two Cesium Native limits are opened under that
flag only, or the held would be counted and 100% never reached: the
overlay's 20 loading slots (`maximumSimultaneousTileLoads`, 1000) and the
20 not-yet-renderable descendants past which a tile's descendants are taken
out of the load queue (`loadingDescendantLimit`). The earlier attempt
(2026-10-29) held one or eight fetches and hung at 96.7%: those limits were
what held it. New test
`a_window_client_whose_imagery_refusals_come_late_waits_for_them_and_draws_as_deep_as_without_imagery`
(port 24959) runs the refused-imagery shot with the flag and requires the
moment built and the frame as deep as without imagery.

**What the late test does and does not show.** It builds a worse version
of the failure than CI saw, and only with Cesium Native's two loading limits
raised: every tile's imagery in flight at once, where CI's runners had a few.
At the default limits, as shipped, the failure was not reproduced here: that
CI hit this cause at those limits is inferred - from Cesium Native's
progress formula and by ruling out every other way an undrawable tile can go
uncounted - not shown. The setup as shipped is covered by the original
refused-imagery test, back on pull requests at the default limits; the
week of CI runs this item waits for is its evidence there.

**Verified, linux-release.**
- **Red without the fix**: the wait's break put back to Cesium Native's 100%
  alone, the new test failed "its imagery refused, the client drew 1 tiles
  to level 0; without imagery, 26 to level 7" - the client had shot at 100%
  with 79 imagery tiles in flight. Reverted.
- **Green with it**: the held went at 100%, the refusals landed, wedged, the
  imagery was given up, and 26 tiles were drawn to level 7.
- **20 of 20**: `a_window_client_whose_imagery_is_refused_in_whatever_order_still_draws_its_frame_on_the_dems_colours`
  and the new test, 20 runs each (`ctest --repeat until-fail:20 -j2`), all
  passed, each drawing 26 tiles to level 7.
- The imagery, Taranaki, terrain, nightly-list, shard and ports tests, 47,
  pass.

**Back on pull requests.** The refused-imagery test leaves
`tests/cmake/nightly_tests.cmake` (5 names to 4). The new test's CI cost is
left for the next refresh of `tests/ci_costs` to measure.

**Searched before building it**: "cesium-native raster overlay tile loading
computeLoadProgress 100% while overlay tiles still loading" (found the
forum thread above) and "cesium-native RasterOverlayTileProvider failed tile
refinement isRenderable children not refined" (nothing on it); then Cesium
Native's own source, `TilesetViewGroup.cpp`, `Tile.cpp`,
`TilesetSelection.cpp`, `RasterMappedTo3DTile.cpp` and
`QuadtreeRasterOverlayTileProvider.cpp`.

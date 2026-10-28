### The wedged-imagery test builds its wedge, 2026-10-28

**What was wrong.** `a_window_client_whose_imagery_cannot_be_fetched_still_draws_its_frame_on_the_dems_colours_and_says_why`
(2026-10-28-imagery-failure-still-draws.md) requires the client to give its
imagery up, Cesium Native having wedged it. Whether Cesium Native wedges is
the order the refusals land in, and that was left to chance: in CI run
40649190581, Rocky 9 linux-release, tests 2/4, every fetch was refused at
once by curl, no tile wedged, the frame was right (26 tiles to level 7 on
the DEM's colours) - and the test failed at frame_terrain.cmake:121, the
path it pins not taken. A test that only sometimes tests its rule.

**What the wedge needs** (cesium-native v0.64.0,
`RasterMappedTo3DTile::update`): a tile's imagery fails while the root
tile's imagery has already failed and been settled on. Its ancestor walk then
ends on the root's failed tile - not loaded, so not used, not null, so the
tile's own failure is never settled - and the tile stays unready for ever.
Only the root's state matters: the walk keeps the last answer, the root's.
A tile whose failure lands before the root's has settled finds nothing at
the root and settles on its own failure: no wedge. That was run 40649190581.

**Built now.** A test flag, `--test-imagery-wedge`
(`TerrainOptions::test_imagery_coarsest_first`), puts an accessor in front
of the fetches (`CoarsestFirstAccessor` in `gfx/terrain_tiles.cpp`): the
first imagery fetch is the root's, and every fetch deeper than its level is
held, unanswered, while the root's and its coarser fall-backs go at once.
Each update looks at the root tile; once its imagery has settled on failure,
the held fetches go (`let_held_imagery_go`, and the client says so), each
is refused, and each tile's failure lands beneath the settled root - wedged,
every run. The offline wait does not end while fetches are held. The test
passes the flag and requires the client to say both that the held fetches
went and that the imagery was given up.

**And without the wedge.** A second test,
`a_window_client_whose_imagery_is_refused_in_whatever_order_still_draws_its_frame_on_the_dems_colours`
(port 24950), runs the same shot without the flag and without the give-up
requirement: every fetch refused, whatever order they land in, the frame is
drawn, every tile on the DEM's colours, as deep as without imagery, and held
to the DEM's colours by `glideslope_terrain_check`. Its claim holds either
way, so it is not a sometimes-test.

**Verified.**
- linux-release: the wedge test 20 runs of 20 green, each "26 tiles of 26
  drawn on the DEM's colours to level 7, ... refused and given up" (each run
  also requires the held fetches to have gone); the unordered test 5 of 5.
- linux-debug (sanitized): the wedge test 20 runs of 20 green, the same;
  the unordered test 20 of 20 (it wedged on its own every time here, as
  2026-10-28-imagery-failure-still-draws.md found; CI's run did not); the
  ports check 20 of 20.
- **Red with the give-up taken out** (`give_up_wedged_imagery` returning at
  once), linux-release: the client, run as the test runs it, 20 runs of 20
  never finished its shot - stopped at 30 s each (exit 124), no frame, each
  having let its held fetches go and never given anything up; through ctest
  the test timed out at its 300 s. The wedge built is the one that hung
  Windows and macOS (run 40627612271), now on Linux too. Reverted, green.
- Windows (windows-debug, `tools/windows_build.sh` with WINDOWS_TEST): both
  tests and the ports check pass, the wedge test in 106 s, the unordered one
  in 109 s - Windows' slow refusals, as before.
- The ports check passes with 24950 (24951 free too).

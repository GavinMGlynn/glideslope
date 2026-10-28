### A window client whose imagery cannot be fetched still draws its frame, 2026-10-28

**What is still not true first.** With its imagery refused the open terrain
settles a level coarser in a shot (19 tiles, level 6, against 26 at level 7
with imagery off or fetched). Cesium Native walks a tile's failed imagery up
to an ancestor's, and an ancestor whose own imagery was never asked for
holds the finer tile unready, so its parent is drawn. The frame still agrees
with the DEM's ray-cast within the tolerances the shot without imagery is
held to (mean difference 5.02 of 255 against 4.76, at most 5.5). Not chased
further: a degraded mode, and Cesium's.

**The cause.** At a shot the client threw "N terrain tiles were drawn
without their imagery" before writing the frame, and exited 1. A tile whose
imagery fetches failed - EOX resetting connections on CI's Linux (run
40618052854, `a_pressed_during_a_take_over_hands_the_aircraft_taken_to_the_ai`),
or refusing them under WinHTTP 12030 on windows-clang (run 40603515425) -
was counted as one without imagery, so the test's client "drew nothing".
And such a tile was drawn grey: with imagery on, its vertex colours were the
slope's light alone, for the imagery to be multiplied by.

**The fix.** With imagery on, each open terrain tile's glTF carries the
DEM's colours too (`_DEM_COLOUR`), kept on the CPU beside the tile - about
45 kB a tile, kept even once imagery attaches, because the imagery can be
given up whole (below) and taken off tiles that had it; freeing it on attach,
as the review asked, was done and undone for that reason. When it settles a tile's
imagery on a raster tile that failed, with none of the tile's overlays still
loading and no ancestor's to stand in (`imagery_failed` in
`gfx/terrain_tiles.cpp`), the tile's DEM-coloured meshes are put on the GPU
then, and drawn instead; `Counts::imagery_failed` counts them apart from
`without_imagery`, which still refuses a shot. The client says on its
standard error, once while flying, that imagery could not be fetched, and at
a shot how many tiles were drawn on the DEM's colours and why. The imagery's
credit is left off when every tile drawn is on the DEM's colours - none of
EOX's is on screen - and kept whenever any of it is.

**The test.** `a_window_client_whose_imagery_cannot_be_fetched_still_draws_its_frame_on_the_dems_colours_and_says_why`
(`tests/cmake/frame_terrain_imagery_refused.cmake`, through
`frame_terrain.cmake`): the Taranaki terrain shot with `--imagery on` and
`--test-imagery-url http://127.0.0.1:24945/...`, a port from the tests'
block that nothing listens on, so every fetch is refused - built, not hoped
for. It must exit 0, write its frame, say "19 terrain tiles were drawn on the
DEM's colours: their imagery could not be fetched" for every tile the frame
holds (19 of 19), show a refused fetch, and pass `glideslope_terrain_check`
against the DEM's colours. Seen red before the fix: exit 1, "19 terrain
tiles were drawn without their imagery", no frame. Green after, mean
difference 5.02 of 255. **The DEM-colour path seen red on its own**: with the
failed tiles counted but drawn from their grey, light-only meshes - a
deliberate bug, reverted - the test failed on the terrain check, mean
difference 127.24 of 255 (at most 5.5). **The credit**: the check reads the
credits back off the frame and holds them to the DEM's notice alone; with
EOX's credit still drawn, an earlier version of this change failed it
("the credits, line 1, read ...").

**Tests about the imagery itself** (`a_shot_of_mount_taranaki_drapes_the_open_imagery_where_it_belongs`,
`a_headless_client_drawing_ten_thousand_frames_keeps_its_memory_level`) now
exit 0 with failed imagery, and so skip on the client's words
(`glideslope_skip_when_imagery_not_fetched` in `client.cmake`) as they
skipped on its exit code before - or fail, with GLIDESLOPE_REQUIRE_NETWORK.

**Verified** (linux-debug, sanitized): the new test, both Taranaki shots,
the ten-thousand-frame memory test, the three ride-along tests (the one CI
failed among them) and the ports check, 9 of 9, after the review's round
too (rebased on main; the item is main's own Phase 12c tail, ticked there).

**The shot waited for ever on Windows and macOS** (full run 40627612271:
the new test timed out at 900 s on windows-clang, windows-release,
windows-debug and macos-release; Linux passed). **Cause, found by printing
every loaded tile's imagery state from inside the wait on the Windows
machine**: Cesium Native (v0.64.0, and main as of today) wedges a tile whose
imagery failed beneath an ancestor already settled on failed imagery.
`RasterMappedTo3DTile::update` finds that ancestor's failed raster tile as
its "closest ready ancestor" - not loaded, so not used, and not null, so the
tile is never marked failed itself - and leaves it unready for ever: never
renderable, so the root alone was drawn (1 tile selected, 48 kicked), and
the load stuck at 44.2%, which `updateViewGroupOffline` waits on. Whether it
happens is the order the failures land in: WinHTTP takes about 2 s to be
refused by a closed loopback port (12029), and each imagery fetch falls back
up the levels one refusal at a time, so the coarse levels' failures settled
first; on Linux curl is refused at once and every tile settles in the same
pass. Same fetches both ways (50, and 150 failures of 0/0/1 logged by
Cesium). Not reproduced on Linux by delaying every refusal 2 s in a stub, nor
by failing level 0 first.

**The fix**: the wait for every tile is now this code's own loop, the same
as `updateViewGroupOffline`'s, and every update looks for a wedged tile
(`imagery_wedged_below`). Finding one, it takes the imagery overlay off the
tileset for good (`give_up_wedged_imagery`), says so on standard error, and
draws every tile on the DEM's colours; the wait then ends. A wedged tile
cannot be freed alone - nothing outside Cesium can reset an ancestor's
settled failure - so all the imagery goes; it had failed down to its
coarsest level. **Seen on Windows** (windows-debug, `tools/windows_build.sh`
with WINDOWS_TEST): the new test passes in 89 s; run by hand it says the
imagery is given up and draws 26 tiles to level 7 on the DEM's colours.
**What covers macOS** is the same code: the wedge is Cesium's and timing,
not the HTTP layer's, and the loop and the give-up are platform-free; only
CI shows it there. **Not covered**: on Linux the new test never wedges, so
it pins the per-tile path there and the give-up only on Windows and macOS.
Linux after the change: the 9 related tests pass, the imagery-draped
Taranaki shot among them (nothing given up where the imagery comes).

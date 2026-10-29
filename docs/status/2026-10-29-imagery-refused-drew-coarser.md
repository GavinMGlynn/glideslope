### A shot with its imagery refused drew coarser than one without, twice - 2026-10-29, open

**What is still not true first.** The cause is not found, and nothing is
fixed. The failure has been seen twice, so the test runs nightly and on no
pull request until the cause is fixed.

**What was seen.** In #264's full run 40733120388, Rocky 9 linux-release,
`a_window_client_whose_imagery_is_refused_in_whatever_order_still_draws_its_frame_on_the_dems_colours`
failed (frame_terrain.cmake, line 171 on main then): "its imagery refused, the client drew 24
tiles to level 7; without imagery, 26 to level 7". That run had no give-up
line: Cesium Native did not wedge.

Again in #268's full run 40811196271, Ubuntu linux-release, the same test at
frame_terrain.cmake line 195 (on #268's branch): "its imagery refused, the
client drew 21 tiles to level 6; without imagery, 26 to level 7", the client
saying "terrain of 21 tiles, 83 loaded, the deepest at level 6". #268's hold
was in that build; the first failure was before it, so the hold is not shown
to be the cause, nor shown not to make it worse.

**What two hours of looking found** (an agent's, time-boxed, on draft #266):

- Cesium Native's load progress counts tiles queued, not the imagery tiles
  of an overlay still loading; the open provider's shot ends at 100% with no
  wedge, nothing given up and nothing held. So imagery can still be in
  flight when the shot is taken.
- Counted at the shot without any fix, linux-release: 20 runs one at a time
  all gave up with 0 loading; 12 runs four at once on two cores, 11 gave up
  with 0, and one did not give up and shot with 13 imagery tiles loading -
  and drew all 26 tiles to level 7. In-flight imagery is real, and not shown
  to be what drew CI's two coarser tiles.
- The "20 loading" seen through a run are real fetches working down the
  coarser levels; they finish, or the give-up takes the overlay off.
- A deterministic hook (`--test-imagery-late`, holding one or eight fetches
  in-process) could not build it: Cesium Native shares a fetch between
  tiles, so holding one stalls about three, their parents are queued again,
  and progress sat at 96.7% - every run hung.

**Not landed.** A wait for every overlay's loading tiles to be 0, bounded at
120 s and saying what it waited for, with the count printed at the shot and
held to 0 by the test: written, never seen to fix anything (no red), so not
landed - a fix nobody has seen matter is a guess. The work in progress was
kept outside the repository, in an untracked scratch directory
(`fix-and-late-hook-wip.diff` and `debug-hooks.diff`), and may not survive.

**What happens next.** Its second failure for this cause moved it to the
nightly run, named in `tests/cmake/nightly_tests.cmake` with both runs, as
CLAUDE.md asks of a test that failed on CI twice for one cause: a nightly
failure of it is reported in its plan item, not a red tree. The shard
check (every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once)
passed on a linux-release build of this commit. It comes back to pull
requests when the cause is fixed. The hold #268 lands - a fetch let go when
nothing else is in flight, and everything after 120 s - may be the base the
deterministic late-imagery hook lacked.

**Counted for the verification.** In flight at the shot: 1 of 32 runs here
(13 tiles); drawn coarser: 2 runs on CI (40733120388, 40811196271) and none of
the 32 here.

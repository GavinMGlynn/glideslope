### The wedged-imagery test holds its fetches whatever order Cesium asks them in, 2026-10-29

**What was wrong.** `a_window_client_whose_imagery_cannot_be_fetched_still_draws_its_frame_on_the_dems_colours_and_says_why`
hung now and then: on CI's Windows debug (run 40686462655), once on the
Windows development machine, and on CI's macOS release (run 40743161009,
tests 2/3), where the shot's own limit caught it - "the shot was still
running after 240 s". Its hook, `--test-imagery-wedge`
(2026-10-28-imagery-wedge-built.md), took the first imagery fetch to be the
root tile's - "assumed, not checked" - and held every fetch deeper than it
until the root's imagery had settled. When a fetch coarser than the root's
came first, the root's own fetches were among those held: the root's imagery
could never settle, nothing was let go, and the shot's wait, which does not
end while anything is held, never ended. The macOS client said neither that
the held fetches went nor that the imagery was given up.

**The hold now** (`HeldImagery` and `CoarsestFirstAccessor`,
`gfx/terrain_tiles.cpp`):
- A fetch is held when it is deeper than the shallowest asked for so far,
  unless it is the parent of one let through - the fall-back Cesium Native
  asks for when that one fails - so a chain of fall-backs, once started,
  runs to its end. The order Cesium Native asks in no longer matters to
  what is held.
- What is held goes all at once when the root's imagery has settled, as
  before; **a level at a time, coarsest first**, when nothing let through
  has been in flight for 2 s and the root's has not settled - the root's
  own may be among the held, or waiting for one of Cesium Native's 20
  loading slots the held fill; and **all at once 120 s after the first
  imagery fetch**, said so ("the root tile's had not settled 120 s after the
  first was asked for, so no wedge is promised"). The test, which requires
  the give-up, would then fail on that and not hang. Nothing is held for
  ever.
- The client says what went and why, each time: the level released early,
  or how many were held when the rest went.

**Made to happen every run.** A second test flag, `--test-imagery-out-of-order`
(`TerrainOptions::test_imagery_out_of_order`), gathers the imagery's first
fetches unanswered until none has come for 2 s, then asks a level-0 fetch of
its own first - coarser than the root's - and the gathered deepest first. A
new test, `a_window_client_whose_imagery_is_asked_for_out_of_order_still_builds_its_wedge_and_gives_it_up`
(port 24958), runs the wedge test's shot with it and requires the same as
the wedge test, plus that the order was built. Asked deepest first alone,
without the level-0 fetch, the old hold did not hang (tried: its first fetch
was at level 12, and the root's were coarser, so not held) - a deep fetch
first is not what breaks it; a coarse one is.

**Verified.**
- **Red on the old hold**: its rule put back (first fetch's level, no
  early or bounded release) with the out-of-order flag, linux-release: the
  client by hand never finished its shot, stopped at 240 s (exit 124),
  having said only that its fetches were gathered; through ctest the new
  test failed "the shot was still running after 240 s" - the macOS failure,
  every run. Reverted.
- **Green on the new**, the same order: the client let level 8 go, then
  level 6, then "the root tile's has failed (18 held)", gave the imagery up,
  and drew 26 tiles to level 7 on the DEM's colours.
- **The bound seen working**: with the level-by-level release taken out,
  the same run let everything go at 120 s, saying so, and finished. Reverted.
- linux-release: the wedge test 20 runs of 20 green and the out-of-order
  test 20 of 20, each requiring the held fetches to have gone and the
  imagery to have been given up (262 s for the 40); the ports check passes
  with 24958 (24959 free too).
- linux-debug (sanitized): the wedge test 20 of 20 and the out-of-order
  test 20 of 20, the same (576 s for the 40).
- Windows (windows-debug, `tools/windows_build.sh`, WINDOWS_TEST=imagery):
  the wedge test passed in 126 s, the out-of-order test in 107 s and the
  unordered one in 118 s. In the same run both
  `a_shot_of_mount_taranaki_drapes_the_open_imagery_where_it_belongs` tests
  failed, each after its shot was written: `glideslope_terrain_check` was
  still running at its 240 s step limit. Neither passes the wedge flag, so
  neither has a hold. **The limit was too short, not the check hung**
  (below).

**Each step's limit is now 600 s, not 240** (`_step_limit_s` in
`tests/cmake/frame_terrain.cmake`, from #262). It is there to name a hang,
not to bound a slow machine: CI's Windows debug runs the whole
imagery-drape test in 129 to 153 s, and the Windows development machine's
terrain check alone went past 240 s. Each test's TIMEOUT is above its
steps' limits together. The three refused-imagery tests run three steps, so
900 s becomes 1900. The two Taranaki shots run two steps; they had the
presets' 900 s, which two steps of 600 do not fit under, so they have 1300.
Verified: Windows debug (`tools/windows_build.sh`, WINDOWS_TEST
"taranaki|imagery") ran both Taranaki shots and the three refused-imagery
tests on both drivers, and all passed. The drape tests took 259 and 265 s.
The terrain check alone, run by hand on the shot left behind with the
machine idle, took 198 s. Under the suite's load it had gone past 240 s. In
the same run `a_parent_imagery_tile_scaled_into_a_missing_childs_place_is_sound`
printed its pass and then died at exit (0xc0000409; an access violation in
a thread "after the program began to exit"). It had passed in the first
run, and nothing here touches it. It is not chased here. linux-debug: the
same seven tests all passed (94 s).

**The crash on the Windows machine** - a window client's null read on a
worker thread while every imagery fetch timed out, in a take-back test run
by another agent - **is not this hold's**: `HeldImagery` and
`CoarsestFirstAccessor` exist only with `--test-imagery-wedge`, which only
the two wedge tests pass. Read for it all the same: the held fetches keep
their accessor and Cesium Native's `AsyncSystem` by value and shared
pointer, not by reference; the destructor lets everything go before the
tileset is destroyed, and those fetches then fail at once on `closing`;
no fetch is passed on while the hold's lock is taken. No lifetime bug was
found, and the crash's cause is not found here.

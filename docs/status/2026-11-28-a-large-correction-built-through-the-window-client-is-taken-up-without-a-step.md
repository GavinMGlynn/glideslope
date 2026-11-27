### A large correction built through the window client is taken up without a step, 2026-11-28

**What is still missing first.** Nothing of the item's verification; it
has not yet been seen on CI's release runners, where the test runs on pull
requests (debug presets leave `timing` tests to main and the nightly). What
cannot be had: whether the old steps CI saw (6.2 and 24.5 m in the hand-over
test, 2.77 m at a take-over, all 2026-09-29) were the session's clock. Their
runs' lines are gone, and the reading below is the most the remaining record
says.

**What changed.**
- **The server builds a large correction for a test**
  (`--test-move-taken-back S M`, `src/frontend/server/main.cpp`): S simulated
  seconds after a player takes her aircraft back from the AI, the server
  moves it M metres up (along the line from the Earth's centre), once each
  take-back, and says "aircraft N moved M m up, for a test". It touches only
  the aircraft's position (`sim::Aircraft::set_motion`). In play the server
  moves no person's aircraft; this is a test hook, refused outside 0 < M ≤
  100, and `server_flags.cmake` now knows it.
- **The window client's shot waits for it** (`--shot-once-corrected M`): up
  to a minute of the flight past its tick, until a correction of at least M
  metres has been heard and a second flown since, so that the frames taking
  it up are measured. Events, not the clock.
- **The hand-over test bounds the step away from a switch**
  (`the_client_with_the_window_hands_its_aircraft_to_the_ai_on_a_server_and_takes_it_back`,
  `tests/cmake/client_hands_over.cmake`). Three seconds after the take-back
  the server moves the aircraft 15 m up. The test asserts:
  - the server moved it exactly once;
  - the shot came after a correction of 10 m or more and a second since;
  - the client's largest correction was at least 10 m, twice the bound, and
    none was too large to hide (`sim::snap_beyond_m`, 20 m) - so a run in
    which the blend was missing would have gone red: a correction shown as
    it came steps its own size;
  - the largest step away from a switch is under 5 m, the bound the test
    already holds at a switch and the network checks hold.
  It writes its figures and what made the largest step to `figures.txt` in
  its work directory, which CI prints for every run, passed or failed.

**Verification** (linux-release, by hand and by ctest):

- Two runs passed, each with the server's move heard as a correction of
  15.000 m and none too large to hide: the largest step away from a switch
  1.026 m and 1.002 m (the second "frame 173 after a switch, 18 ms long,
  predicted: 0.050 m from where it was carried at 55.4 m/s, the blend moving
  1.000 m" - the move itself, a fifteenth of it a sixtieth of a second); at
  the switches 0.457 m and 0.499 m. 143 s from a cold build, 25 s after.
- `every_flag_the_server_prints_in_its_usage_is_one_it_takes`: 59 flags.

**Seen to fail.** With the correction blend taken out of `OwnShown`
(`if (switched && carried)` in `src/frontend/shown.cpp`, flagged corrections
no longer blended):

failed in 26 s with "what the client showed stepped 15.000 m away from a
switch, the bound 5 m", the step "frame 173 after a switch" - the move,
shown as it came.

Reverted, it passes.

**The old steps, and the clock.** Read against what was found since:
- The 6.2 m step away from a switch (main's CI, Linux debug, 2026-09-29)
  came with 349 corrections, the worst 6.093 m, none snapped. On 2026-09-30
  the client was found dropping every tick past 24 a pass, and at 250 ms
  passes each pass's first correction was then 5.0 to 5.4 m, unflagged as
  the prediction fell behind. That is the 6.2 m run's shape, in predicted
  frames, which the session's clock does not touch. It was fixed then.
- The 2.77 m at a take-over came the same day; on 2026-09-30 a take-over was
  found put where a stale word said, a pass behind, which at 250 ms passes
  stepped 1.1 m and at 1.3 s passes 8.5 m. Also fixed then.
- The 24.5 m came with nothing but its size. The clock's steps reach that
  only drawn from the updates, between the hand-over and the take-back, and
  the clock's own steps built on their own reached 7 m at passes of up to
  700 ms (2026-10-09). So it is not shown to be the clock, nor shown not.
- So: two of the three fit causes fixed on 2026-09-30, not the clock; the
  third cannot now be read. Every cause found has been fixed (the dropped
  ticks and the stale take-over on 2026-09-30, the clock slewed on
  2026-10-09), and the step away from a switch is now bounded on every run,
  with its cause kept in `figures.txt`, so a step like those will come with
  its lines.

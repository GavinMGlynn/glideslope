### The window client draws at a clock that does not step; the take-back test takes the aircraft back on the event, 2026-10-09 — the item stays open

**What is still missing first.** No test builds a large correction through
the client itself, and the hand-over test's step away from a switch is still
held to no bound seen to fail; and the steps CI saw before this (6.2 and
24.5 m in the hand-over test, 2.77 m at a take-over) have no lines left to
say whether they were this clock. "The client with the window does not blend
corrections" stays `[ ]`. The command-line client's display model still draws
at the fitted clock: it reads its updates in short loops, not a long pass,
and its bounds have held; it is not changed here.

**Fixed: the clock the window client draws at stepped.** `net::ShownClock`
(net/interpolation.*) follows `net::SessionClock` without stepping: each frame
it moves on at the rate it was moving - the fitted rate plus a pace chosen at
the end of the frame before - and only then chooses its pace again, toward the
fit: the difference over 1 s, at most a tenth of real time either way, never
carried past the difference it aimed at. `rate()` counts the pace, and
`client::Online::others` draws the others - and its own while the AI flies it -
at it, their path velocity at that rate, so an aircraft carried on at its
drawn velocity is where the next frame draws it. **The numbers**: a tenth,
because an aircraft drawn a tenth fast or slow for a moment is not seen as a
jump, and a pass's worth of the fit (700 ms) is still taken up in seven
seconds; 1 s to settle, longer than any frame drawn, so a pace does not
overshoot; a difference past 0.5 s - joined again, a server stalled and caught
up, larger than any pass - is taken at once, since slewed it would draw every
aircraft seconds wrong for most of a minute. One within it settles to a
millisecond within 12 s (5 s at a tenth, and the last tenth of a second
`ln(100)` seconds more).

Tests (tests/unit/test_interpolation.cpp):
- `the_clock_aircraft_are_drawn_at_does_not_step_under_long_uneven_frames`:
  the table below, five seeds each - 20 runs of a minute, asserted all run -
  each frame's step held to a tenth of the frame (what the slew can move in
  it: 2.9 m at 42 m/s and 700 ms) - measured 0.00 ms in every case, which is
  near circular, since the step is measured against the clock's own `rate()`
  and that counts its pace. **So it is judged from outside as well** (from
  the review): over every frame, from two drawn times, the clock drawn at
  must go at the fit's rate within the slew's tenth (at worst 0.006, 0.018,
  0.100 and 0.100 off it, by kind of pass); and it must stay within the
  longest pass of the fit, how far a pass can put the fit out (at most 16,
  27, 107 and 256 ms off it) - not 0.5 s, which the snap holds whatever
  happens and which would say nothing. **Seen to fail three ways**: with the
  fitted clock drawn at instead (`clock.now`, `clock.rate()`), first "with
  passes of 17 ms the clock drawn at stepped 6.009 ms in a frame, 35.3% of
  it", and with the outside checks in, "the clock drawn at went at 0.694 of
  real time over a frame where the fit went at 0.999"; with the slew's cap
  taken out (`pace_ = difference / settle_s`), "went at 0.706 ... where the
  fit went at 0.920". Each reverted, it passes.
- `a_real_change_of_the_sessions_clock_is_followed_within_its_stated_time`:
  the latency changed 20 s in by +0.3 s, -0.3 s and +2 s: settled to a
  millisecond in 5.68, 6.90 and 9.32 s against 12 s; the 2 s change snapped
  once, the others never.

**Through the client itself** (linux-release, by hand,
`client_hands_over.cmake` with `SLOW_FRAMES`, which holds a pass that much
longer after each tenth of a second at full speed - uneven frames; the frame
guard then fails the run as not testing its bound, as it should, and the
figures are read from what it printed): the largest step away from a switch,
drawn from the updates, "from where it was carried" -

| passes held | fitted clock (before) | slewed clock (now) |
|---|---|---|
| 300 ms | 0.799 m (310 ms frame, 41.8 m/s) | 0.126 m (314 ms, 39.8 m/s) |
| 600 ms | 1.625 m (616 ms frame, 37.6 m/s) | 0.521 m (612 ms, 39.3 m/s) |

One run each, through a relay of 100 ms and no jitter: smaller than CI's
5.4 m, where the runner's network and frames were rougher, but the same
kind, and down by two thirds and more.

**The copilot test's 5 m bound at a switch is not tightened**: CI's 5.683 m
there was 5.414 m of this clock and 0.269 m of the blend, so with the clock
slewed it would have been about 0.3 m - but that is one run's arithmetic, not
a measurement on the runner that failed, and a bound is changed on evidence.

**The cause of the steps drawn from the updates.** CI run
37866144089 (PR #143, windows-release, attempt 1) failed
`the_client_with_the_window_asks_its_copilot_and_the_server_flies_its_route`
at 5.683 m against 5 m, and its own lines say what made it: "frame 1 after a
switch, 329 ms long, drawn from the updates: 5.414 m from where it was carried
at 42.3 m/s, the blend moving 0.269 m"; the largest step otherwise was the same
kind, "frame 5 after a switch, 70 ms long, drawn from the updates: 3.330 m ...
at 36.4 m/s, the blend moving 0.000 m"; the prediction's own corrections were
small (median 0.000 m, the worst 0.400 m once its clocks' difference was
known). So it was not the blend nor a correction: what is drawn from the
updates moved 128 ms (and 92 ms) of flight more or less than its own path
velocity said - **the session's clock it is drawn at stepped**. Why:
`Online::listen` hears every update that arrived during a pass at the pass's
own time (`arrived_s_`), not when each arrived, and `net::SessionClock` takes
its offset from the update that "waited least" over the last second. With
long, uneven passes - CI's software renderer draws 150 to 330 ms - which
update looks least delayed changes as the best one ages out of that window,
and the offset, so the clock, jumps by up to a pass. Built on its own (a
scratch program, not kept, over `net::SessionClock` alone: updates at 30 Hz,
100 ms latency, 10 ms jitter, every third pass long, five seeds): passes of
17 ms step the clock 1.3 ms at worst (0.05 m at 42.3 m/s); fixed 329 ms passes
16.8 ms (0.71 m); passes of up to 400 ms at random 88.9 ms (3.76 m), and up to
700 ms 165.5 ms (7.00 m) - CI's size. Predicted frames do not use that clock,
and the steps CI saw at a take-over (2.77 m) and in the hand-over test (6.2
and 24.5 m) cannot now be read against this: those runs' lines are gone.
Stamping each update with when it arrived was not open: they are read a
pass at a time, and only the socket's own timestamps could say when within
it each came, so the clock is slewed (above).

**`others()` moves the clock on, so it is called once a frame**; the
watched aircraft's controls are read at the same clock without moving it
(`ShownClock::peek`). `session_now_s_` - the time the server's air and the
prediction are on - stays the fit's: the two are apart by no more than the
longest pass, and never past 0.5 s, where the clock drawn at snaps to the
fit. **A snap is a step, and may go backwards** - a latency risen by two
seconds, a server started again - which the header says.

**Fixed: the take-back test only sometimes tested its rule.** CI run
37866144089's windows-debug job failed
`a_players_copilot_stands_by_when_its_pilot_takes_the_aircraft_back_as_recorded`
with "no route came after the take-back, so the rule was not tested". The
player took the aircraft back 40 s in, and the routine look that should have
been out then is asked 20 s after the first answer, which is taken 10 s after
the question at 5 s - 35 s, with 5 s to spare. A first answer later than 20 s
- its copilot's ground (a geoid, the DEM, the world's runways) made on a slow
debug machine - put the look after the take-back, and nothing came after it.
Now the client takes the aircraft back on the event, not the clock:
`glideslope_cli connect ... --take-back-while-looking` takes it back as soon as
its copilot has a routine look out (`frontend::PlayersCopilot::looking`), and
an answer cannot be taken sooner than `thinking_s`, 10 s of the session's
clock, after it was asked - so one always comes after the take-back. The test
passes `TAKE_BACK_AT=looking` (tests/CMakeLists.txt,
tests/cmake/server_copilot.cmake); `--take-back-at S` is kept. A look out
is a flag set when the routine look is asked, not the words it was asked
with. The flag refuses, exit 2, without a `--copilot` task and a
`--copilot-routine`, and a run that never had a routine look out to take it
back during ends 1, saying so (that path not exercised by a test).
`--copilot-thinking S` (for tests; `THINKING` in the script, by hand) sets
how long an answer is taken after its question.

**Verified** (linux-release): the test passes (84 s), its client saying "asked
its copilot, a routine look", then "its pilot has taken it back: the copilot
stands by", then "its copilot answered with a route of 1, and was not heard";
the server handed the aircraft to the AI once. **Seen red** (by hand, each
reverted): the old timed take-back (`TAKE_BACK_AT=40`) with every answer
taken 21 s after its question (`THINKING=21`, a first answer at 26 s, as a
slow machine's) failed with CI's own words, "no route came after the
take-back, so the rule was not tested", while `looking` with the same delay
passed; and `looking` with the standing by taken out (`engaged_` left set in
`taken_back` and in `look`) failed both of two runs, its client sending "its
copilot's route of 1 at 45 s", after the take-back.

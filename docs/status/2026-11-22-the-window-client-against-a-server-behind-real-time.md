### The window client against a server behind real time is held to the command-line client's bound, 2026-11-22

**What is still missing, first.** The plan item stays open. The first-input
test and the F-22A's still run alone (RUN_SERIAL); nobody has yet seen them
pass on CI with their server held up. That needs them taken out of serial
and watched over several CI runs, which is not done here. Windows' two-step
placements (1.1 m, one update in a hundred) are owned by the item "On
Windows the server's word is placed two steps off now and then". The new
tests' figures on Windows and on CI's runners are not yet measured: they
are measured by this pull request's CI run. The worst single update is
still one step's travel, 1.4 to 1.8 m for the F-15C. The bound, as
glideslope_cli's, is on the 95th percentile.

**Measured first, on main (Linux debug).** The window client
(`client_on_server.cmake`, F-15C, flying for 4,800 ticks) no longer showed
the 6 to 9 m and 10 to 13 m of 2026-10-08:

| server | corrections once the difference was known | prediction error: median / 95th / worst |
|--------|-----|-----|
| 0.6 of real time, stick still | 0.000 m | 0 / 0 / 0 m (964 updates) |
| keeping real time, stick still | 1.381 m | 0 / 0 / 1.378 m (974) |

**Why it was not seen before.** The window client's tests fly a stick held
still, so every input is the same. glideslope_cli's paced test says why
its own pilot keeps moving: with every input the same, a prediction that
flies each input for the wrong number of steps is still right. Flown with a
pilot whose controls keep changing (the same as glideslope_cli's):

| server, window client | median / 95th / worst | a metre or more | pace |
|----|----|----|----|
| 0.6, paced | 0 / 0 / 1.461 m (966 updates) | 4 | |
| 0.6, unpaced | 4.541 / 8.387 / 19.147 m (573) | 526 | |
| 1.0, paced | 0 / 0 / 0 m (974) | 0 | |
| 0.6, paced, through ctest | 0 / 0 / 1.807 m (969) | 6 | 0.600 |
| 1.0, paced, through ctest | 0 / 0 / 0 m (974) | 0 | 1.000 |

So pacing holds the window client at the server's pace, and the old
figures are gone. **The old figures were not bisected.** They came before
2026-10-24, when the window client began comparing every word at its own
step and steering its pace by every word, not each frame's newest. Before
that, a word about a step not yet flown, if it was the frame's newest, put
the aircraft at the server's word ahead of the steps counted. That is the
likely cause, but it has not been shown.

**What changed.**
- `frontend/test_pilot.hpp`: the test pilot glideslope_cli flew
  (`Predicting::pilot`), now shared. glideslope_cli calls it, with the same
  numbers.
- Window client: `--test-pilot` sends that pilot's controls in place of the
  stick (`Online::fly_test_pilot`). `--unpaced` flies its ticks at this
  machine's pace (the test's red). At the shot it says the prediction
  error's 95th percentile, how many updates were a metre or more, and
  `paced: flown at P of this machine's clock at the end`.
- `client_on_server.cmake`: `-DSHOT_AT`, `-DTEST_PILOT`, `-DUNPACED`, and
  `-DPREDICTED`, which holds the error to glideslope_cli's paced bounds (95th
  percentile 1 m, worst 3 m, median 5 cm, 600 updates at least) and, with
  `-DSERVER_PACE`, the pace flown within 2 percent of it. The client's
  output is kept in `WORK/said.txt`.

**Verification.**
- `a_window_client_predicting_against_a_server_behind_real_time_flies_at_its_pace_and_is_off_by_centimetres`
  (port 24942, server at `--test-pace 0.6`) and
  `a_window_client_predicting_against_a_server_keeping_real_time_is_off_by_centimetres`
  (port 24944), both RUN_SERIAL, costed at 200 and 160 s until
  tests/ci_costs measures them: passed, 101 and 76 s, Linux debug.
- **Seen red**: the paced one with `-DUNPACED=ON`: "its prediction was off
  by more than its bounds: 573 updates compared, the worst error 18043 mm,
  the 95th percentile 7966 mm, the median 4547 mm". The keeping-time one,
  with the client's ticks flown 2 percent fast (`gap_s * 1.02`, put in,
  built, run, taken out): 95th percentile 1574 mm, worst 1859 mm, 87 a
  metre or more.
- The window, paced, prediction, first-input, inputs and behind-real-time
  tests, with the flag and port checks, Linux debug, `-j4`: 113 of 113
  passed, two skipped for want of a display (the nightly label left out).
- **Selftest hash** 27e94c795b47141c, unchanged: the selftest does not fly
  the test pilot.

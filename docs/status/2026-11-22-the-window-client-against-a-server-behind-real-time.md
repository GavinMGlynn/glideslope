### The window client against a server behind real time is held to the command-line client's bound, 2026-11-22

**What is still missing, first.** The plan item stays open. The first-input
test and the F-22A's still run alone (RUN_SERIAL); nobody has yet seen them
pass on CI with their server held up. That needs them taken out of serial
and watched over several CI runs, which is not done here. Windows' two-step
placements (1.1 m, one update in a hundred) are owned by the item "On
Windows the server's word is placed two steps off now and then". The new
tests' figures on CI's runners are not yet measured in the C172P they now
fly. The bound, as glideslope_cli's, is on the 95th percentile.

**From train #400's run (44934787992): the F-15C was flown into the
ground.** On Ubuntu linux-release the paced test failed: one update 7,064 m
off, the client put right 7,080 m, "too large to hide". At the shot the HUD
read 300 kt at 4,928 ft, wings level: the F-15C's start. The test pilot's
controls (aileron 0.3 sin 1.3t, elevator 0.05 sin 0.9t) had flown her into
the ground. A wreck stays where it hit for 5 s of the server's time
(`wreck_s`) and then flies again from her start, about 7 km back along her
40 s. The client took that word as an ordinary correction. In the debug
runs here she was a wreck too: SPD 0 KT, 15 ft at the shot, 122 words of
about 966 saying so. The respawn simply came after the shot. So part of
every F-15C figure below is a wreck sitting still, whose error is nought.

**Fix.** The two tests fly the C172P (`-DAIRCRAFT=c172p`), the aircraft
glideslope_cli's paced test flies with the same pilot. The window client now
says how many words on its own aircraft said it was a wreck. PREDICTED fails
on any: a prediction test must fly, not sit as a wreck. The bound is not
loosened.
- Seen red: the paced test in the F-15C, as before the fix: "its aircraft
  was a wreck in 122 words".
- In the C172P, Linux debug through ctest: 0 words a wreck. At 0.6, median
  0, 95th 0, worst 0.632 m (966 updates). Keeping time, 0 / 0 / 0.509 m
  (972). Every update under a metre. At the shot she was at 140 kt and
  3,599 ft.
- In the C172P on the Windows machine (tools/windows_build.sh,
  windows-debug): both passed, 255 and 128 s, 0 words a wreck; 0 / 0 /
  0.599 m (967) and 0 / 0 / 0.522 m (974).
- Unpaced at 0.6 in the C172P is still red: 95th 2.154 m, worst 6.521 m,
  median 1.224 m.
- **The keeping-time test's bounds are not seen red in the C172P.** Flown 2
  or 5 percent fast against a server keeping time, its 95th percentile was
  0.48 and 0.52 m. Its ticks flown on the input sent in the same pass, or
  flying the stick while sending the pilot, gave 0 m and 5 mm: each word
  puts the prediction right, and 40 ms between words is too short to part
  far at 55 m/s. Its wreck guard is seen red, and its bounds are the paced
  test's code, seen red there.
- COST is now a release runner's seconds: 25 and 12, Windows debug's 273
  and 121 s over twelve. Given as debug seconds, run 44934787992 dealt them
  as an hour with the night's repeats.

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
  (port 24944), both RUN_SERIAL: in the F-15C, before the fix above, passed, 101 and 76 s, Linux debug; on the
  Windows machine (tools/windows_build.sh, windows-debug) both passed, 273
  and 121 s.
- **The F-15C's figures, median / 95th / worst** (the bounds: 5 cm / 1 m /
  3 m), each run ending as a wreck:

  | run | 0.6 of real time | keeping real time |
  |-----|------------------|-------------------|
  | Linux debug, first | 0 / 0 / 1.807 m (969) | 0 / 0 / 0 m (974) |
  | Linux debug, after the review | 0 / 0 / 0 m (960) | 0 / 0 / 1.833 m (972) |
  | Windows debug, first | 0 / 0 / 1.829 m (968) | 0 / 0 / 1.653 m (974) |
  | Windows debug, after the review | 0 / 0 / 1.836 m (966) | 0 / 0 / 1.572 m (972) |

  The worst is always one step's travel, 1.4 to 1.8 m. At most 9 updates in
  about 970 were a metre or more, and 49 would move the 95th percentile, so
  that bound has a wide margin. The worst is 1.2 m under its 3 m bound. No
  `timing` label: the held-a-second sibling has one because its long frames
  need a release build's quick frames between them, and these frames are
  not held. CI's figures are in this pull request's runs; ctest prints the
  "its prediction" line only on failure there.
- **From the review of #398.** The window client's 95th percentile is now
  taken as glideslope_cli takes it, ceil(0.95 n) - 1. `--unpaced` also
  stamps its sent inputs at this machine's pace. The count of updates a
  metre or more is printed, not judged.
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

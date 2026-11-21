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
about 966 saying so. The respawn simply came after the shot. So
every F-15C test-pilot figure first reported here included a wreck sitting
still, whose error is nought (withdrawn below).

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
  far at 55 m/s. **Seen red from the re-review:** with each word compared
  three steps past its own (`*p.at_step + 3` in `Online`'s two
  `flight.compare` calls), the test as committed failed: "its prediction was
  off by more than its bounds: 1 updates compared, the worst error 1529 mm,
  the 95th percentile 1529 mm". Words placed past the steps flown are held,
  so only one was compared. With the change taken out and the client
  rebuilt, it passed again: 974 updates, 0 / 0 / 0.537 m.
- COST is now a release runner's seconds: 25 and 12, Windows debug's 273
  and 121 s over twelve. Given as debug seconds, run 44934787992 dealt them
  as an hour with the night's repeats.

**Before: main's window client (Linux debug), in the C172P, no word a
wreck.** Main's window client has no `--test-pilot`. So "before" is
measured two ways, each over 4,800 ticks (40 s of the server's time):
- **Main's client as it is**: the stick held still. This branch's binary
  differs from main's here only in what it prints at the shot.
- **Main's client with moving controls**: `--test-pilot`. This PR changes no
  prediction or pacing code. Its source diff against main is the shared
  test pilot, the `--test-pilot` and `--unpaced` flags, and lines printed at
  the shot. So with the pilot added, this is main's flying.

| server | stick still: median / 95th / worst | test pilot: median / 95th / worst |
|--------|-----|-----|
| 0.6 of real time | 0 / 0 / 0.462 m (967 updates) | 0 / 0 / 0.634 m (965) |
| keeping real time | 0 / 0 / 0 m (974) | 0 / 0 / 0 m (973) |

Every run flew: 0 words a wreck, at 100 or 137 to 140 kt at the shot.
Once the clocks' difference was known, the worst correction was 0.46 to
0.63 m at 0.6, and 0 m keeping time. While it was learnt it was 3.7 to
4.6 m at 0.6, and 0.5 to 0.9 m keeping time.

**So main already holds the bound; this PR fixes nothing in the window
client's flying.** It adds the test that pins it, and the pilot and
unpaced red that test needs. The 6 to 9 m and 10 to 13 m of 2026-10-08 are
gone on main. **What removed them was not bisected.** A likely cause is the
2026-10-24 change: since then the window client compares every word at its
own step and steers its pace by every word, not each frame's newest. Before
that, a word about a step not yet flown, if it was the frame's newest, put
the aircraft at the server's word ahead of the steps counted. Pacing's later
changes (2026-11-19, 2026-11-21) are another candidate. Neither is shown.

**Why the window tests could not have seen it.** The window client's tests
fly a stick held still, so every input is the same. glideslope_cli's paced
test says why its own pilot keeps moving: with every input the same, a
prediction that flies each input for the wrong number of steps is still
right. Unpaced against 0.6, the test pilot in the C172P is put 2.2 m off at
the 95th percentile.

**Withdrawn: the F-15C figures first reported here.** In the first version
of this file, the test pilot flew the F-15C and every run ended as a wreck
(below). Those figures included a wreck's nought error and are not
evidence. The stick-still F-15C runs did fly, at 300 kt at the shot:
0 / 0 / 0 m at 0.6 and 0 / 0 / 1.378 m keeping time.

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

**Verification.** The C172P figures, the tests as they now stand, are in
the train section at the top. They are 0.63 and 0.51 m at worst on Linux
debug, and 0.60 and 0.52 m on Windows debug.
- `a_window_client_predicting_against_a_server_behind_real_time_flies_at_its_pace_and_is_off_by_centimetres`
  (port 24942, server at `--test-pace 0.6`) and
  `a_window_client_predicting_against_a_server_keeping_real_time_is_off_by_centimetres`
  (port 24944), both RUN_SERIAL.
- No `timing` label. The held-a-second sibling has one because its long
  frames need a release build's quick frames between them. These frames are
  not held, and the worst is 2.4 m under its 3 m bound. CI's figures are in
  this pull request's runs. There, ctest prints the "its prediction" line
  only on failure.
- **Withdrawn (F-15C, the test pilot flying her into the ground).** First
  reported here: passed, worst 1.8 m, with these seen reds. Unpaced at 0.6,
  95th percentile 7.97 m and worst 18.0 m. Keeping time with ticks 2 percent
  fast, 95th 1.57 m. A wreck's nought error was part of each, so none is
  evidence.
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
  tests, with the flag and port checks, Linux debug, `-j4`, before the
  C172P change: 113 of 113 passed, two skipped for want of a display (the nightly label left out).
- **Selftest hash** 27e94c795b47141c, unchanged: the selftest does not fly
  the test pilot.

**Tail (not a plan item).** A wreck flown again from its start is taken by
`Flight::reconcile` as an ordinary correction. It is snapped as too large
to hide, so the aircraft is drawn right at once. But the client's
correction and error figures count the jump: 7,080 m and 7,064 m on CI. A
player sees nothing wrong, so this is named here, not added to the plan. A
client that knew the server had placed her again would start its
prediction afresh there, as at a take-over.

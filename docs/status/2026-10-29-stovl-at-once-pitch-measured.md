### The STOVL at-once tests' pitch judged where she was, and bounded from what was measured, 2026-10-29

**What is still not true first.** The new figure has been seen on two
platforms here (linux-release, Windows debug), not yet on CI's others; the
full run of this pull request is its first sight of them. linux-debug was
not built here: CI's Ubuntu linux-debug figures below are of the old
measure.

**The failure.** #273's full run 40829041494, Windows debug, shard 5/7:
`a_client_whose_first_asking_for_her_levers_is_ignored_asks_again_and_seeds_its_law_as_the_server_does`,
"her predicted pitch was 0.032 degrees off the server's, over 16
thousandths". The bound was twice the worst of one run (2026-10-29's
`stovl-at-once-pitch-every-platform`).

**Cause: the measure, not the law.** Her pitch was compared at the step
each server word was *placed* at by the clocks' difference. On a busy
machine a word's input reaches the server late and the word is placed
whole steps off (the failing run's client said "placed 11 steps from there
at most"); the pitch at that step is then off by those steps' pitching,
though the prediction flew the same path - 0.003 m from it at worst in the
failing run. The client now also says her pitch *where she was*: the
server's word against her pitch on the path flown, interpolated between
the steps either side of the nearest point, outside the machine's stalls
and not at an end of the steps kept (as the error off the path already
is). On Windows debug here, the same runs:

| placed-step figure | where she was | steps off at the placed worst |
|---|---|---|
| 0.020 (red against 16) | 0.003 | 2 |
| 0.013 | 0.001 | 2 |
| 0.006 / 0.008 / 0.008 / 0.009 | 0.001 / 0.001 / 0.002 / 0.001 | 1-4 |

The law itself agrees to a thousandth or two; what grew to 0.032 was
placement. The at-once tests now judge the figure where she was, over at
least 2,700 of the 3,000 updates (3,000 in every run here), and print
both in their `figure` line. The placed figure's line and the other STOVL
tests that read it (landing, join, take-over, vertical landing) are
unchanged.

**The figures, this branch.**
- linux-release here, the three tests 4 at once, 8 rounds, plus one round
  alone: 27 runs, where she was 0.001 in every one; placed 0.005 at most.
- Windows debug here (tools/windows_build.sh, the three at once), 4 rounds
  with the new figure: 12 runs, where she was 0.003 at most (0.001 in 7,
  0.002 in 4); placed 0.020 at most. With the old figure alone, 2 rounds
  more: placed 0.024 at most.
- CI, the old placed figure, every completed run since 2026-10-28 20:00
  (`gh run view --job --log`, "The figures said"), worst per platform over
  all three tests: Ubuntu release 0.006 (n 27), Ubuntu debug 0.009 (44),
  Rocky 9 0.006 (30), macOS release 0.007 (27), macOS debug 0.007 (15),
  Windows release 0.011 (23), Windows clang 0.011 (23), Windows debug
  0.032 (40) - Windows debug alone above 0.012.

**The bound: 0.010 degree, all three tests.** One figure where she was for
all three, since with placement taken out the three scenes agree (0.001 on
Linux, 0.003 at most on Windows debug). 0.010 is three times the worst
seen, ten printed thousandths, and fifty times below the law gone astray
that the test was written for (0.2 on Linux, 0.58 and 0.96 on CI's
Windows, 2026-10-28).

**Seen red**: the spent test run here with `-DPITCH_BOUND_MDEG=0` failed,
"her predicted pitch where she was was 0.001 degrees off the server's, over
0 thousandths"; and on Windows debug here the old measure failed at 0.020
against 16, the failure reproduced, with 0.003 where she was in the same
run.

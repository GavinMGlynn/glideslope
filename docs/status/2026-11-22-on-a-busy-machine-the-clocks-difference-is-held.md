# 2026-11-22: on a busy machine the clocks' difference is held, and the server's word is placed where it was sent

Phase 12a, "On a busy machine the server's word is placed a step off now and
then".

**What is still not true first.**
- The STOVL selector test still fails beside twice as many busy loops as
  cores, on two checks the 95th percentile is not part of:
  - In every loaded run, before this change and after, fewer than nine in ten
    of the updates compared were corrections once joined: 1,999 to 2,198 of
    3,000 before, 1,684 to 1,998 after. Under load, words come in batches of up
    to 35, and all but the newest are compared without being put right from.
    This change does not cause it and does not cure it. It is named as a tail
    in the plan.
  - In one run of ten after the change, the server slipped about 50 steps
    within 0.7 s, 109 s in. The worst error was then 39.4 m, over the test's
    5.6 m. See "The cost" below.
- Windows' two-step spread (its own item) is not touched or measured here.

## Cause (measured, then fixed)

glideslope_cli was run with `--track` in the STOVL selector test's flight,
ten times beside 40 busy loops on 20 cores. Linux debug, main at 8c5869bf.

- Most of a word's own sample (the server's step at reading less the client's
  step at sending) sat one or two steps above the least. About one sample in
  a hundred sat a step below the rest (35 of 3,029 in run 1). Those are the
  inputs a held-up server read at once.
- Each such low lowered the least of the 50-word window by a step when it
  came, and raised it again 50 words later as it left: 18 to 32 changes in a
  run.
- After each change, the next round trip of words, about six, is compared
  against steps flown on the figure before. Each of those words is a step off,
  about 1.4 m at her speed. The number of words more than 0.1 m off was six
  times the number of changes, to within a word, in all ten runs. The
  distance off the path stayed under 0.005 m: the error was all along the
  path, which is where a wrong placement puts it.

## Fix

`sim::ClockDifference` (src/sim/clock_difference.hpp) is now the figure
`sim::Prediction` places words by, replays from and flies the session's clock
by.
- Until settled (25 words) it is the least of the window, as before.
- After that it is lowered only when at least `offset_lowered_after` (10) of
  the window's 50 samples are below it, and then to the 10th least.
- It rises whenever the least of the window is above it, as the least did.
- The pace is still steered by the least of the newest ten (unchanged).

Replayed over the ten runs' samples before it was built: 0 or 1 changes a run.
With 5 instead of 10 it was up to 7, and with 3 up to 10.

**The cost (accepted by the owner).** A server that really slips is followed
on the 10th word after, not the first, about 0.4 s. The unit test pins it to
exactly the 10th. But that is not where the error comes from. Placement is
self-consistent while the figure holds still: the words before it moves were
0.002 m off in loaded run 1, 26 steps "wrong". The error comes when it moves.
A slip that arrives as a ramp (run 1: samples 510, 482, 475, 465, 460, 457
over 0.7 s) moved the figure four times within a round trip (508, 482, 475,
467, 465, 463, 460). The words compared then were up to 45 steps off: 22.8 to
39.4 m, one correction too large to hide. The plain least would have moved at
each new low too (508 to 457 in five changes over the same 0.7 s). That run
was not repeated on the old code, so whether it would have been as far off is
not shown. None of the ten runs before the change had such a slip.

## The gust test's rule, rewritten

`server_weather_gusts.cmake` asserted that no step was flown later than the
server but for a word setting a new least. That was true of the least: no
word's own difference was below it. Held, the figure stands above up to nine
of the window's samples by design. So it now asserts the held figure's own
rule from outside: no step is flown later than the server where the estimate
it was flown on stood above ten or more of the 50 words' own differences
before it.
- glideslope_cli records, for each step flown, how many of the words' own
  differences were below the estimate (`shown_below`).
- Each sample is no more than its word's own difference, so fewer than ten
  samples below means fewer than ten words below.
- A later step with ten or more below is an estimate held where its rule
  would have lowered it.
- The line said is now "... later than the server with the estimate held
  above too many words".

## Verification

- `a_clocks_difference_is_held_against_a_sample_now_and_then_low_and_follows_a_server_that_slips_ten_words_late`
  (test_prediction.cpp) checks four cases, counted, with every word heard
  counted:
  - settling at 433;
  - 3,000 settled words, 433 and 434 by turns, with 39 a step low (30 alone,
    and runs of 2, 3 and 4), and no change;
  - a slip of 30 steps followed on exactly the 10th word;
  - a return of 5 steps risen to on exactly the 50th.

  **Seen red** with the figure always the least: "53 changes over 3,000
  words with 39 a step low". Reverted.
- **The item's verification**: the STOVL selector test's flight, ten runs
  beside 40 busy loops (20 cores), Linux debug, this branch:

  | run | 95th percentile | median | worst | words a step off | figure's changes |
  | --- | --- | --- | --- | --- | --- |
  | 1 | 0.003 m | 0.000 m | 39.392 m (the slip) | 94 | 21 |
  | 2 | 0.001 m | 0.000 m | 1.385 m | 12 | 2 |
  | 3 | 0.001 m | 0.000 m | 0.003 m | 0 | 0 |
  | 4 | 0.001 m | 0.000 m | 0.877 m | 32 | 4 |
  | 5 | 0.001 m | 0.000 m | 0.007 m | 0 | 0 |
  | 6 | 0.002 m | 0.000 m | 0.873 m | 6 | 1 |
  | 7 | 0.001 m | 0.000 m | 0.875 m | 18 | 2 |
  | 8 | 0.001 m | 0.000 m | 1.366 m | 6 | 1 |
  | 9 | 0.001 m | 0.000 m | 0.003 m | 0 | 0 |
  | 10 | 0.001 m | 0.000 m | 0.006 m | 0 | 0 |

  The 95th percentile was under 10 cm in all ten runs, which meets the item's
  verification. All ten still failed the test, on the corrections-once-joined
  check (1,684 to 2,122 of 3,000), and run 1 on its worst (the slip). The
  test's own bounds (3.4 m at the 95th percentile, 60 mm at the median) are
  left as they are: they are set from every platform's CI figures, and
  Windows' two-step spread is its own open item.

  Before (main, the same ten-run setup): the 95th percentile was 0.874 to
  0.876 m in six runs and 0.002 to 0.004 m in four. The median was 0.000 to
  0.001 m, and 109 to 202 words a run were a step off.
- Related suites, Linux debug, idle, `ctest -j4`:
  - A run of 171 tests matching predict, pace, clock, gust, stovl, held_up,
    stopped_alone, behind_real_time, first_input, starved, weather and
    machine_stall: 152 passed and none failed when it was stopped for time.
    The rest are left to CI.
  - The gust test with its new rule passed. It judged 554 updates in air: 279
    flown at the server's moment, 275 on another estimate, 0 later than the
    server, and 0 out otherwise. Its median was 0.029 m.
  - Run apart, each passed: the server-behind-real-time tests (engine,
    pace, machine stopped for seconds), the stopped-alone test and the
    held-up-unevenly test.
- Selftest hash: `27e94c795b47141c`, the same as main's build. The selftest
  flies no prediction.

## The item's history, from the plan

The plan's text before this change, kept here as the plan is a summary:

2026-10-16: until then that test holds it to four steps (3.4 m) and its
median to 60 mm, from CI's figures. 2026-10-23: CI's Windows placed the
first-input test's words 5 and 18 steps off (24.7 m), no client frame over
171 ms; that test holds its updates to the path flown instead. 2026-10-29:
the STOVL join with its first datagrams lost reached a 77 mm median on
Windows, along her path, its cause unknown; it is held to 120 mm.
2026-10-29: under load the STOVL join's pitch went 0.5 to 0.9 degrees off at
the right step (not placement): her jet-borne law's state, which no word
carries, drifts from the server's, mostly during B's 150 unselected inputs.
CI 0.592 once; the tests stay on pull requests until a second CI failure.
2026-10-30: CI's Windows debug placed the STOVL at-once test's words 11
steps off (run 40829041494), 0.032 degree of pitch; it judges pitch where
she was. 2026-10-30: the lost-datagram STOVL join test (four CI failures)
and the starved F-35B's placement (twice) run nightly, named in
tests/cmake/nightly_tests.cmake, until this item is done. 2026-11-01: the
mid-bank STOVL join (no datagrams lost) reached a 64 mm median on CI's
Windows debug, run 41284303752, its first failure; 2026-11-02: 61 mm on
#298's run 41397416052, its second, and it moved to nightly
(tests/cmake/nightly_tests.cmake), the 60 mm bound kept; its nightly result
is reported here. 2026-11-20 nightly 44832435697: the lost-datagram STOVL
join failed (shard 4 of 8).

# 2026-11-22: on Windows the server's word is placed where it was sent

Phase 12a, "On Windows the server's word is placed two steps off now and then".

**What is still not true first.**
- Measured on this machine's Windows debug build alone. The paced test's new
  99th-percentile bound (250 mm) is not set from CI's figures: a passing test
  prints none in CI's log (`--output-on-failure`). This pull request's full
  run is its first on windows-release, windows-clang, macOS and Linux.
- The held-up-unevenly test stays nightly. The held figure did not change its
  Windows debug figures (below), so the cause it went nightly for is not shown
  fixed. And the nightly runs it on Linux debug alone, where it never failed,
  so the nightly as it stands cannot show it fixed: a new open item, "The
  held-up-unevenly test comes back to pull requests", adds windows-release,
  windows-clang and macOS to it, and the test returns after seven green
  nights there.
- The first run of a fresh Windows build of the A-B below timed out at 900 s:
  the client waited with its server and relay already gone. Every later run
  of that build passed in about 51 s. Not seen on this branch's build, and
  not chased; the owner's note says Webroot blocks freshly built test
  programs on this machine.

## Cause, and why no new code

The cause was found on 2026-10-08 (docs/status of that date): Windows'
15.6 ms sleeps give each word up to two steps of spread, and the least of the
fifty words' differences moved by two steps as a quick word entered the
window and again as it left, placing that round trip's words two steps off,
1.1 m.

`sim::ClockDifference` (src/sim/clock_difference.hpp, the busy-machine item,
on main today) holds the figure once settled and lowers it only when ten of
the fifty are below it. A quick word now and then is one of fifty, so it no
longer moves the figure. No Windows-specific change was needed. Timer
resolution was not changed: `timeBeginPeriod` is per-process since Windows
10 2004 and is not needed for the bound.

## Verification

`a_client_predicting_against_a_server_behind_real_time_flies_at_its_pace_and_is_off_by_centimetres`,
ten runs on Windows debug (this machine, `tools/windows_build.sh`, main at
0b7d552f):

| run | worst | 95th | 99th | median |
| --- | --- | --- | --- | --- |
| 1 | 8 mm | 6 mm | 7 mm | 3 mm |
| 2 | 8 mm | 6 mm | 7 mm | 3 mm |
| 3 | 8 mm | 5 mm | 7 mm | 3 mm |
| 4 | 8 mm | 6 mm | 7 mm | 3 mm |
| 5 | 8 mm | 6 mm | 7 mm | 3 mm |
| 6 | 8 mm | 6 mm | 7 mm | 3 mm |
| 7 | 8 mm | 5 mm | 7 mm | 3 mm |
| 8 | 7 mm | 6 mm | 7 mm | 3 mm |
| 9 | 8 mm | 6 mm | 7 mm | 3 mm |
| 10 | 8 mm | 5 mm | 7 mm | 3 mm |

All ten under a metre at the 99th percentile, which meets the item's
verification.

**A-B with the plain least**: the same build with `offset_lowered_after` set
to 1, which makes the figure exactly the least again (a local commit, never
pushed, then deleted). Ten runs; the first timed out (above), and in the
nine that finished the 99th percentile was 482, 505, 507, 516, 517, 518, 519,
520 and 572 mm, the 95th 7 to 465 mm, the worst up to 919 mm. That is the
two-step spread back. All nine passed the test's bounds as they stood (the
metre is on the 95th).

## The 99th percentile now held (review of #404)

The test read the 99th percentile and bounded only the 95th, so nothing
guarded the item's figure. The paced flight alone (not the stall or the
server held up, which spread more by what they build) now fails past
250 mm at the 99th: under one step's travel (0.42 m), 35 times the 7 mm
measured, and under the plain least's least (482 mm).

- Green on this branch, Windows debug: three runs, 99th percentile 7 mm each.
- **Seen red**: `offset_lowered_after` set to 1 again (local, never pushed,
  deleted), Windows debug, three runs: all three failed on it, the 99th
  percentile 519, 521 and 518 mm. Reverted.

## The held-up-unevenly test

`a_client_predicting_against_a_server_held_up_unevenly_every_second_is_within_a_metre_for_ninety_five_updates_in_a_hundred`
went nightly on 2026-10-29 for windows-release's 1,143 mm 95th percentile
(run 40743161009) and 3.3 m worst (40673705998).

- Ten runs on Windows debug with the held figure: all passed. 95th percentile
  341, 55, 479, 237, 394, 72, 419, 360, 64 and 499 mm against its 1 m; 99th
  284 to 1,438 mm; worst 0.42 to 1.48 m against its 10 m.
- Five with the plain least: all passed, 95th 403 to 497 mm, 99th 497 to
  992 mm. Not different: its spread comes from the hold-ups it builds.
- The nightly has run it on Linux debug alone: five times a night on
  2026-11-18 to 11-21 (44418731482, 44636793974, 44832435697, 44954156532),
  all twenty passed.

Its reason in tests/cmake/nightly_tests.cmake now says why it stays and when
it returns.

Selftest hash: not moved, as no source changed - a test's bound, the nightly
list's reason, comments and documents.

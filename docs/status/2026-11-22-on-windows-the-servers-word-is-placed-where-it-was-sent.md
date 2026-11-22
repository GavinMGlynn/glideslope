# 2026-11-22: on Windows the server's word is placed where it was sent

Phase 12a, "On Windows the server's word is placed two steps off now and then".

**What is still not true first.**
- Measured on this machine's Windows debug build alone. Windows release,
  windows-clang and macOS were not run here; the pull request's full CI run
  is the first time the held-up-unevenly test runs on them since it went
  nightly (2026-10-29).
- The held-up-unevenly test's 99th percentile is not improved by this. Its
  spread comes from the hold-ups it builds, not from Windows' timers: 284 to
  1,438 mm with the held figure, 497 to 992 mm with the plain least. Its test
  bounds the 95th percentile, not the 99th.
- The first run of the fresh Windows build of the A-B below timed out at
  900 s: the client waited with its server and relay already gone. Every
  later run of that build passed in about 51 s. Not seen on this branch's
  build, and not chased; the owner's note says Webroot blocks freshly built
  test programs on this machine.

## Cause, and why no new code

The cause was found on 2026-10-08 (docs/status of that date): Windows'
15.6 ms sleeps give each word up to two steps of spread, and the least of the
fifty words' differences moved by two steps as a quick word entered the
window and again as it left, placing that round trip's words two steps off,
1.1 m.

`sim::ClockDifference` (src/sim/clock_difference.hpp, the busy-machine item,
on main today) holds the figure once settled and lowers it only when ten of
the fifty are below it. A quick word now and then is one of fifty, so it no
longer moves the figure. That is the same mechanism as the busy machine's,
so no Windows-specific change was needed. Timer resolution was not changed:
`timeBeginPeriod` is per-process since Windows 10 2004 and would make each of
the server, relay and client ask for 1 ms. It is not needed for the bound,
and a client that relied on it would still have to survive a machine that
does not grant it (Windows 11 does not for an occluded or minimised window).

## Verification

`a_client_predicting_against_a_server_behind_real_time_flies_at_its_pace_and_is_off_by_centimetres`,
ten runs on Windows debug (this machine, `tools/windows_build.sh`, this
branch at its start, i.e. main at 0b7d552f):

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
verification. All passed, 600 updates compared each, 2,876 steps flown here
to the server's 2,876 in the run shown.

**Seen red, by A-B**: the same build with `offset_lowered_after` set to 1,
which makes the figure exactly the plain least again (a local commit, never
pushed, then deleted). Nine runs that finished (the first timed out, above):
the 99th percentile 482, 505, 507, 516, 517, 518, 519, 520 and 572 mm; the
95th 7 to 465 mm; the worst up to 919 mm. That is the two-step spread back:
0.5 m is a little over one step's travel at this aircraft's speed. None went
past the test's own bounds (the metre is on the 95th); the item's figure is
the 99th, which the held figure takes from about 0.5 m to 7 mm.

## The held-up-unevenly test comes off the nightly list

`a_client_predicting_against_a_server_held_up_unevenly_every_second_is_within_a_metre_for_ninety_five_updates_in_a_hundred`
went nightly on 2026-10-29 for windows-release's 1,143 mm 95th percentile
(run 40743161009) and 3.3 m worst (40673705998). Both predate inputs naming
their step, flown apart after a hold-up (2026-11-17).

- Ten runs on Windows debug, this branch: all passed. 95th percentile 341,
  55, 479, 237, 394, 72, 419, 360, 64 and 499 mm against its 1 m; worst
  0.42 to 1.48 m against its 10 m; median 7 or 8 mm.
- Five with the plain least (the A-B build): all passed, 95th 403 to 497 mm.
- The nightly has run it only on Linux debug: five times a night on
  2026-11-18 to 11-21 (44418731482, 44636793974, 44832435697, 44954156532),
  all twenty passed.

Its cause on Windows is not shown to persist, and its worst Windows debug
figure is half its bound, so it comes back to pull requests
(tests/cmake/nightly_tests.cmake now names four). A red on CI is a red tree
again; a second for one cause puts it back with that cause named.

Selftest hash: not moved, as no source changed - only a test's label, the
nightly list, a comment and documents.

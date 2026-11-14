# Gust names from the scheduled run of 2026-11-13

Scheduled ci.yml run 43513776356, on main at 3c35ac0d, failed two single-seed
gust tests on measured figures (owner, 2026-11-04: a one-seed, one-platform
gust name closes on the figure). Test-data change only; no simulation change,
so `selftest`'s hash does not move.

## Named, in `named_on_a_seed` (tests/unit/test_lesson.cpp)

- **short_s23, seed 15, every Windows preset** (`windows()`): touches at
  336.335134 ft/min, inside 360 but not by 25, on windows-debug (gust sweep
  day 5, 3/8), windows-clang (gust sweep 8/8) and windows-release (gust
  sweep 5/10), the same figure on all three.
- **f22, seed 3, every macOS preset**: was `macos_release()` at 372 ft/min
  (CI run 43240423674); macos-debug fails the same seed at 372.363196
  ft/min, so it is now `macos()`.
- **j3cub, seed 18, macOS release** (`macos_release()`): touches at
  352.085248 ft/min (gust sweep 4/5). macOS debug did not fly seed 18 that
  night (the weekly rotation), so it is not named there.

## Names taken off, where they passed with every margin

On macOS release (gust sweep 4/5) the test said to take two names off:
- **a380, seed 31** and **short_s23, seed 18**, both named on `macos()`,
  passed with every margin. Each is now `macos_debug()`, a new helper:
  macOS debug was where they were measured failing alongside release, and it
  did not fly either seed that night, so the name stays on debug until a
  debug run measures it.

## Other failures in the run

Only one: linux-debug tests 8/14,
`the_window_client_joining_on_final_behind_a_pa28_waits_told_why_and_is_at_the_gate_thirty_seconds_after_she_starts`
(214 s), and macos-debug tests 2/8,
`ai_aircraft_keep_flying_with_nobody_connected_and_a_late_client_finds_them_mid_flight`
(13.9 s). Both are multi-process tests, not gust names, and are left alone.
No other gust failure appears in the logs read.

## Verification

linux-release builds with `-j6`; the gust naming tests of test_lesson pass
(see the pull request). On Linux the two new names are not `here`, so the
change is exercised on Windows and macOS CI only.

# Gust names from the scheduled run of 2026-11-13

Scheduled ci.yml run 43513776356, on main at 3c35ac0d, was the first full
gust sweep on macOS release since every aircraft started moving with her wind
(#365; the sweep runs only on schedule and dispatch since #368). It failed
single-seed gust tests on Windows and macOS, closed here on their measured
figures (owner, 2026-11-04: a one-seed, one-platform gust name closes on the
figure). Test-data change only; no simulation change, so `selftest`'s hash
does not move.

## Named, in `named_on_a_seed` (tests/unit/test_lesson.cpp)

- **short_s23, seed 15, every Windows preset** (`windows()`): touches at
  336.335134 ft/min, inside 360 but not by 25, on windows-debug (gust sweep
  day 5, 3/8), windows-clang (8/8) and windows-release (5/10), the same
  figure on all three.
- **f22, seed 3, every macOS preset**: was `macos_release()` (CI run
  43240423674); macos-debug fails it at the same 372.363196 ft/min, so it is
  now `macos()`.
- **737-300, seed 6, every macOS preset**: was `macos_release()`; macos-debug
  (tests 1/8) fails it at the same 349.072720 ft/min, so it is now `macos()`.
- **macOS release only** (`macos_release()`); macOS debug did not fly these
  seeds that night (the weekly rotation), so they are not named there:
  - j3cub, seed 18: touches at 352.085248 ft/min (gust sweep 4/5);
  - 737-300, seed 20: touches at 382.507879 ft/min (5/5);
  - 747-400, seed 23: touches at 353.905731 ft/min (2/5);
  - pa28, seed 16: climbs at 91.569158 ft/min in her flare (3/5);
  - short_s23, seed 28: rises 0.773471 ft after her touch (5/5).

## Names removed, where macOS release passed with every margin

The test said to take these names off, and they are removed, not narrowed to
macOS debug: each was named on `macos()`, and a name kept on debug alone would
stand on a figure measured before the start changed.
- a380, seed 31 (gust sweep 4/5);
- short_s23, seed 18 (4/5);
- j3cub, seed 30 (2/5);
- pa28, seed 34 (2/5).

## Other failures in the run

Two, both multi-process tests and not gust names, left alone here:
- linux-debug tests 8/14,
  `the_window_client_joining_on_final_behind_a_pa28_waits_told_why_and_is_at_the_gate_thirty_seconds_after_she_starts`
  (214 s): its cause is fixed by #375.
- macos-debug tests 2/8,
  `ai_aircraft_keep_flying_with_nobody_connected_and_a_late_client_finds_them_mid_flight`
  (13.9 s): its cause is fixed by #374.

Every failed job's log was read, each failing test's whole `WRONG` list taken;
no other gust failure is in the run.

## Verification

linux-release builds with `-j6`, and the gust naming tests of test_lesson
pass. On Linux none of these names is `here`, so the change is exercised by
the next macOS and Windows sweep: this pull request's dispatched full run.

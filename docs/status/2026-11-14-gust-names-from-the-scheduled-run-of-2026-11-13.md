# Gust names from the scheduled run of 2026-11-13

Scheduled ci.yml run 43513776356, on main at 3c35ac0d, failed two single-seed
gust tests on measured figures (owner, 2026-11-04: a one-seed, one-platform
gust name closes on the figure). Test-data change only; no simulation change,
so `selftest`'s hash does not move.

## Named, in `named_on_a_seed` (tests/unit/test_lesson.cpp)

- **short_s23, seed 15, Windows debug and clang**: touches at 336.335134
  ft/min, inside 360 but not by 25. Failed on windows-debug (gust sweep day 5,
  3/8) and windows-clang (gust sweep 8/8), the same figure on both. It uses a
  new helper, `windows_debug()` (Windows without NDEBUG), not `windows()`:
  windows-release's seed 15 is not in the run's logs (its gust sweep shards
  5/10 and 6/10 have no logs, 404), so it is not named there. If it fails
  there, name it from that figure.
- **f22, seed 3, every macOS preset**: was `macos_release()` at 372 ft/min
  (CI run 43240423674); macos-debug fails the same seed at 372.363196 ft/min,
  so it is now `macos()`.

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

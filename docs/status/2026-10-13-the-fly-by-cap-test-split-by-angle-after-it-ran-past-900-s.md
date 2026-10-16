### The fly-by cap test split by angle, after it ran past 900 s in CI's linux-debug, 2026-10-13 — item stays done

**What is still missing, first.** Nothing of the item. The CI times below
for the new tests are estimates, not measured: each preset's time for the
90-degree heading test scaled by its local time. CI's next runs will measure
them.

**Split.** `a_turn_of_45_90_120_or_150_degrees_at_a_waypoint_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class`
timed out in linux-debug (run 38393627412, over 900 s). It is now four tests,
one for each angle, as the orbit-entry tests were split:
`a_turn_of_{45,90,120,150}_degrees_at_a_waypoint_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class`.
- Each flies 28 turns: an aircraft of each of the seven classes, at two
  speeds, both ways. Each asserts that coverage.
- `the_fly_by_cap_tests_together_turn_an_aircraft_of_every_class_112_times_at_four_angles`
  asserts the four angles and the 112 turns the four make together, the
  same as the one test they were split from.

**One test removed as covered.**
`a_turn_of_45_120_or_150_degrees_either_way_at_a_waypoint_strays_within_its_bound_in_an_aircraft_of_every_class`
flew the same turns at cruise speed only, with the same stray bounds. Those
turns are now inside the four cap tests, and its estimated linux-debug time
was 883 s.

**Local linux-release times** (`ctest -j1`), and the linux-debug estimate
from the same scaling (34.6 times):
- 45 degrees: 17.5 s, about 650 s in debug.
- 90 degrees: 17.4 s.
- 120 degrees: 16.2 s.
- 150 degrees: 13.5 s, about 490 s in debug.
- The coverage test: 0 s.
- The branch's other new tests, which were not split:
  - the bank hold: 10.6 s, about 370 s in debug;
  - short legs: 9.7 s;
  - the 90-degree fly-by groups: 1.6 to 3.0 s;
  - the crosswind test: 3.0 s;
  - fly-over: 2.1 s;
  - trim decay: 1.1 s.

  None of these is near 120 s locally, so none is split.

**`tests/ci_costs/`.** All seven presets now carry the branch's 14 new tests,
each the local time scaled by that preset's ratio for
`every_aircraft_turns_ninety_degrees_without_overbanking_or_overshooting`
(6.48 s here). The ratios: linux-debug 34.6, windows-debug 17.3, macos-debug
9.1, windows-release 5.6, linux-release 3.9, windows-clang 2.8,
macos-release 1.5.

**Verified**, linux-release: the four cap tests, the coverage test and
`every_compiled_unit_test_is_registered_with_ctest` pass.

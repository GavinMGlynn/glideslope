### The fly-by cap tests split by speed too: eight of 14 turns, the slowest about 320 s of linux-debug's 900, 2026-10-13 — item stays done

**What is still missing, first.** Nothing of the item. The CI times are
still estimates, scaled as in the entry below, until CI's next runs measure
them.

**Why.** Split by angle alone, the slowest of the four tests, 45 degrees, was
estimated at about 650 s of linux-debug's 900. That margin is too thin for a
slow runner.

**The tests.** Each angle is now split by speed as well:
`a_turn_of_{45,90,120,150}_degrees_at_a_waypoint_at_her_{climb,cruise}_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class`.
- Each flies 14 turns, an aircraft of each of the seven classes both ways,
  and asserts that coverage.
- `the_fly_by_cap_tests_together_turn_an_aircraft_of_every_class_112_times_at_four_angles`
  now asserts eight tests and 112 turns.

**Local linux-release times** (`ctest -j1`), climb then cruise:

| Angle | Climb | Cruise |
|---|---|---|
| 45° | 7.97 s | 8.96 s |
| 90° | 9.32 s | 8.95 s |
| 120° | 7.70 s | 8.58 s |
| 150° | 6.45 s | 7.11 s |

The slowest, 9.32 s, is estimated at 323 s in linux-debug and 162 s in
windows-debug. `tests/ci_costs/` now carries the eight tests in every preset
in place of the four.

**Verified**, linux-release: the eight tests, the coverage test and
`every_compiled_unit_test_is_registered_with_ctest` pass.

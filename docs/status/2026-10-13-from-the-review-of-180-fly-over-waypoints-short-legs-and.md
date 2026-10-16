### From the review of #180: fly-over waypoints, short legs and turns past 120 degrees, and the bank integral gone when the wings come level, 2026-10-13 — item stays done; the OpenAI coast recording must be made again

**What is still missing, first.**
`the_copilot_follows_the_coast_as_told_by_openai_as_recorded` fails. Its
flight now goes another way at its third ask (exit 3), because the copilot's
coast route has legs short enough for the new cap on the lead to bind. With
the cap alone switched off, the playback passes again; with the new trim decay
alone switched off, it still fails. Re-recording needs a live call, which this
round was told not to make without asking. Until it is re-recorded, the tree
is red on that one test. No platform code changed, so this was not built on
Windows.

**Fly-over waypoints** (sim/plan.hpp, plan.cpp, navigator.cpp):
- A waypoint line may end in `flyover`. That waypoint is passed abeam, and
  only then is it turned from, as every waypoint was before. Fly-by is the
  default, as ARINC 424 and FMS practice has it.
- Any other seventh word is refused, naming the line.
- The plan format is documented in plan.hpp's header and in the plan files'
  own comments.
- **Committed plans checked.**
  - `sydney-harbour.plan` marks THE_HEADS and BRIDGE `flyover`: the tour's
    landmarks. OLYMPIC is a turning point and stays fly-by.
  - `sydney-arrival.plan`'s one waypoint is its last, and
    `sydney-cbd-orbit.plan`'s CLIMB has an orbit after it: both are flown over
    whatever they say.
  - `tests/data/copilot/747-off-bondi.plan` is left unmarked, so that its
    recording's request does not change.
- **Not in the copilot.** Its prompt and its routes do not carry the marker
  (copilot.cpp's `waypoint_line` and the server's route-to-plan text). Every
  recording's request would change if they did. That work is a new Phase 13
  item.
- **Test** `a_waypoint_marked_flyover_is_passed_within_100_m_by_an_aircraft_of_every_class`
  flies the first catalogue aircraft of each of the 7 classes, at cruise
  speed, north 8 km to the marked waypoint and then 90 degrees right. Each
  passes it within 9.0 m (the Mosquito; the rest within 1 m).
  **Seen to fail** with the marker ignored: 483 m to 3,437 m off.

**Short legs, and no cliff at 120 degrees** (`sim::fly_by`, now public):
- **A lead is no more than half of either leg it joins.** Where that binds, the
  arc is tightened to fit.
- **Past 120 degrees the turn is still flown by.**
  - The arc keeps the 120-degree lead, r tan 60, so its radius shrinks as the
    turn grows, down to the radius at 25 degrees of bank at 143 degrees.
  - From there the lead grows with the turn up to its length at 160 degrees,
    r(25) tan 80, and then stops growing.
  - Before, 120 degrees was led by 1.7 radii and 121 not at all.
- **Tests.** Every stray is measured off the next leg, past it or from where
  its arc meets it, against the turn radius at 25 degrees. Each test asserts
  its coverage.
  - `a_turn_of_45_120_or_150_degrees_either_way_at_a_waypoint_strays_within_its_bound_in_an_aircraft_of_every_class`:
    7 aircraft × 3 angles × 2 ways = 42 turns. Up to 120 degrees every turn is
    under 0.1 r (worst 0.015). At 150 degrees every turn is under 0.15 r: the
    F-15C 0.121 and 0.103, the Mosquito 0.081, the 172P 0.064, the rest
    under 0.02. At 150 degrees the arc is flown at 25 degrees of bank, with
    none to spare.
  - `a_turn_at_a_waypoint_in_a_20_kt_crosswind_strays_within_its_bound`: the
    172P, the 737-300 and the F-15C, turning 90 degrees each way with a 20 kt
    wind across the first leg. Every one is under 0.1 r; the worst is the
    172P onto the tailwind, 0.044.
  - `a_plan_of_legs_shorter_than_their_turns_passes_each_waypoint_in_turn_near_its_arc`
    zigzags each of the 7 aircraft over two lengths of leg.
    - Over legs of 1.6 r(15) + 7 s, where the cap binds and the arc can still
      be flown, every waypoint is passed on a step of its own and within 15 m
      of where its tightened arc passes it.
    - Over legs of 0.4 r(15), which no arc she can fly fits, every waypoint is
      passed at most half a leg short of itself.
    - **Seen to fail** with the cap off: 5.2 to 5.7 half-legs short, and two
      waypoints passed on one step.
- `the_navigator_flies_a_plan_past_every_waypoint_in_calm_air_and_in_wind`
  now reads each arc from `sim::fly_by`. THE_HEADS and BRIDGE, flown over,
  are passed 0.3 and 23 m off. OLYMPIC, a fly-by of more than 120 degrees, is
  passed 1,277 m off against its arc's 1,267.
- `along_from` now wraps longitude into -180 to 180.

**The bank integral** (sim/autopilot.cpp):
- **Gone when the wings come level.** Below 5 degrees of bank commanded, or in
  an upset, the trim falls to nothing within 1 s from its most
  (`aileron_trim_level_s`). Before, it faded at a tenth of that rate.
- **Test** `the_ailerons_trim_from_a_turn_in_gusts_is_gone_within_half_a_second_of_the_wings_coming_level`
  flies every aircraft at an 8-degree bank in moderate turbulence for 60 s,
  then wings level. Every trim is gone within 0.35 s of the bank commanded
  falling below 5 degrees (the Cherokee, whose trim was 0.113). Every one of
  the 16 found a trim, which the test asserts.
  **Seen to fail** with the old fade: the Cherokee 3.53 s, the Mosquito
  3.02, and five more over 0.5.
- **The bank test holds each case to the bank asked.** It no longer holds a
  case to a lesser limit, and it asserts that no case reached a lesser limit;
  none did.
- **The gate swept** across the 90-degree turn test and the Cherokee's
  fly-by turn. At 0.25, 0.5 and 1 degree a second, the Mosquito turns 3.70,
  3.78 and 3.95 past her heading (4 allowed), and the Cherokee strays 0.066,
  0.063 and 0.063 r. 0.5 is the middle, clear of both bounds, and is what the
  orbit, lesson and landing tests were run against.
- The constants' comments were put back in order: `offset_fade_s` had been
  parted from its own.

**The selftest hash does not move**: `glideslope_cli selftest` printed
`hash 182dd6c996e0ee4c` (linux-release).

**Verified**, linux-release, display unset:
- 495 tests by
  `ctest -j4 -R "orbit|loiter|turn|navigator|plan|approach|crosswind|turbulen|gust|lesson|bank|trim|take_off|takes_off|takeoff|first_leg|stall|land|lander|selftest|heading|copilot|flyover|waypoint|legs" -E "server|client|vulkan|window|shot|_now"`:
  494 passed, and the coast playback failed as above.
- Every `as_recorded` test, one at a time: 15 of 16 passed, the coast one
  failing.

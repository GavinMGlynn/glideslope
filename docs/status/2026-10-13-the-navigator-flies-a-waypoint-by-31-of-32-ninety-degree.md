### The navigator flies a waypoint by: 31 of 32 ninety-degree turns stray at most 0.04 of the turn radius, against 1.1 to 1.6 before, 2026-10-13 — item still open

**What is still missing, first.** The Cherokee (pa28) at her climb speed,
74 KCAS, strays 52 m past the next leg, 0.142 of her 368 m radius at 25
degrees, against the tenth asked. Asked for a bank, she holds about four
fifths of it (15.7 for 20). The autopilot's aileron integral works only at
its bank limit (sim/autopilot.cpp), so she flies the arc wide and rolls out
past the leg. Her case is named in the test and held to a fifth, so the item
stays open. Fixing her bank hold is autopilot work, not navigator work, and
was outside this item's time box. **And the OpenAI coast recording must be
recorded again** (below): until it is, that test is red. No platform code,
so not built on Windows.

**What changed** (src/sim/navigator.cpp, sim/turn_laws.hpp):
- **Fly-by waypoints.** A waypoint with a leg after it is passed short of
  itself, by the distance of turn anticipation. FAA Order 8260-44A gives that
  distance as DTA = r tan(A / 2), and ICAO Doc 8168 defines a fly-by waypoint
  as one "which requires turn anticipation to allow tangential interception
  of the next segment". Here r is her radius at `fly_by_bank_deg`, 15
  degrees, at her speed over the ground. Two more terms are added: half the
  roll-in at 5 degrees a second, and 1.5 s of roll lag (`roll_lag_allowed_s`).
- **The arc is flown, not just begun.** Round the arc tangent to both legs,
  the navigator asks a bank from the loiter law the orbits use, now factored
  out as `loiter_bank_deg`, so orbits fly the same arithmetic. When her track
  is within what her roll-out still turns her through, wings level is asked.
  That figure is half the roll-out at the arc's rate of turn, plus the roll
  lag measured as she rolled in. Once her wings are level, or her track is
  within half a degree of the leg's, the leg's law takes over again. If she
  never comes round, the leg's law takes over once she is twice the lead
  past the waypoint.
- **Flown over, as before**: the last waypoint (so a plan's landing approach
  starts as it did), a waypoint an orbit follows, a turn of more than 120
  degrees (the most PANS-OPS lets a fly-by turn be), and a turn within twice
  what the roll-out turns through.

**Why each piece** (measured with the new test, linux-release):
- *The lead alone*, still turned by the heading law (a degree of bank per
  degree off, 25 at most), with a worked allowance for the roll-in and the
  heading law's exponential roll-out: the Cherokee at climb speed strayed
  0.19 r and the Cub 0.11. Both were slower to roll than the law assumes;
  the Cherokee took ten seconds to reach 23 degrees. A model of the laws
  predicted jets at 300 kt near 0.1 r: the 30-degree-per-km intercept and
  the heading loop are too lightly damped at their speed (zeta about 0.44 at
  155 m/s).
- *The arc at 20 degrees by the loiter law*: everything under 0.1 except the
  S.23 at climb speed, 0.118. She overbanked to 30, rolled out late, and
  came onto the leg from 79 m inside it. Measuring her roll lag fixed that,
  0.05. Asking wings level for the roll-out, rather than handing the last
  of the turn to the heading law, roughly halved the jets' strays.
- *At 15 degrees*: the airliners strayed two to three times less than at
  20, because there are 10 degrees left over for the loiter law. A roll lag
  allowance of 3 s, in place of 1.5, made every aeroplane stray further,
  because she began too early.

**Figures**, stray / turn radius at 25 degrees (5,000 ft, calm, 90 degrees
right; climb, then cruise):
737-300 0.001, 0.002; 747-400 0.007, 0.004; 787-8 0.002, 0.003; A320 0.001,
0.002; A380 0.002, 0.002; Learjet 35A 0.001, 0.001; B-2A 0.003, 0.002;
F-15C 0.005, 0.002; F-22A 0.004, 0.005; F-35B 0.010, 0.003; Mosquito 0.019,
0.013; S.23 0.036, 0.011; 172P 0.026, 0.015; 182S 0.031, 0.017; Cub 0.020,
0.018; **Cherokee 0.142**, 0.040. The old navigator, abeam: 1.10 to 1.57.

**The plan tour's waypoints are passed where their arcs pass them.**
`the_navigator_flies_a_plan_past_every_waypoint_in_calm_air_and_in_wind`
held each waypoint within 100 m of the aircraft. A waypoint flown by is
passed r (1 / cos(A / 2) - 1) off by its own arc, so it is now held within
100 m of that, and its altitude is taken as it is passed. In calm air:
THE_HEADS, a sharp turn left, 771 m against its arc's 769; BRIDGE 15.7
against 14.8; OLYMPIC, more than 120 degrees and so flown over, 0.4.

**The selftest hash does not move**, `182dd6c996e0ee4c` (linux-release): the
selftest flies no plan.

**Verified**, linux-release:
- The four tests
  `every_{light_aeroplane,airliner_and_business_jet,fighter_and_bomber,warbird_and_seaplane}_turning_90_degrees_at_a_waypoint_at_her_climb_and_cruise_speeds_strays_off_the_next_leg_less_than_a_tenth_of_her_turn_radius`
  each walk one of the tightest-orbit groups. That every class is in exactly
  one group is asserted already. Each test asserts that every aircraft in its
  group flew both speeds, and that the one named case was flown where it
  belongs. Climb speed is `departure_speeds().climb_kts` and cruise is the
  catalogue start speed. Each is held to the plan speeds: the 747-400's
  climb 170 becomes 220, the F-35B's 200 becomes 204. They take 3 to 6 s each.
  **Seen to fail** on the old navigator (the lead forced to 0): all four
  red, every aircraft 1.10 to 1.57 r. Restored, green.
- Every test whose name speaks of the navigator, a take-off, a first leg, a
  plan, an orbit, an approach, a recording, a departure, the selftest, a
  heading, a ninety-degree turn, the docs or the copilot, but not the server,
  client or renderer ones: 400 by
  `ctest -j4 -R "navigator|take_off|takes_off|takeoff|first_leg|plan|orbit|landing_approach|approach|recorded|departure|selftest|heading|turns_ninety|docs|copilot" -E "server|client|vulkan|window|shot"`.
  388 passed and 11 live-model `_now` tests skipped (no key given). **One
  failed: `the_copilot_follows_the_coast_as_told_by_openai_as_recorded`.**
  Its fifth exchange, at 420 s, "was recorded for another request". The
  prompt carries the flight's state, and that state changes now that
  waypoints are flown by. It must be recorded again with a live OpenAI call,
  which this branch was told not to make. The tree is red on it until then.

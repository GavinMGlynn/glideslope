### A jet's orbit entered from a waypoint holds its circle: joined going its way, flown by a loiter law after ArduPilot's, 2026-10-09 — item done

**What the item's figures were.** The 460 m was Claude's 2026-10-06 plans
(the F-22A's on a 9.4 km circle). Flown again on this tree, both recorded
plans - a waypoint at the CBD, then a 7,003 m orbit left at 220 kt - held
within 10 m from the half-turn `fly-plan` measures from, and so did the
747-400's on de45cc78, where the 460 m was written down. What was wrong
showed only once an orbit was entered from the places a plan can put the
waypoint before it.

**The new test builds that**: `sim::fly_orbit_from_waypoint`
(`sim/orbit_trial.hpp`) flies an aircraft at 3,000 ft from 5 km short of a
waypoint at the orbit's centre, half a radius out, on the circle or two
radii out, south of the centre, arriving from each of the eight points of
the compass, then once round the orbit. `the_<jet>_enters_an_orbit_from_a_waypoint_..._and_holds_its_circle`,
for each of the ten jets (the airliners, the business jet, the fighters
and the bomber; `every_jet_has_its_own_test_of_entering_an_orbit_from_a_waypoint`
holds the list to the catalogue's), flies each at 220 kt - the speed
Claude asked of both - or the nearest its plan speeds allow, round the
tightest circle a plan may ask at it, both ways round: 50 entries each,
the count asserted. Left out, by name in the test: from the centre, the
seven arrivals other than north (the same flight turned, in calm air), and
wind (the tightest-orbit tests fly in it). Each must join the circle, stay
within the 100 m it is joined within from there, within 60 m from a
quarter-turn, and within 50 ft of its height.

**Red on the old navigator**: the 747-400 strayed up to 6.1 km off its
7 km circle. Two causes. It was "on the circle" - joined, counting and
held to it - as soon as it was within 100 m, whichever way it was going:
arriving at a waypoint on the circle heading across it or the wrong way, it
was then turned round onto the circle, a turn kilometres wide. And from
inside, it closed at 45 degrees and then turned in by what brought it back
in twelve seconds - faster than a jet's heading loop, whose own time is
about v/g (11.5 s at 220 kt), can follow; it swung through the circle.
Capping the closing angle at an arc it could fly (a first attempt, in the
WIP commit) brought the swing to 257-421 m from joining and 49-93 m from a
quarter-turn, and was touchy: a capture bank of 10 degrees gave the 747
33 m, 12 degrees 131 m.

**The fix** (`sim/navigator.cpp`), general, no aircraft named:
- Joined only going its way round: within 100 m and its track within 20
  degrees of the circle's.
- On the circle, and out to it from inside, the navigator asks the autopilot
  for a bank, not a heading (`AutopilotModes::bank_deg`, new; the autopilot
  flies it within the bank it sustains, and a turn away from the separation
  monitor still overrides it): a loiter law after ArduPilot's
  AP_L1_Control loiter (GPLv3), itself from Park, Deyst & How 2004 ("A New
  Nonlinear Guidance Logic for Trajectory Tracking", AIAA GNC). Its
  structure is ArduPilot's, not the paper's a = 2 V^2 sin(eta) / L1: a
  spring and damper on the offset, the 0.5 r floor on the centripetal
  term's radius, and no pull back in while the speed round is negative -
  tan bank = (w^2 off + 2 zeta w v_out + v_round^2 / (max(r/2, d) cos
  crab)) / g, w = 2 pi / period, damping 0.75. The cos crab (review of
  #154) is the wind's: the circle over the ground needs v_round^2 / r
  square across the ground track, and the bank pulls square across the
  air's. It flies it inside the circle,
  joined or not, and outside within its L1 distance (zeta period v / pi) and
  250 m going its way round; farther out, joined or not, the tangent line as
  before - L1's capture. Each limit was found by a test going red: held by
  the loiter law only once joined, a Mosquito that crossed its circle not
  yet going its way followed the tangent line round 200 m outside it and
  never joined (`the_tightest_orbit_at_the_fastest_speed_..._mosquito_fb6`,
  in wind); held by it anywhere once joined, a Cherokee gliding at 30,000
  ft, short of the bank her circle needs there, looped 1.5 km off it and
  never round its centre (`the_piper_pa28_glides_round_...`, and the
  C182's, Learjet's and F-15C's glides); within the whole L1 distance, a
  kilometre for a jet, it took over the tangent line too soon and the
  747-400 swung 350 m outside after joining.
- The period: ArduPilot's 17 s swung the 747-400 7 km off (its bank cannot
  follow so fast), 25 s 1.9 km; 40 s and 60 s held every entry, 40 the
  tighter (747-400 8 m, 16 at 60). 40 s it is. The heading law's 12 s
  closing, its 45-degree intercept and its trim integral are gone.

**From the review of #154** (on the branch restacked onto #153):
- The law's attribution, above, says plainly whose structure it is.
- **Wind**: the review asked for v_air v_ground / r in place of
  v_ground^2 / r. Measured with a new windy part of the entry test (20 kt
  from the west; from the centre and from on the circle arriving from each
  quarter, both ways round - 10 more entries a jet, the count asserted),
  v_air v_ground / r held the jets within 9 to 15 m after the first
  quarter-turn and v_ground^2 / r within 4 to 10: the turn over the ground
  is the ground speed's, and the bank meets it across the crab angle, so
  the exact term is v_ground^2 / (r cos crab), which is what flies (its
  figures are v_ground^2's at 20 kt, the crab under 6 degrees).
- `heading_off_for_bank_deg`, which only the old heading law used, is gone.
- **A bank left behind is never flown**: the navigator sets no heading with
  a bank, and the autopilot flies `bank_deg` only when there is no heading
  - a heading set wins (the server's `hold_course` takes the modes and sets
  a heading, which would otherwise have flown a stale bank).
- **The entry test's bound is 30 m after the first quarter-turn**, not 60:
  every jet holds within 10 m on Linux, and the rest is a margin for other
  platforms and the wind, small enough that a navigator letting one wander
  50 m fails.

**After**, linux-release, every jet's 60 entries: within 100 m from joining
(the join band itself) and, after the first quarter-turn, within 4 to 9 m
in calm air - the 737-300 5, 747-400 8, 787-8 9, A320 4, A380 5, Learjet 4,
B-2 4, F-15C 8, F-22A 4, F-35B 4 - and 4 to 10 m in a 20 kt wind - 737-300
4, 747-400 8, 787-8 10, A320 4, A380 5, Learjet 4, B-2 4, F-15C 9, F-22A 4,
F-35B 5 - and within 50 ft of the height. 283 orbit, navigator, plan,
glide, autopilot, crosswind and stall tests green in linux-release after the
review's changes (the window client's and server's not built here); the
selftest hash is still `182dd6c996e0ee4c`. Claude's two recorded plans
(`take_off_climb_to_3000_ft_and_orbit_the_cbd_is_planned_for_the_{747-400,f22}_by_anthropic_as_recorded_and_flown`)
are now held to the Cessna's 60 m, not 500 (`CIRCLE_M` dropped): the
747-400 flies 6,995 to 7,001 m round its 7,003 m, the F-22A 6,999 to 7,006;
the Cessna's two, 1,452-1,455 m of 1,447 and 1,492-1,495 m of 1,500.

**What else moved**: `an_orbit_begun_from_its_centre_counts_its_turns_only_from_its_circle`
asserted the Cessna joined from inside; steered out from the centre it now
joins going its way just outside (1,586 to 1,600 m of 1,500, 1,250 to 1,270
of 1,172), so it is held to joining within 100 m either side, and round the
circle from there within 10 m. Everything else that flies an orbit, a plan
or a glide - 234 tests in linux-release (`ctest -R
"orbit|navigator|plan|glide|monitor|turned_away|between_two_layers|16r|handing_the_aircraft"`,
less the window client's and the server's, which were not built here),
among them every aircraft's tightest orbits at its slowest and fastest and
one step past them, every glide round the tightest orbit, the take-offs
from 16R flown on to the orbit, and the four CBD orbits as recorded - is
green; the five that ask a model now report themselves skipped.

The selftest hash does not move (`182dd6c996e0ee4c`, linux-release): it
flies the test pilot, not the navigator. The new tests' entries in
`tests/ci_costs/*.txt` are **estimates** (below) until
`tools/ci_test_costs.py` measures them.

**Split for CI's time** (after #154's first CI run): as one test a jet,
the F-15C's, A320's and A380's ran past ctest's 900 s in CI's linux-debug
(run 37957098079). Each jet's 60 entries are now five tests, none dropped:
`..._in_calm_air_heading_north_or_northeast_...` (arriving at 0 and 45
degrees, with the centre's: 14 entries), `..._heading_east_or_southeast_...`,
`..._heading_south_or_southwest_...` and `..._heading_west_or_northwest_...`
(12 each), and `..._in_a_20_kt_wind_...` (10). Each asserts its own count;
`every_jet_enters_every_orbit_entry_once_across_its_five_tests` builds the
whole space (60) and the five parts and asserts every entry is in exactly
one. Halves of the compass were tried first: the F-22A's ran 291 and 315 s
locally in linux-debug. In quarters, locally in linux-debug four at a time,
the 50 tests took 56 to 94 s, the F-22A's 111 to 150 (its north quarter).
The ci_costs entries are those times x2.2 for the debug builds (CI's
linux-debug took over 900 s for what took about 410 here) and x0.15 for
the release builds (a tenth of debug here, and half again for CI's
runners) - estimates still.

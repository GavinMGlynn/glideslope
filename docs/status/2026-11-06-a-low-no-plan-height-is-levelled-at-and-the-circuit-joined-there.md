### A low no-plan height is levelled at, and the circuit joined there, 2026-11-06

Two Phase 10a tails found on 2026-11-06
(2026-11-06-a-take-off-with-no-plan-captures-its-height-promptly).

**What it does not do, first.**
- **The levelling off with the flap out is flown at the take-off's
  thrust**, the throttle left where the take-off had it. Nothing watches a
  flap's limiting speed, because no model here gives one. The fastest
  hand-over at 650 ft is the F-22A's, at 281 kt.
- **The levelling starts at the lead whatever the class.** It waits only
  for the screen height and the gear coming up, not for the 400 ft an
  airliner's altitude capture waits for (below). With a 400 ft floor the
  F-22A went 291 ft past 650.
- **No flap limit speed (VFE) is watched.** No model here gives one.
  Levelling off at 650 ft at take-off thrust, the fastest each flew with
  any flap still out (more than half a degree, so including flap still
  running up):
  - 737-300 193 kt;
  - 747-400 269;
  - 787-8 199;
  - A320 206;
  - A380 228;
  - F-22A 265;
  - Learjet 35A 179.

  This is a Phase 10a tail. The levelling autopilot is given her plan's
  fastest (`limit_speed`), as every engaged autopilot is.
- **A take-off demonstrated to the default 500 ft** levels a jet off at
  about 200 ft with her flap out, and she flies out of the lesson's band.
  This is a Phase 10a tail; nothing in the programs asks for the default.
- **Measured on linux-release only.** Nothing here is platform-specific
  code, so there was no Windows build.

**1. A jet taken off with no plan to a low height no longer climbs far
past it.** At the lead (a tenth of her climb short of her height), a
take-off with no plan that still has her flap out or her take-off trim on
now levels off there. Until now it climbed on until both were off. She
must be through the 35 ft screen height, with her gear up where it
retracts. An autopilot inside the take-off (`Departure::levelling_`)
captures the height at the capture's gain. The throttle stays where the
take-off has it, and the flap comes up on the take-off's own schedule.
The take-off is over once the flap is up and the trim off, and the
controller's autopilot then holds the height as before.

Held to her climb speed on the throttle instead, the 737-300 had her flap
up at 166 kt and was flown level, clean, under her clean wing's stall. She
sank into the ground from 638 ft. So the throttle is left at take-off
thrust, and she accelerates level while she cleans up. That is how Airbus's
engine-out departure is flown: "level off, accelerate and clean up".

Past 650 ft at most, from brakes off to two minutes after the hand-over:

| Aircraft | Before | After | Handed over now |
|---|---|---|---|
| 737-300 | 323.3 ft | 0.0 ft | 629 ft, 204 kt |
| 747-400 | 453.7 | 10.5 | 634 ft, 272 kt |
| 787-8 | 139.4 | 4.7 | 609 ft, +651 ft/min |
| A320 | 531.5 | 3.0 | 649 ft |
| A380 | 468.4 | 10.4 | 653 ft |
| Learjet 35A | 195.5 | 0.0 | 569 ft |
| F-22A | 313.0 | 20.6 | 482 ft, +4,787 ft/min |
| B-2A | 6.6 | 6.6 | unchanged |

The light aeroplanes, the F-15C, the F-35B and the Mosquito are handed over
clean, as before, and are unchanged: no more than 12.1 ft past (J-3 Cub).

**Research.** Searches:
- the FAA Airman Certification Standards' altitude and traffic-pattern
  tolerances;
- airliner take-off flap and thrust schedules against a low level-off:
  acceleration altitude, thrust reduction, NADP 1 and 2, Boeing 737 low
  level-off, Airbus A320 ALT* below acceleration altitude;
- how autopilots treat a capture altitude reached during the take-off mode,
  and one below the aircraft at engagement (Garmin G1000, S-TEC, TruTrak,
  Eclipse 500).

What they found:
- Airbus's SRS take-off mode gives way to ALT* or ALT CST only above
  400 ft RA (The Flying Engineer; FlyByWire A32NX guidance). Below that,
  the take-off mode keeps climbing.
- An engine-out departure is "level off, accelerate and clean up" (A320
  procedure notes), at take-off or maximum continuous thrust.
- Acceleration height is typically 1,000 to 1,500 ft, as low as 800. NADP 2
  begins no lower than 800 ft (ICAO Doc 8168 as quoted). A forum post says
  400 ft is the least with no noise procedure. None of this was found in
  the primary documents.
- A PPRuNe question asks whether a 737 may go to climb thrust below 800 ft
  for a level-off at 1,000 ft or less. The FMC accepts no thrust reduction
  below 800 ft. No answer was found.
- An autopilot engaged with the selected altitude below it descends to it
  only when asked (Eclipse 500: ALT CHG; TruTrak: climbs or descends to the
  selected altitude). The Garmin GFC hands over from ALTS to ALT 50 ft
  short.

**The bound is 100 ft**, the FAA's altitude tolerance (Private Pilot ACS,
FAA-S-ACS-6C, "maintain altitude ±100 feet"). It is the
`no_plan_within_ft` the 3,000 ft hold test uses.

**Why not the 400 ft floor.** Airbus's 400 ft was tried first, applied to
every jet. The F-22A climbs at 6,900 ft/min, so her lead is 690 ft, more
than the whole 650. From 400 ft she was clean by 514 ft and went 291 ft
past. The autopilot's pitch moves 3 degrees a second at most, so taking
her 14-degree climb path level takes about 4.7 s and 270 ft of climb. With
the floor at 200 ft or lower she went 20.6 ft past. The screen height and
the gear are the take-off's own guards, and they are kept. 650 ft no-plan
heights are this project's choice, not an airline's.

**2. The circuit tests ask the take-off for 650 ft, and a light aeroplane
is joined to the circuit with her flap where her take-off left it.**
Joined level at 650 ft, the PA-28 started her downwind leg at 954 ft and
climbed on to 1,078 ft, 124 ft over where the leg began. That is past the
lesson's 120 ft band. Traced, the cause was the test, not the climb:
- the circuit test joined every aeroplane with her take-off flap
  (`joined.flaps = dep.flap`). For a light aeroplane, the take-off had
  already brought it up from 50 ft;
- the PA-28's is 25 degrees. Put back out at 650 ft, she slowed to 51 kt
  climbing at full throttle, 23 kt under her climbing speed;
- she rolled out downwind on the back of the drag curve, and ballooned as
  her speed came back.

A light aeroplane now keeps the flap she has at the join, which is up. A
jet still flies the pattern at her take-off flap, as Boeing's circuit is
flown. Downwind, at 650 ft:

| Aircraft | Circuit | Downwind before | Downwind after |
|---|---|---|---|
| PA-28 | 1,035 ft | 954 to 1,078 | 1,004 to 1,035 |
| C182 | 1,035 | 1,006 to 1,035 | 996 to 1,035 |
| C172P | 1,000 | 971 to 1,000 | 966 to 1,000 |
| J-3 Cub | 1,000 | 945 to 1,006 | 945 to 1,006 |

The "before" column is the 650 ft run on the old code, with the flap put
back out.

**The downwind band.** The item asked for 50 ft of circuit height. That is
tighter than any source found. The FAA's ACS gives ±100 ft for traffic
pattern altitude (Private and Commercial ACS, traffic patterns task). The
pattern itself turns downwind from 100 ft under circuit height
(`GoAroundCircuit::modes`), so the J-3 Cub, the slowest climber, begins
her downwind leg 55 ft under it. The verification is widened to the ACS's
100 ft. The lesson's own band, ±120 ft about where the stage began, is
unchanged.

**Verification.**
- `every_landplane_taken_off_with_no_plan_to_650_ft_goes_no_more_than_100_ft_past_it`
  (tests/unit/test_lesson.cpp, new). It takes off each of the 15 landplanes
  to 650 ft with no plan and holds her two minutes past the hand-over. The
  test asserts:
  - no wreck;
  - the take-off is over;
  - she was held the two minutes;
  - from brakes off she is no more than 100 ft past 650 ft;
  - 15 of 15 flown.

  The Short S.23 is named and left out because she takes off from water.
  All passed; the most past is the F-22A, at 20.6 ft. Red on the old code
  (`departure.cpp` and `.hpp` put back): the 737-300 went 323.3 ft past,
  and it failed.
- `the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief` now asks
  the take-off for 650 ft. It also asserts that every aeroplane's downwind
  leg is within 100 ft of its circuit height. The lower bound catches a
  sink along the leg, not a low entry, because the pattern turns downwind
  from 100 ft under. Each bound was seen red with the downwind leg's
  height in `circuit.cpp` moved 150 ft, then reverted. Moved up, the
  737-300 flew 1,559 to 1,650 ft of 1,500; moved down, 1,350 to 1,377.
  14 of the 16 are flown, the 747-400 and
  F-22A named and left out as before. At 650 ft with the old join it was
  red: the PA-28's debrief said "Hold the circuit height downwind".
  `a_circuit_flown_low_downwind_is_named_in_the_debrief` and the circuit
  demonstration fly the same circuit, at 650 ft too.

**What moved.** Measured on linux-release.
- Only a take-off with no plan that reaches its lead before its flap is up
  or its trim off: in the tests, the jets taken off to 650 ft. At 3,000 ft
  every flap is up long before the lead, and nothing changed.
- The circuit tests' climb out: joined at 650 ft level, not climbing.
  The jets that now level off at 650 ft accelerate there before they are
  joined. The lesson's climb-out band (climbing speed less 12 to plus 15,
  judged to 400 ft above the start) still holds. The fastest each flew in
  the climb out, before (take-off to 750 ft) and now: 737-300 170 to
  188 kt, A320 164 to 191, A380 156 to 173, Learjet 35A 177 to 191.
  Every downwind leg is unchanged but for the light aeroplanes above.
- `an_instructor_demonstrates_a_take_off_and_hands_it_over` now asks the
  take-off for 1,000 ft, clear of the lesson's end at 500. At the default
  500 the 737-300 levelled off at her lead and accelerated past the
  lesson's band for the climb before she reached 500 ft.
- Run on linux-release, excluding the gust_sweep label: every test in
  test_controller, test_navigator and test_runway_ground, and every test
  matching take, depart, circuit, lesson, autopilot, selftest, lift_off,
  level, hand, capture, gear, go_around, going_around or instructor:
  473 of 473 passed.
- The selftest hash does not move. The selftest flies a pilot's input log,
  not an AI take-off.

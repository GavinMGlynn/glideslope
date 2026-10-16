### The F-22A rotates at a held four degrees a second: her pitch loop's ground mode, and her rotation speed re-measured at 150 kt, 2026-10-14 — item done

**What is still not true first.** Nothing of the item. Two things change with
it and are said here: **her measured rotation speed is now 150 kt, not 110**,
and **the over-rotation test runs her up on part power** (0.6), because on
full power her new ground law flies her off before her tail reaches the
runway. **The ground mode engages on every touch of a wheel**, not only
at the take-off: at once on a landing, a bounce or a touch-and-go, fading
out again over five seconds after each. **That is unmeasured**: she has no
approach speed, so no test or plan lands her. Of the tests that put her
wheels down after flight, two fly her: she is dropped on her wheels from 25
ft and from half a foot (wrecked and not wrecked, as the test asks), and put down
with her wheels up (no wheel touches, so no ground mode); both pass. The
take-backs, skips, touch-and-gos and landings leave her out for want of a
Vref. **Her figures in the air are unchanged by construction**: clear of
the ground and five seconds after her wheels have left it, her pitch loop
is the pinned model's. Anything within five seconds of a lift-off flies the
blended loop. **Only subsets of the suite were run** (below), not the full
suite. Built only on Linux: no platform code.

**The cause, from #175 round three and #181.** Her flight controls' pitch
loop commands a pitch rate from her stick, and its integrator alone drives
her stabilators. On her wheels her nose wheel holds her nose, the rate does
not come, and the integrator wound to full stabilator whatever the stick
asked; when the nose wheel broke free she came up at ten to thirteen degrees
a second. #175 round three put the stick on the stabilators directly on her
wheels, which held her combat loading at 4.4 deg/s but moved her rotation to
140 kt; holding the integrator would leave nothing to lift the nose.

**What real fly-by-wire aircraft do, as published.**
- NASA TM-112208 (J. A. Franklin, "Revised Simulation Model of the Control
  System, Displays, and Propulsion System for an ASTOVL Lift Fan Aircraft",
  Ames, October 1997): "The rate command integrator is initialized to the
  current value ... if flaps are up or the aircraft has weight on wheels
  (wow)" (page 12 of the PDF) - the integrator kept from winding while on
  the ground. **Pinned**: NTRS 19970036304,
  https://ntrs.nasa.gov/api/citations/19970036304/downloads/19970036304.pdf,
  2,731,545 bytes, SHA-256
  `00911d6eeb3e41858cff9f210becf8a726b2a9052a97cbe5ce8921bec4998436`
  (fetched 2026-10-14).
- The A320's normal law (its FCOM, as quoted by airbusdriver.net, "Airbus
  Flight Control Laws"): a ground mode, a direct relation of stick to
  elevator before lift-off, and a blend of the ground law into the flight
  law over five seconds after lift-off. **Cited, not pinned**: a web page,
  and the FCOM itself is not public.
- Nothing published describes the F-22's own take-off law.

**The change** (`tools/make_f22.py`, `with_pitch_ground_mode`; the model
regenerated):
- **On any wheel** (`gear/wow`), the pitch loop's integrator is **washed out**
  at ten per second (`PITCH_GROUND_WASHOUT_PER_S`) and its gain raised by as
  much: it follows the rate error within a tenth of a second, so her stick
  commands a pitch rate through a proportional loop, the stabilator in
  proportion to the rate she is short of. The loop's own pitch-rate damping
  stays. Full back stick asks the gear-down 15 deg/s and gives full
  stabilator; she comes up at nine to eleven a second.
- **Off her wheels** both fade to the flight law over five seconds
  (`PITCH_GROUND_BLEND_S`, the A320's), through a rate-limited actuator; on
  touching they return at once. The integrator's state is the stabilator's
  command in both laws, so the flight law takes over where the ground law
  left it: no step.
- **Tried first and not kept**: the washout alone, at one per second, with no
  raised gain. The stabilator then lagged the stick by a second, and the
  autopilot's rate loop overshot: 5.9 deg/s at 0.3 of stick per degree a
  second, 9.3 at 0.05.
- **Her figures** (`assets/figures/f22.xml`): `rotation_degps="4"`, as the
  F-15C's and F-35B's (a tuning value, code7700's three to four), and
  `rotation_stick_per_degps="0.1"` - her stick already commands a rate, so
  less is needed. Turns of her stick on her wheels at her measured speeds,
  model's and combat loadings: 1 and 1 at 0.1, 7 and 5 at 0.15, 8 and 7 at
  0.2, 8 and 8 at 0.25 (the test allows eight); at 0.3, before the speeds
  were re-measured, 11 and 10.

**Her take-off speeds, re-measured** (`glideslope_cli takeoff-speeds f22
100`, linux-release, 61,570 lb, military power): from 100 kt to 140 she
lifts off more than 15 kt past the speed she was rotated at (rotated at 110,
unstuck at 143 kt; at 140, at just over 155) - her nose comes up at four
degrees a second, her wheels leave near ten degrees, and she floats in ground
effect before she is 5 ft up. 145 kt is the first that holds (unstuck 156);
**150 is written** (unstuck about 158), and the climb away, never slower
than 222.7 kt at 500 ft, is written **225** (was 220). **One published pair
to compare**: the Air Force's Accident Investigation Board report on the
F-22A at NAS Fallon, 13 April 2018 (released 15 November 2018; quoted by The
Aviationist and Air & Space Forces Magazine), gives that day's figures for a
military power take-off as 143 KCAS to rotate and 164 KCAS to take off, at a
weight it does not give. **Cited, not pinned**: the report's PDF is on
media.defense.gov and pacaf.af.mil, which refuse a scripted download
("Access Denied", 403) - its figures here are as the two articles quote it. The 110 written before was her integrator's snatch:
she was thrown off at about 119.

**Verification** (linux-release, server and client built):
- `every_aeroplane_given_a_rotation_rate_rotates_on_her_wheels_at_every_loading_no_faster_than_it`
  now flies the F-22A: 13 aeroplanes, 46 loadings, three named (the flying
  boat and two tail-wheel aeroplanes). Hers: 4.53 deg/s at her model's
  loading and 4.69 at combat, one turn of her stick each, off at 144.8 and
  136.3 kt. **Seen red** with the ground mode taken out of the model (her
  figures kept): 9.38 and 10.03 deg/s.
- `an_over_rotated_take_off_strikes_the_tail_and_its_debrief_says_so`
  failed with the ground mode in ("f22 was held fully back and never struck
  its tail": on full power she flew off at 10 degrees and 120 kt). Run up on
  0.6 of her throttle she strikes at 16.4 degrees on her wheels, 84 kt,
  against her contacts' 16.5, and her debrief names it; 0.5 and 0.7 strike
  on her wheels too (16.3 at 77 kt, 16.4 at 91), 0.8 with her wheels just off
  (16.6 at 99).
- `the_rotation_rate_a_take_off_is_held_to_is_read_and_refused_outside_0_to_10`
  reads the J-3 Cub's figures for "none" now, the F-22A's having one.
- **Her recorded plan made again**:
  `take_off_climb_to_3000_ft_and_orbit_the_cbd_is_planned_for_the_f22_by_anthropic_as_recorded_and_flown`
  failed, its prompt carrying her climb speed; recorded live from Anthropic
  (claude-haiku-4-5, one call) by its `_now_` test, which passed, and
  `tests/data/copilot/cbd-orbit-f22-anthropic.jsonl` replaced: the plan
  flies her at 225 kt.
- Every test matching "take|rotat|lift|hand|depart|climb_away|strike|lesson|fault|selftest|f22|figures"
  (216): all pass but those two before their changes, and nine skipped: eight
  live model calls, not asked for without GLIDESLOPE_LIVE_MODEL, and one
  Windows-only test. After the changes the
  two pass, with the measured-speeds test. Then every test matching
  "speed|climb|measured|copilot|plan" (387): all pass, fifteen of them
  skipped, each a live model call.
- The selftest hash does not move: 182dd6c996e0ee4c (it flies a C172P).

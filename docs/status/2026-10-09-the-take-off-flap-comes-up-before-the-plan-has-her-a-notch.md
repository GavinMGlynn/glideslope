### The take-off flap comes up before the plan has her, a notch at a time, 2026-10-09 — item done

**What is still missing, first.** Nothing of the item. The heights and
speeds are general rules cited per class, not each aeroplane's own
handbook figure: no aeroplane's figures file carries a flap-retraction
speed or height of its own, and the take-off's climb speed stands in for
it. Not run on Windows or macOS; nothing here is platform code. Found on
the way, tails: the Cub climbs her first leg 4.8 kt under the take-off's
climb speed, with no flap to blame; and the take-off climbs a light
aeroplane at its speed for her weight (the 172P 66.7 KCAS) and hands over
to an autopilot whose floor is the published, heavier figure (75.4), so
she accelerates for half a minute after the hand-over.

**The bug.** `Departure::fly_laws` set the lever to the take-off flap at
the top of every step and to 0 only while climbing above 200 ft; on the
step the take-off ended it was the take-off flap again, and those controls
were the ones the autopilot was engaged with and held. So every one of the
nine aeroplanes that takes off with flap flew its plan with it out: the
Cherokee 25 degrees, the 172P 10, the 182S 20, the 737 5, the A320 10, the
A380 17, the 747 20, the 787 5, the Learjet 8. The flaps had begun to run
up at 200 ft (6 to 8 degrees left at the hand-over) and ran out again.

**The rule** (src/sim/departure.cpp, `retract_flaps`), the same for every
aeroplane, from data:
- **Height**: `DepartureSpeeds::flaps_up_ft`, from the catalogue's class.
  A light aeroplane's is 50 ft: the FAA's Airplane Flying Handbook
  (FAA-H-8083-3C, chapter 6, short-field take-off) climbs over the 50 ft
  obstacle and retracts "when the airplane is stabilized at Vy ... in
  increments to avoid sudden loss of lift and settling"; the Cherokee 140's
  own handbook, "slowly retract the flaps when the obstacle has been
  cleared". Every other's is 400 ft: 14 CFR 25.111(c)(4), no change of
  configuration on the take-off path below 400 ft.
- **Speed**: within 2 kt of the take-off's initial climb speed, which comes
  from the figures - Vy for a light aeroplane, V2 + 10 for a jet.
- **In steps**: the lever goes to the next notch of the model's own flap
  kinematic (`Aircraft::flap_notches`, read from the `<kinematic>` the flap
  command drives, in the model or a system file it names), and the next
  only once the flaps have run there and stood a second. Where there are no
  notches between, one movement.
- **At a hand's pace**: 1/120 of the lever's travel a step, as the
  autopilot moves every control.
- **The take-off is not over until the lever is up and the flaps have
  stopped** - as it already waited for the take-off trim - so the plan is
  handed a clean aeroplane, which also gives the autopilot's best-climb
  floor to the light aeroplanes from the hand-over.

**After the review** (#144):
- **The take-off always ends.** Waiting for the flap, one that never made
  its flap speed climbed on for ever. Once climbed out it now also ends,
  the flap left where the speed allowed, at 1,500 ft above the runway,
  where the take-off path ends (14 CFR 25.111(a)), or five minutes after
  the throttle opened, the limit on rated take-off power (14 CFR 1.1),
  whichever comes first. The flap still never comes up below its speed.
  The 1,500 ft also bounds the overshoot past the height asked while the
  flap comes up: the airliners, asked for 500 ft, hand over at 1,056 to
  1,194 ft (940 for the 747-400), and the new test holds every hand-over
  to 1,500 ft. A take-off asked for more than 1,500 ft ends at its height,
  flap or not.
- **The S.23 uses the 400 ft gate**, as every aeroplane not light does:
  she has no handbook height of her own. Her take-off is flown with no
  flap (her speeds give none, and no notch is read from her motor-driven
  flaps), so today nothing waits on it.
- **Taken over in the air**, a take-off starts its lever where the lever
  was found, not at the take-off flap; only on the ground is it set to the
  take-off flap.
- **No model has a flap whose first detent is not 0**: every one of the
  ten with a flap kinematic starts at 0, so that case is exercised by none.

**Measured** (linux-release), from the new test: up at / handed over at,
ft; the slowest with them up against the least-flap stall published, KCAS:
pa28 86 / 500, 66.4 vs 58.2; c172p 80 / 500, 65.0 vs 51.8; c182 108 / 500,
73.6 vs 55.0; 737-300 907 / 1,056, 173.5 vs 131.2; 787-8 607 / 671, 185.6
vs 151.3; a320 1,055 / 1,194, 179.1 vs 125.4; a380 1,038 / 1,079, 153.8 vs
136.8; 747-400 931 / 940, 176.7 (no stall published); learjet35a 543 /
601, 171.4 vs 119.0. The seven that take off clean hand over at 500-509 ft
as before.

**Moved, and its bound tightened back**: the first leg flown after the
take-off (`a_plan_that_takes_off_leaves_its_runway_and_flies_its_waypoints_in_every_light_aeroplane`)
strays, before -> after: pa28 151 -> 46 m; c172p 60 -> 62; c182 65 -> 66;
j3cub 28 -> 28. The bound #142 widened to 200 m is now 100 (it was 150
before #142). **And it now checks the first leg is climbed at her climb
speed** - the take-off's climb speed for her weight, which she is handed
over at - within 5 kt, slowest before -> after: pa28 51.5 -> 66.4 KCAS
against 63.8; c172p 62.4 -> 65.0 against 66.7; c182 71.5 -> 73.6 against
73.7; j3cub 34.9 both, against 39.7 (no flap; 4.8 under, not traced: a
tail).

**The 747-400's measured climb-away speed moved, and was measured again**:
190 -> 170 kt (`<takeoff_speeds>`, `glideslope_cli takeoff-speeds 747-400`:
the slowest she climbed away at, 187.1 -> 165.2 kt). At 165.2 she sagged
5 kt under the slowest speed the measurement asks, 170, with her nose at
the take-off autopilot's highest; 170 is written. The rotation text
changed with it, the speed she ran on to before leaving the ground now
"168 kt from 115 to 150" where it read "167 kt from 130 to 150"; her
rotation speed is unchanged at 160. The old measurement
read it with her flaps running up from 200 ft, the old law, though the
file says flaps 20. The trial (`sim/takeoff_trial.cpp`) now holds the
take-off flap to where it reads the climb away, and reads it at
`Departure::climbed_out` - through 500 ft with the take-off trim off,
where the take-off used to end - rather than at the end of the take-off,
which now waits for the flap. Her rotation is unchanged at 160. Her brief
to the copilot carries the speed, so **the copilot recording
`tests/data/copilot/cbd-orbit-747-400-anthropic.jsonl` was recorded
again** by its `_now` test with the owner's key (scanned: no key in it),
and played back green; its plan is a 1,000 ft take-off to an orbit of the
CBD at 3,000 ft and 220 kt.

**Verified**, linux-release:
- `every_aircraft_raises_its_take_off_flap_before_the_plan_has_it_and_keeps_above_its_flaps_up_stall`
  (new, registered): all 16 aircraft in the catalogue, the S.23 from water,
  taken off by a plan to 500 ft and flown to a waypoint 15 km off at
  3,000 ft: the lever up and the flaps stopped at the hand-over; up from
  then to the waypoint; never slower than the least-flap stall its figures
  publish (the 747-400 and F-22A publish none and are named); the lever no
  faster than 1/120 a step; a second at every notch between the take-off
  flap and up (the 737's two, the A320's three, the 747's, A380's, 182S's
  and Cherokee's one). The F-22A's flaperons are her flight controls' and
  not checked as flaps (her figures give no flap travel). **Seen to fail**
  with departure.cpp put back as it was: every one of the nine handed over
  with the lever at its take-off flap ("pa28 was handed to the plan with its
  lever at 0.625000"), the lever moving all at once ("0.625000 in a step"),
  no notch held, and the Cherokee at 49.4 KCAS against her 58.2 stall;
  restored, green.
- The new test now also states its space as numbers - 16 aircraft, 9
  taking off with flap, each aircraft's notches between its take-off flap
  and up (the 737 2, the A320 3, the 747, A380, 182S and Cherokee 1, the
  rest 0) - and holds every hand-over to 1,500 ft and every take-off's
  first lever to its take-off flap.
- `the_flap_levers_notches_are_the_settings_of_each_models_flap_kinematic`
  (new): all 16 read, six held to their model files - the 172P 0, 1/3,
  2/3, 1; the Cherokee 0, 0.25, 0.625, 1; the 737 and A320 nine settings
  each; the Learjet 0, 0.2, 0.5, 1; the Mosquito 0, 1.
- `a_take_off_that_never_makes_its_flap_speed_ends_with_the_flap_still_out`
  (new): the Cherokee asked to climb away at 140 KCAS, past her top
  speed, to 20 ft. At full throttle it ends at 1,500 ft after 223 s; with
  the throttle held to six tenths from 300 ft, at 298 ft after five
  minutes; never faster than 88.5 KCAS, the lever at 0.625 throughout.
  **Seen to fail** with the fallback taken out: "the take-off ended within
  five minutes: after 600 s", at 3,892 ft; restored, green.
- `a_take_off_taken_over_in_the_air_never_puts_out_a_flap_it_found_up`
  (new): the Cherokee at 300 ft and 70 KCAS, flaps up, given to a take-off
  with a 25-degree flap: the lever stays at 0 and the take-off ends in
  15 s. **Seen to fail** with the lever started at the take-off flap:
  "the lever stayed up: at most 0.625000"; restored, green.
- The first-leg test's climb-speed check, **seen to fail** the same way:
  "pa28 climbed its first leg at no less than 51.541286 KCAS, against its
  climb speed of 63.821254 less 5"; restored, green.
- `the_measured_take_off_speeds_in_the_747_and_f22_figures_are_what_the_measurement_makes`
  failed on the new law with the old file (165.2 against 190); green with
  the file measured again.
- 262 tests matching take, depart, circuit, plan, selftest, flap, figure,
  navigator, instructor and 747 (the client, HUD and window tests left
  out): all pass but three that need the window client, which was not
  built here (`a_pressed_during_...` and `a_take_over_past_the_servers_rate...`);
  the two take-over tests that need `glideslope_impair` pass once it is
  built. Four skipped, wanting a key or live consent.

**The selftest hash does not move**: `182dd6c996e0ee4c` (linux-release); it
replays a pilot's inputs on the 172P and flies no take-off.

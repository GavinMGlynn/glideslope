### The Mosquito on the ground: her brakes wait for her tail, and steer her at a crawl, 2026-10-30 - two items done

Two Phase 10b items were left open by the de Havilland Mosquito alone:
"The AI's roll-out drifts ... when a 15 kt crosswind arrives" and "The
Mosquito swings below walking pace". Both are now ticked. Built on #284
(the roll-out's crosswind steering), now on main.

**What is still not true first.**
- **Her real brake and tail-wheel behaviour is not measured.** The Pilot's
  Notes (A.P. 2019E) say only that her tail wheel castors ("taxy forward a
  few yards to straighten the tailwheel", para. 40) and that a swing is
  checked by "coarse use of the rudder and by differential throttle"; nothing
  found gives her landing roll's braking or crosswind taxying. The rules
  below come from the FAA's Airplane Flying Handbook for tailwheel
  aeroplanes in general.
- **The J-3 Cub is still left out of the roll-out test** (her roll-out never
  brings her below half a knot, and the item on her ground loop in 15 kt
  across), as before.
- **Differential throttle, the Pilot's Notes' own answer to a swing, is not
  flown.** The rollout's throttle is closed; her brakes do it instead.
- **The J-3 Cub's landing is longer.** Her brakes now wait for her tail
  too, and she rolls on her main wheels to half her reference speed: landed
  by the approach autopilot she stops 549 m past the threshold, not 429, and
  her measured landing distance (assets/figures/j3cub.xml, no published one)
  is measured again, 1,500 ft to 1,900 (`tools/measured_landing.py`; the
  copilot test's pin moved with it). Tried and no shorter: judging her tail
  by her tail wheel's weight (554 m), and with that the stick eased fully
  back once it touched, the AFH's three-point roll (554 m). The tail-up
  0.7 Vref hold alone, without the wait, left her at 428 m; the wait alone,
  491.

**What changed (`sim::Lander`, lander.cpp), for a tail-wheel aeroplane only.**
1. **Her brakes wait for her tail.** The autobrake puts on nothing while her
   pitch is more than `tail_down_deg` (2) below the attitude she stands at,
   and lets off if braking lifts her tail. AFH (FAA-H-8083-3C) chapter 14,
   wheel landing: "the tail should be permitted to drop on its own accord ...
   the elevator should be brought to the full aft position and deceleration
   should be allowed to proceed as in a three-point landing." Braked from the
   touch with her tail up, she went over on her nose dry.
2. **Her tail is held up to 0.7 of her reference speed**, not 0.9 (the
   stick comes back over 0.5 to 0.7 Vref). AFH chapter 14: many tailwheel
   pilots wheel-land in a crosswind, "making the flight controls more
   effective", and rudder effectiveness "can be reduced by the blocking effect of the
   fuselage and flaps with the tail low". Brought down from 0.9, her wing at
   12 degrees carried five sixths of her weight at 100 kt and her tyres held
   little: wet, 9.9 m off. #284's agent tried this alone and a hand-flown
   short-runway landing went over on her nose, braked tail up; with (1) it
   stops on the runway.
3. **Her rudder bar's steering brake does not fade at a crawl**
   (`Lander::toe_brakes`, now told whether she is on a tail wheel). A
   nose-wheel aeroplane's still fades below 5 kt to none at a stop. Her tail
   wheel castors and her rudder does nothing at a crawl, so her brakes are
   all that steer her: AFH chapter 14, crosswind taxi "sometimes requiring
   use of the brakes when tailwheel steering alone proves inadequate to
   counteract the weathervane effect". The steering brake is still six
   tenths at most (the Cub once broke away from nine tenths of one brake).

**Figures (linux-release, this FMA build).**

Roll-out as 15 kt arrives across, worst distance from the centreline:

| | before | after |
|---|---|---|
| Mosquito dry | went over on her nose | 2.12 m |
| Mosquito wet | 9.93 m | 3.36 m |

Rolled up from rest to every walking pace (0.5 to 4.5 kt) and stopped, most
turn off the runway's heading, any pace:

| | calm before | calm after | 15 kt across before | after |
|---|---|---|---|---|
| Mosquito | 34.9 | 8.9 | 59.0 | 5.8 |
| J-3 Cub | 0.1 | 0.1 | 4.4 | 4.2 |
| the eleven nose-wheel landplanes | 1.1 at most | unchanged | 6.6 at most | unchanged |

**Verification.**
- `every_landplane_rolled_out_dry_or_wet_as_15_kt_of_crosswind_arrives_stays_within_5_m_of_the_centreline`
  now asserts the Mosquito both ways: 24 roll-outs asserted, none named, 4
  left out (flying boat, 747-400, F-22A, J-3 Cub) of 16, coverage asserted.
- New, `every_landplane_rolled_up_from_rest_to_every_walking_pace_and_stopped_keeps_within_30_degrees_of_her_heading`
  and `..._in_a_15_kt_crosswind_and_stopped_...` (tests/unit/test_lesson.cpp):
  13 landplanes by 9 paces from rest, rolled up on the throttle's slow ramp,
  held 10 s, handed to the rollout to the stop; heading within 30 degrees of
  the runway's, wheels only, not over, reached and stopped, judged every step.
  Coverage asserted, 117 runs each; the flying boat and 5 kt named.
- **Seen red**: the new tests before the change, the Mosquito 34.9 degrees
  calm and 44.9 to 59.0 across (9 runs); the roll-out test with the tail's
  wait taken out, the Mosquito struck her airframe dry; with her tail
  brought down from 0.9 Vref again, 10.35 m wet. Both reverted, green.
- The full suite on Linux release, 1,421 of 1,421 passed (on the tree
  before its rebase on to main past #284, whose two commits it held).
- Sources: FAA-H-8083-3C chapter 14
  (https://www.faa.gov/sites/faa.gov/files/regulations_policies/handbooks_manuals/aviation/airplane_handbook/15_afh_ch14.pdf);
  Mosquito FB 6 Pilot's Notes, para. 40, as quoted at
  https://www.pprune.org/6939491-post15.html.

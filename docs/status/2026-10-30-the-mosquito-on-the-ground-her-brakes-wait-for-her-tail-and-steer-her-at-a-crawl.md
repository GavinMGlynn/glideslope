### The Mosquito on the ground: her brakes wait for her tail, and steer her at a crawl, 2026-10-30 - two items done, one opened

Two Phase 10b items were left open by the de Havilland Mosquito alone:
"The AI's roll-out drifts ... when a 15 kt crosswind arrives" and "The
Mosquito swings below walking pace". Both are now ticked. Built on #284
(the roll-out's crosswind steering), now on main.

**What is still not true first.**
- **The J-3 Cub's landing is longer: 498 m past the threshold, not 429.**
  The AI wheel-lands her - she touches at 3.1 degrees, and stands at 12.1 -
  and her brakes now wait for her tail wheel to bear weight, so she rolls
  on her main wheels unbraked until it comes down. That is right for a
  wheel landing; landing a Cub on her main wheels at all is the fault, a
  flare change, opened as its own item ("The AI wheel-lands the J-3 Cub").
  Her measured landing distance (assets/figures/j3cub.xml; none is
  published) is measured again with `tools/measured_landing.py`: 1,500 ft to
  1,700, and the copilot test's pin with it.
- **The Mosquito's landing is longer too**: 1,696 m past the threshold,
  not 1,633 (she is wheel-landed and waits for her tail as well); her
  measured landing distance is measured again, 5,500 ft to 5,700, and the
  copilot test's pin with it.
- **The Mosquito's real braking is not sourced.** Her Pilot's Notes (A.P.
  2019E) say that her tail wheel castors ("taxy forward a few yards to
  straighten the tailwheel", para. 40) and that a swing is checked "by
  coarse use of the rudder and by differential throttle"; nothing found
  gives her landing roll. The rules below are the FAA's Airplane Flying
  Handbook's for tailwheel aeroplanes in general, and her 0.7 is this
  model's figure, not a published one.
- **Differential throttle is not flown**: the roll-out's throttle is closed,
  and her brakes do it instead.

**What changed (`sim::Lander`, lander.cpp), for a tail-wheel aeroplane.**
1. **Her brakes wait for her tail**: the autobrake puts on nothing while her
   tail wheel bears no weight (its JSBSim WOW, found from her model's
   contacts), and lets off if braking lifts it. AFH (FAA-H-8083-3C) chapter
   14, wheel landing: "the tail should be permitted to drop on its own
   accord ... deceleration should be allowed to proceed as in a three-point
   landing." Braked tail up from the hand-over, she went over on her nose
   dry. **By weight, not attitude**, which a runway's slope moves.
2. **Where her stick starts back is her figures' data**
   (`stick_back_from_vref` on `<published_figures>`, read by sim::figures,
   refused outside 0.5 to 1; 0.9 where a file gives none, as every aeroplane
   flew before). The Mosquito's is 0.7 (assets/figures/mosquito-fb6.xml says
   why): brought down from 0.9, her wing at 12 degrees carried five sixths
   of her weight at 100 kt and her tyres held little - 9.9 m off wet; held
   to 0.7, 3.4 m. The Cub keeps 0.9.
3. **Her rudder bar's steering brake does not fade at a crawl**
   (`Lander::toe_brakes`, now told whether she is on a tail wheel). A
   nose-wheel aeroplane's still fades below 5 kt to none at a stop. Her tail
   wheel castors and her rudder does nothing at a crawl, so her brakes are
   all that steer her: AFH chapter 14, crosswind taxi "sometimes requiring
   use of the brakes when tailwheel steering alone proves inadequate to
   counteract the weathervane effect". Still six tenths at most.

**Tried and not kept.**
- Brakes waiting for her pitch to come within 2 degrees of the attitude she
  stands at: slope-dependent, and the Cub 549 m.
- Brakes let off while her nose pitched down faster than 2 deg/s (the Cub
  428 m): on a runway rising 1 degree the Mosquito, braked lightly as her
  nose sank under 2 deg/s, struck it, dry and wet; adding "more than a
  degree under the attitude she touched at" did not save her.
- The stick eased fully back once her tail wheel touched (the AFH's
  three-point roll): no shorter for the Cub, who never touches it early.

**Figures (linux-release, this FMA build).** Roll-out as 15 kt arrives
across, worst distance from the centreline, dry / wet:

| | flat | 1 degree up | 1 degree down |
|---|---|---|---|
| Mosquito, before | went over / 9.93 | - | - |
| Mosquito | 2.12 / 3.36 | 1.23 / 2.53 | 3.16 / 4.05 |
| J-3 Cub (left out before) | 1.35 / 1.63 | 0.71 / 0.92 | 3.00 / 3.46 |

Rolled up from rest to every walking pace (0.5 to 4.5 kt) and stopped, most
turn off the runway's heading: the Mosquito 34.9 degrees calm and 59.0 in
15 kt across before, 8.9 and 5.8 after; the Cub 0.1 and 4.2; the eleven
nose-wheel landplanes unchanged, 6.6 at most.

**Verification.**
- `every_landplane_rolled_out_dry_or_wet_as_15_kt_of_crosswind_arrives_stays_within_5_m_of_the_centreline`
  asserts the Mosquito and now the J-3 Cub (left out until now for a ground
  loop in 15 kt across, closed 2026-10-29), and both on runways sloping 1
  degree up and 1 down, dry and wet (`rolled_out`'s new slope). Coverage
  asserted: 13 landplanes both ways, 2 tail-wheel ones on 2 slopes both
  ways, 34 roll-outs asserted, 3 left out (flying boat, 747-400, F-22A).
- New: `every_landplane_rolled_up_from_rest_to_every_walking_pace_and_stopped_keeps_within_30_degrees_of_her_heading`
  and `..._in_a_15_kt_crosswind_and_stopped_...` (tests/unit/test_lesson.cpp):
  13 landplanes by 9 paces from rest, rolled up on the throttle's slow ramp,
  held 10 s, handed to the rollout to the stop; heading within 30 degrees of
  the runway's, wheels only, not over, reached and stopped, every step.
  Coverage asserted, 117 runs each; the flying boat and 5 kt named.
- **Seen red**: the new walking-pace tests before the change, the Mosquito
  34.9 degrees calm and 44.9 to 59.0 across; the roll-out test with no wait
  for her tail, the Mosquito struck her airframe dry; with her stick back
  from 0.9, 10.35 m wet; with the pitch-rate rule in place of the weight,
  the sloped cases red - the Mosquito struck the runway rising a degree, dry
  and wet. Each reverted, green.
- `the_j3cub_lands_within_her_measured_landing_distance` at 1,700 ft (498 m
  measured, 466 to 518 allowed) and the Mosquito's at 5,700 ft (1,696 m).
- New: `where_the_rollouts_stick_starts_back_is_read_and_refused_outside_half_to_all_of_her_reference_speed`
  (tests/unit/test_figures.cpp); seen red with 0.5 let through.
- The full suite on Linux release: 1,418 of 1,422 passed; the Mosquito's
  measured landing distance failed (1,696 m against her 5,500 ft) and was
  measured again; three server tests failed under the suite's load and
  passed alone (the F-35B STOVL take-back, a hand-over model with no key,
  the servers' ChatGPT plan in the air).
- Sources: FAA-H-8083-3C chapter 14
  (https://www.faa.gov/sites/faa.gov/files/regulations_policies/handbooks_manuals/aviation/airplane_handbook/15_afh_ch14.pdf);
  Mosquito FB 6 Pilot's Notes, para. 40, as quoted at
  https://www.pprune.org/6939491-post15.html.

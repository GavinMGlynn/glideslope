### AI landings leave the runway and go around from one not clear; a handed aircraft measured; the F-35B's go-around circuit, 2026-10-08 — one tail done, two narrowed, one untouched

**What it is not, first.** **A handed aircraft is measured, not always
kept apart**: handed over level above the AI's layers it is, but in a run
where its player had rolled it with full aileron for two seconds first, the
AI recovered it from a spiral between two plan-file layers 1,000 ft apart,
where no height is 700 ft from both, and separation from AI 1 was lost for
69.6 s (least 210 ft within 1.5 nm) - the monitor's "squeezed between two"
case. **A copilot's route still cannot end in a landing**: `COPILOT_ROUTE`
carries waypoints only. **The circuit lessons' AI still flies its own copy
of the circuit** (`fly_a_circuit` in `test_lesson.cpp`): not started here
(done since, above).
**Vacating knows no taxiways**: she turns off to the right square to the
runway where she stopped, on to the DEM beside it, which nothing checks for
buildings or water; the window client's offline flight does not vacate (no
other traffic there). A runway's "not clear" is anything the server flies
within 70 m of its centreline from 400 m short of the threshold to its end,
under 100 ft; a departure is not told to wait for a landing, nor a landing
for a departure beyond that.

**What works.**
- **Off the runway after landing** (`sim::Vacate`, `sim/vacate.hpp`): told
  to (`Controller::vacates_runways`, which the server tells every
  controller), an aeroplane the AI has landed and stopped - by the approach
  autopilot or the learnt landing - taxis off to the right at 8 kt over the
  ground, steered by the landing roll's own rudder-and-brake law to a
  heading led round at 10 degrees a second and never more than 10 ahead of
  her nose, and stops, gently (a fifth of the brakes until under half a
  knot), 110 m from the centreline. Asked whether the ground beside the
  runway abeam her is free (`Vacate::SpotFree`: nothing stopped within
  600 m along it), she first rolls on down the centreline until it is, or
  until 400 m from the end. The server says "aircraft 4, c172p (AI 1), has
  left YSSY 16R, stopped beside it".
- **A landing goes around from a runway not clear**: on an approach or in
  the flare, below 400 ft over the ground, a controller told how to ask
  (`Controller::clears_with`) goes around if anything else is on the runway
  (`sim::on_runway`) - from the learnt landing too, which hands her to an
  approach lander to fly the go-around (`Controller::go_around`) - and is
  flown round the go-around's circuit to land again. The server asks of
  every other aircraft it flies, wrecks aside, and says "goes around from".
- **Landed is not traffic**: an AI aircraft rolling out, taxiing off or
  stopped beside the runway gives way to nothing and is given way to by
  nothing, is not counted as separation lost, and does not hold a departure
  or a wreck's flying again.
- **A handed aircraft is measured**: the server's measurement now counts an
  aircraft its player handed to the AI (still in the player's slot, holding
  its course or flying a copilot's route) against every AI aircraft, for
  every step the AI has it, as the monitor already had it give way.
- **The F-35B's go-around circuit**: at 179 kt with her gear up she needs
  15 degrees of incidence to stay level, and 15 degrees is the autopilot's
  highest pitch; the throttle held the speed asked, so on the upwind leg she
  sat level at 1,300 ft for fifteen minutes, the vertical-speed integral
  winding on at the pitch's stop, and then - lightened by her fuel - crept up
  through her circuit height to 6,000 ft with the nose still held up. Now,
  with the nose at its highest and the climb 200 ft/min short, while she
  holds the speed asked, the speed asked rises a knot a second, up to 40 kt
  over it, and falls back as fast once the nose comes down or the climb
  comes: the throttle opens to it and the wing climbs on less incidence. She
  climbs to her raised circuit, flies every leg at 2,002 ft or more over the
  slope, and stops 2,593 m past the threshold, on the centreline. **Tried and
  taken out**: opening the throttle outright at the nose's stop took the
  stall out of the F-15C's stall demonstration, which asks for ten knots
  under the stall ("f15c flew every stage of the stall: 0 of 2") - hence
  "from the speed asked" only; and winding the integral only back from the
  envelope's edge made the F-35B hold 194 kt round her tightest orbit,
  10 kt under the slowest her figures were measured to
  (`the_f35b_holds_nothing_a_plan_asks_one_step_past_...` failed), which
  would mean measuring her plan and glide speeds again: not needed for this
  tail, so left as it was.

**Verification.**
- `every_aeroplane_the_ai_lands_taxis_off_the_runway_and_stops_clear_of_it`:
  all 13 landplanes taught the approach (the flying boat named) landed and
  vacated - 112 to 124 m right of the centreline, 608 to 2,829 m along,
  no faster than 11 kt, clear 45 to 149 s after the stop (the A380 the
  longest). **Seen to fail** before the turn was led: the C172P, C182,
  PA-28, Cub, 787 and A380 stood with the nosewheel hard over and the inside
  brake on and never moved; and with the stop at full brakes the Mosquito
  went over on her nose.
- `an_aeroplane_landing_on_a_runway_another_is_on_goes_around_and_lands_once_it_has_left`:
  two C172Ps; the first landed and held on the runway until the second,
  down the same approach, has gone around, then let vacate. The second goes
  around once, lands with the first clear, never within 500 m of it while
  it was on the runway, and stops 115 m from it - by the approach autopilot
  and handed to the learnt landing at its gate. **Seen to fail** with the
  runway's question never asked: "the second went around once, not 0", the
  first still on the runway at its touch; and with the free-spot question
  ignored: "the second stopped clear of the first: 0.82 m".
- `two_ai_aircraft_on_plans_ending_at_one_runway_both_land_the_second_once_the_first_has_left_it`
  (`tests/cmake/server_two_land_on_one_runway.cmake`): a server with two
  AI C172Ps on `sydney-arrival.plan`, the first put on final to 16R, flown
  120,000 steps: both landed by the learnt landing (216 ft/min, 1.2 m
  across, stopped 476 m along), the first has left 16R before the second
  touches down, both leave it, nothing wrecked, separation never lost. No
  go-around in this run: the first is long gone. 141 s. **Seen to fail**
  before the free spot: the second turned off where the first had and
  "collided with aircraft 4".
- `an_aircraft_handed_to_the_ai_is_measured_against_every_other_ai_aircraft_and_kept_apart_from_them`
  (`tests/cmake/server_handed_kept_apart.cmake`): two plan-file AI
  aircraft and a headless client's aircraft, a thousand feet above their
  layers, handed to the AI two seconds in: three pairs measured, the handed
  aircraft's two over at least 2,760 steps each (3,360 here), separation
  never lost. 33 s. **Seen to fail** with the old rule: "1 pairs were
  measured, not the three". With the client's `--fly` (full aileron before
  the hand-over) it lost separation, as above: the run is not that one.
- `an_aeroplane_going_around_beside_rising_ground_flies_its_circuit_height_over_it_and_lands`:
  the F-35B is no longer named and left out; all 13 landplanes, downwind
  1,264 (the Cub) to 2,016 ft over the slope. **Seen to fail** with the
  autopilot as it was: "f35b did not fly every leg of the circuit in
  order: 3 legs", highest 6,025 ft, not stopped 10.6 km on; traced, she
  held 1,300 ft upwind for 900 s first.
- Unchanged and green after both changes: every test matching autopilot,
  orbit, glide, climb, stall, instructor, circuit, plan, land, hand,
  take-over, leave, stop, go-around, taxi and separation, but the window
  client's GPU tests and the live-model ones: 399 of 399, linux-debug.

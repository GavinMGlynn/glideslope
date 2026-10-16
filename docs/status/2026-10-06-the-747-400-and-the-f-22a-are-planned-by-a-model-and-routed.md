### The 747-400 and the F-22A are planned by a model and routed by a copilot: their take-off speeds measured, 2026-10-06 — tail done

**Why they could not be.** A model's plan takes off (`copilot/planner.cpp`
refuses one that does not), and a take-off is flown from
`sim::departure_speeds`: a rotation and a climb-away speed, each from a
published figure. The 747-400 publishes only a field length and an
engine-out gradient, the F-22A only a ceiling climb with no speed, and their
measured stalls moved 14 and 21 kt with the entry speed ("A measurement that
moves with how you approach it", below). The planner and the copilot were
also given an approach speed, which neither has.

**The owner's decision** (REQUIREMENTS.md section 9): measure them in the
sim, as the plan speeds were. `glideslope_cli takeoff-speeds AIRCRAFT
[FROM_KT]` (`sim/takeoff_trial.hpp`) takes the aircraft off a level 3,500 m
runway at sea level in calm air, at the weight its model flies a plan at,
with the take-off autopilot to 500 ft, at its take-off field length's flap
or none:
- **the rotation**, sought up from 100 kt by 5 kt, each climbing away at 20
  more: the first that *lifts off* - handed over unwrecked, off the ground
  inside the runway and within 15 kt of the rotation (lifted by it, not by
  the speed it ran on to), never 5 kt slower once off. Written with 5 kt to
  spare, plan-speeds' rule.
- **the climb away**, asked from 10 to 100 kt over that: the slowest speed
  it was at at 500 ft, rounded up to 5. Neither can be held slower than its
  own - the take-off autopilot's nose limit: the 747-400 was at 187 to
  203 kt whatever under 195 was asked, the F-22A at military power 216 kt
  asked 125 and faster the faster asked.

Measured on linux-debug, written in `<takeoff_speeds>` in each figures file,
labelled measured, with what was flown:
- 747-400: rotation held from 155 kt (100 to 150 ran on and lifted at about
  167), written 160; climbs away at 187.1, written 190; flaps 20, 754,313 lb.
- F-22A: rotation held from 100 kt (it leaves the ground at 112), written
  105; climbs away at 216.3, written 220; no flap, 61,570 lb.

`sim::departure_speeds` uses them where no climb figure is published
(`DepartureSpeeds::measured_from_model`); `rotate_is_published` is false.
They still have no approach speed and are not landed by the AI. **The
lessons and the take-off-at-every-loading tests leave them out as before**,
named with the reason (`book_departure_speeds` in test_lesson.cpp): the
speeds are measured at one weight for a plan, and a lesson teaches the book
at every loading it names - rotated at 120.8 kt, scaled to its light
loading, the 747-400 left at 132, 11 past.

**Planned and routed without an approach speed.** The briefs the client,
the server and glideslope_cli give the copilot and the planner are now made
in one place, `frontend::brief_for` and `plan_request_for`
(`src/frontend/briefs.cpp` - the copilot library sees no figures file):
an approach speed only where the aircraft publishes a stall. With none,
the model is told "N kt climbing away" in place of "A kt on the approach, N
kt best climb", is not told the clean-flight floor reason, and a glide is
allowed between its climb-away speed and its slowest plan speed
(`copilot::glide_speeds`) - for the 747-400 190 to 220 kt, the F-22A 220 to
255. Every aircraft that has one is told exactly what it was, so no
recording changes.

**Verified** (linux-debug):
- `the_747_and_the_f22_take_off_at_the_speeds_measured_from_their_models`:
  every landplane whose speeds are measured is walked and they must be those
  two; each flown at its file's speeds holds (`TakeoffFlown::held`). Red
  with the 747-400's rotation made 140 ("1 of 2": off at 168 kt).
- `every_aircraft_is_planned_and_routed_from_its_own_speeds_with_or_without_an_approach_speed`:
  all 16 aircraft briefed from the data; for each a stand-in model's plan
  off Sydney's 34L at its slowest planned speed, to a waypoint and its
  tightest orbit, is taken first time; its copilot takes a route at that
  speed, and with the engine stopped a glide at the middle of its glide
  speeds; none is told "0 kt"; the 747-400 and F-22A are the two with no
  approach speed. Coverage asserted (16 of 16). Red with the approach speed
  asked of every aircraft ("747-400 publishes no stall speed"). **The jets'
  glides are accepted, not flown**: a glide between the 747-400's and the
  F-22A's climb-away and slowest plan speeds (190 to 220, 220 to 255 kt) is
  taken by the check, but whether either glides round its tightest orbit
  there is not measured - the open tail "A glide may still be asked of a
  jet at its approach speed" covers it.
- `the_measured_take_off_speeds_in_the_747_and_f22_figures_are_what_the_measurement_makes`:
  the measurement moved into the simulation (`sim::measure_takeoff_speeds`,
  which `takeoff-speeds` prints) is run again for each, and must give its
  file's rotation and climb within the 5 kt they are rounded to, its flap
  and its weight to the pound. Red with the F-22A's climb hand-edited to 230.
- **A model's own plan flown, end to end**:
  `take_off_climb_to_3000_ft_and_orbit_the_cbd_is_planned_for_the_747-400_by_anthropic_as_recorded_and_flown`
  and `..._for_the_f22_...`, as the Cessna's: Claude (claude-haiku-4-5)
  asked live with the owner's key, recorded (`tests/data/copilot/cbd-orbit-747-400-anthropic.jsonl`,
  `cbd-orbit-f22-anthropic.jsonl`; scanned, no keys), and played back
  `glideslope_cli plan` then `fly-plan` over the DEM. The 747-400's first
  plan was refused (its orbit at 190 kt, under its 220 floor) and the second
  taken; the F-22A's first. Each took off 16R, handed over at 1,001 ft above
  the runway, and flew two turns of its 7.0 and 9.4 km circle at 2,983 to
  3,019 ft - **but 460 m off its circle at worst** (6,904 to 7,467 m; 9,229
  to 9,850 m), against the 60 m a Cessna is held to, so they are held to
  500 m (`CIRCLE_M`) and that is a tail. Played back with the request's
  words changed, red ("it must be recorded again").
- `the_airliners_roll_down_sydneys_16r_on_the_dem_and_those_that_can_climb_away_fly_their_plan_to_its_orbit`
  and `the_military_jets_...` (main's five groups, rebased onto) now take
  the 747-400 and F-22A off 16R too and fly the CBD orbit plan to its
  orbit - none is only rolled. Green, with the stall, orbit and take-off
  lesson tests (`every_landplane_takes_off_at_every_loading_...`, the take-off
  and climb lessons, the instructor's demonstration).
- An aircraft with no climb speed is still refused: the test that showed it
  with the 747-400 now uses its figures with `<takeoff_speeds>` taken out.
  So does the server's
  `a_flyable_route_is_refused_when_its_speeds_are_not_known_and_the_server_goes_on`
  (`cmake/server_copilot.cmake`, FOR=no_speeds), which ran both programs on
  the data as it is and went red on CI once the 747-400 had speeds: it now
  runs them on a copy of the data with them taken out.
- `no_aircraft_holds_what_a_plan_asks_one_step_past_its_slowest_or_fastest`,
  which timed out once on CI, has a 1,800 s TIMEOUT and an entry in every
  `tests/ci_costs/*.txt` - **estimates**, scaled from its 232 s in a local
  linux-debug by the ratios of tests measured both places, until
  `tools/ci_test_costs.py` measures it.

**Not done**: a model's own plan is flown end to end for the Cessna, the
747-400 and the F-22A only; the other thirteen are planned by a stand-in
model and flown by the take-off from 16R to the CBD orbit plan's orbit and
the orbit tests at their plan speeds. The jets' orbits entered from a
waypoint swing 460 m off their circles.

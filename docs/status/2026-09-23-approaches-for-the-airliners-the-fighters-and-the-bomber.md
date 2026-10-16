### Approaches for the airliners, the fighters and the bomber, 2026-09-23

**Every aeroplane that publishes or has measured a landing stall now flies
its class's approach lesson to an empty debrief**: the 737-300, 787-8, A320
and A380, the B-2, the F-15C and F-35B, the Learjet, the Mosquito and the
four light aircraft - thirteen. The 747-400 and F-22A are left out by name:
neither has a reference speed. What it took:

- **An approach starts established on it.** `InitialConditions` gained
  `flaps` (the landing flap already down, set in one step by running the
  initial conditions in trim mode), `flight_path_deg` (already on the
  three-degree path, not level at its height) and `trim` (JSBSim's
  longitudinal trim for that path). Started clean at its landing speed the
  A380 stalled before its flaps were a third out and fell 688 ft in ten
  seconds; started on the path but untrimmed - nose on the path, no angle of
  attack, no lift - the F-15C dropped at 41 ft/s and ran from 196 knots to 208
  before the elevator caught it. JSBSim cannot trim the B-2 or the Mosquito;
  they start as they did, the Mosquito's engines started again the way every
  constant-speed propeller's are, and the test says which started untrimmed.
- **The approach autopilot learns the attitude the path needs.** Its pitch
  was proportional to the vertical-speed error alone, so an aeroplane that
  flies the path nose-up could hold that attitude only by sinking faster than
  the path: the F-35B needs about twelve degrees at its reference speed, which
  took 2,000 ft/min of error, and the limit was ten. A slow trim now learns the
  attitude, from the one it has on the first step, and the limit is fifteen.
- **Power for the path on the back of the drag curve.** A sink more than 300
  ft/min faster than the glidepath asks opens the throttle in proportion. On
  its path an approach never comes near that margin.
- **Every approach lesson watches the descent**, `velocities/h-dot-fps >=
  -30` by the threshold: every stage ends on a height, and a crash reaches a
  height as surely as a landing does. The A380 that fell 688 ft was passed
  through all three stages.

**Verification:** `the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief`
flies all thirteen: the airliners within 3 to 6 knots of their reference
speed, the F-15C 195 to 198 against 196, the F-35B 159 to 166 against 159,
none sinking faster than 35 ft/s. **Not yet done:** the fault test still flies
only the Cessna.

### Take-offs for the jets, 2026-09-23

**Thirteen aeroplanes now take off to an empty debrief**, in six classes: the
airliners, the business jet, the fighters and the bomber join the light
aircraft and the Mosquito. What it took:

- **A jet rotates from its stall at its take-off flap.** `departure_speeds`
  worked every rotation from 1.15 times the cleanest published stall - for an
  airliner its landing stall, flaps fully down - and took off with the flaps
  up: a 737 would have been asked to leave the runway at 121 knots with a
  clean stall of about 155. An aeroplane with a published take-off field
  length now rotates at 1.15 times its stall at that field length's flap,
  takes off with that flap, and climbs out at `initial_climb_kts`, V2 and ten.
  The airliners' stalls at their take-off flaps are **measured**, as their
  landing stalls were - 737-300 131.25 at flap 5, 787-8 151.29 at 5, A320
  125.38 at 10, A380 136.76 at 17, at their maximum loading, gear down, each
  from entries of 160, 200 and 240 knots (the 787's 160 too close to its stall
  to use) - and written into their figures as `stall_speed_takeoff`.
- **Military power, not afterburner.** Full throttle for the take-off
  autopilot is 0.99, below which JSBSim does not light an afterburner. T.O.
  1F-15A-1 gives MIL as a normal take-off; in afterburner the F-15C gained 24
  knots a second. And its climb away is 350 knots in its manual, which the
  fighter lesson's band - its climbing speed to 180 above it - allows.
- **Each class's lesson judges its own climb-out**: an airliner's and a
  business jet's against V2 (the rotation speed and five up), a fighter's and
  a bomber's against their climbing speed, which they accelerate straight to.
  The bands are measured, with room.

**Faults.** Part throttle is flown and named in all six classes. Rotating
early is flown with half the stick back from 85 percent of the rotation speed
- the F-15's manual's normal technique - and is named in the airliner, the
bomber, the light aircraft and the Mosquito. **The F-15C, F-35B and Learjet
35A are left out of it by name**: their noses do not come up until well past
their rotation speeds whatever the stick does - off at 220, 213 and 161 knots
pulled early, 230, 213 and 160 by the book, against 174, 141 and 125. The
F-15C's manual (figure A3-6) has its nosewheel off at about 130 and the
aeroplane off at 157 at 55,000 lb, so that is the models' and is a tail.

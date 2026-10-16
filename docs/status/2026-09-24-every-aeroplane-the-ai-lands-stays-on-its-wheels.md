### Every aeroplane the AI lands stays on its wheels, 2026-09-24 — tail done

**The Learjet was no longer ending nose down; three jets were bouncing, and
nothing looked.** The light aircraft's landing tests asked which way up she
stopped; the approach lesson, its demonstration and the circuit landed
fourteen aeroplanes each and asked nothing of the roll - and the approach
lesson stopped watching at thirty knots. All three now watch every step from
the first touch (`tests/unit/after_touch.hpp`): under 15 degrees of bank,
the nose above 10 degrees down, no more than 3 ft back into the air, and
nothing but wheels on the runway - JSBSim's own contact points for a tail, a
wingtip or a nose (a flying boat's hull on the water excepted). The approach
lesson now flies on from the end of its last stage to the stop. Each test
gathers every aeroplane's faults and fails once, naming them all, and states
its count: 14 of 16 each (the 747-400 and F-22A have no reference speed, so
the AI cannot land them at all).

**Run on the lander as it was**, it went red at once: the 737-300 rose
12.4 ft after touching, the A320 12.3 and the B-2A 20.7, in the approach
lesson; the 737 11.5 and the A320 12.6 in the circuit. The Learjet was level
(-0.0 degrees) in both - the rollout rewritten earlier today had already
cured it. **The cause**: the lander landed every aeroplane as the FAA's
Airplane Flying Handbook (FAA-H-8083-3C) lands a tailwheel aeroplane,
holding the attitude it touched at while it was fast. A jet touching at its
reference speed is still carried by its wing, and rode its gear's rebound
back into the air. What `sim::Lander` does now, each from a source:

- **A jet is landed as chapter 16 of the handbook lands a jet** ("Touchdown
  and Rollout"): the nose-wheel lowered immediately - its landing distance
  charts assume within four seconds; here in three, to a degree below level,
  on three times the flying law's pitch gain, so a nose still rotating from
  the flare is stopped - the spoilers out at once, and the brakes on from the
  touch rather than below nine tenths of the reference speed. A jet that has
  touched and come up again is not flared again: her nose goes on down and
  her spoilers stay out. "A jet" is read from the model: every engine a
  turbine (`AircraftFigures::jet`).
- **The A320's model had no ground spoilers.** Its speedbrake only added
  drag, over two seconds; with its gear damped at a third of critical it
  still rode the rebound 4.1 ft up however its nose was lowered.
  `tools/make_a320.py` now gives it ground spoilers - out with the lever
  while a wheel has weight on it, taking the lift down as JSBSim's own 737's
  do, in 0.6 s, to six tenths of it (docs/ASSETS.md).
- **The wings are held level on the roll with the flying law's gains**,
  angle and rate - the handbook's chapter 9 uses the ailerons on the ground
  "in much the same way they are used in flight". And **the centreline is
  regained over three seconds of travel**, not at two degrees of heading a
  metre whatever the speed: with its nose-wheel down at 190 knots the F-15C
  steered that hard, weaved either side of the line with a growing swing,
  banked 17.8 degrees in the approach lesson and 21.7 in the circuit and put
  a wingtip on the runway. Below 19 knots the law is the old one.

**Verification**, `the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief`,
`the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief` and
`an_instructor_demonstrates_an_approach_and_hands_it_over` (to the hand-over,
where the demonstration's landing ends), with the light aircraft's two: every
one upright on its wheels. Worst of the 42 lesson landings: bank 9.3 degrees
(the Short S.23, on the water), nose 3.0 degrees down (the C182), 1.8 ft up
(the B-2A in the approach lesson, touching fourteen knots fast - a tail);
every jet but the B-2A under 0.2 ft. Each rule was seen red: the bounce on
the old lander, as above; the bank and the wingtip on the F-15C before the
steering was changed; and with the light aircraft's rollout stick held fully
forward as a deliberate bug, the PA-28's nose went 12.3 degrees down and the
Mosquito put its airframe on the runway. The jets' circuits stop 1,591 to
2,706 m beyond the threshold of a 3,000 m runway, the F-35B's aside.

**Found on the way** (tails): the F-35B's circuit touches down 2,109 m short
of the threshold, at 165 knots, and rolls on to the runway - it did so before
this change too, and the circuit test asks only how far across she touched;
taking an aeroplane back on its landing roll hands it the plain autopilot,
not the landing, and it never stops; and the B-2A cannot slow on the approach.

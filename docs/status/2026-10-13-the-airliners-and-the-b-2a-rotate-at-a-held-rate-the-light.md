### The airliners and the B-2A rotate at a held rate; the light aeroplanes, fighters and Learjet not yet, 2026-10-13 — item still open

**What is still not true first.** The rate is held only for the six
aeroplanes whose figures give one: the 737-300, 747-400, 787-8, A320, A380
and B-2A. **The C172P, C182, PA-28, F-15C, F-22A, F-35B and Learjet 35A
still rotate under the old law**, with the A320-like overshoot it allows
(the C182 measured at 8.2 deg/s, the F-35B at 8.6, the Learjet at 8.6), and
the item stays open for them. The tail-wheel aeroplanes and the flying boat
are not rotated on a nose wheel and are not held.

**What landed.**
- **A figures-parser test**,
  `the_rotation_rate_a_take_off_is_held_to_is_read_and_refused_outside_0_to_10`:
  the A320's rate read as three, the C172P's as none, ten read back, and
  nought, -3, 10.5 and 40 refused, each naming `rotation_degps`.
- **Each held aeroplane's rate is data**: `rotation_degps` on her figures
  file's root, cited there, read into `DepartureSpeeds::rotation_degps` and
  `rotation_held`. Three degrees a second for all six: Boeing's flight crew
  training manuals rotate "at 2 to 3 degrees per second"; Airbus asks about
  three (Airbus Safety First, "A Focus on the Takeoff Rotation"); the B-2A
  takes an airliner's, nothing found giving hers.
- **The rate is held on her wheels** (`sim::Departure`): the pull builds at
  the whole stick a second until her nose comes at a quarter of her rate
  (half, as before, for an aeroplane not held); from there the stick is her
  pitch rate's, taken over where it was - 0.3 of stick per degree a second
  off the rate, at once, and the pull under it moved by two rates' worth a
  second per degree a second off. When the rotation begins is worked from
  her own rate. Tried and dropped: a lead on pitch acceleration (the PA-28's
  gear rings her pitch rate at ±4 deg/s, and the derivative drove her stick
  stop to stop).
- **The test**,
  `every_aeroplane_given_a_rotation_rate_rotates_on_her_wheels_at_every_loading_no_faster_than_it`:
  the six at their 24 loadings, at a plan's speeds; the rate is her pitch
  change over each quarter second wholly on the runway (some wheel on it
  at every step) once rotating, one degree a second of tolerance, and no
  less than half her rate, so a loading never measured cannot pass; the ten
  others named, with why. Seen red
  with the hold switched off: all 24 loadings too fast (and in round one, with
  every nose-wheel aeroplane given a rate, 40 of 42). Worst now: A320 light 3.62
  against 3 (was 7.51), A380 light 3.60 (was 7.60), 747-400 light 3.88,
  737-300 light 3.53, B-2A light 3.27, 787-8 light 3.15. Every one leaves at
  least 2.9 degrees below her strike attitude; every airliner within 5 kt of
  her rotation speed but the 747-400.
- **The 747-400 leaves 11.1 kt past her rotation speed at her light
  loading** (4.6 degrees nose up, 131.9 kt against 120.8), 7.3 at her
  model's. Her rotation speed is measured at one weight, 754,313 lb, and
  scaled by the square root of her weight; she leaves at four to six
  degrees, her lift-off set by her speed rather than her attitude, and at
  her light weight she gains speed fastest through the rotation. Within the
  15 kt her trial allows; not traced further.
- **Her measured speeds, re-measured** (`glideslope_cli takeoff-speeds
  747-400 100`): the rotation is unchanged, 155 the first that holds and 160
  written; the slowest climb away fell from 165.2 kt to 163.8 with her
  rotation held, which rounds up to 165 - five over her rotation speed, and
  her plans' climb floor with it. **The measurement now writes no climb
  away within ten knots of the rotation speed** (`measure_takeoff_speeds`):
  14 CFR 25.107(c)(2) puts V2 above VR by the speed she gains before 35 ft,
  and ten knots is this project's floor for it. Hers stays 170, and her
  recorded plan, whose prompt carries it, is unchanged. The F-22A's, 220
  against 110, is untouched by it.
- **Rotating early is judged by where she was rotated**
  (`a_take_off_flown_with_one_fault_has_that_fault_in_its_debrief`): her
  nose a degree above where she rolled at 20 kt more than five knots sooner
  than by the book, and short of her rotation speed - against the book's own
  rotation, because the book begins its rotation short of that speed by what
  she gains while the nose comes up (the 737-300 by the book has her nose up
  at 141 kt against 151). Not by her lift-off, which the held rate rightly
  moves: the 737-300 hauled back early now flies off 2.4 kt sooner than by
  the book, not 3. A tail-wheel aeroplane, whose nose never comes up on her
  wheels by the book, is judged as before by her lift-off; every nose-wheel
  aeroplane's nose must have come up by the book, so none falls back to it.
  The debrief still names the fault. Seen red with the early pull at the book's own rotation:
  "737-300 was rotated early: her nose up at 141.2 knots, by the book at
  141.2". (Read from where she stands, the 737's nose was up at 1.5 kt: she
  settles more than a degree on her gear as she moves.)

**Why the rest are not held, from three rounds of trying (all backed
out).**
- **The C172P handed over at lift-off touched the runway again.** Held at
  4 deg/s her elevator is at its stop from her nose wheel's lifting at 44 kt
  to her lift-off at 56.7, and she left with 0.975 of stick; the hand-over
  takes the stick as trim (`Departure::from_lift_off`), and in 0.15 s her
  nose went from 7.9 to 12.3 degrees and her main wheels struck twice more.
  Under the old law her stick at lift-off is 0.13 and she climbs away at 9
  to 10 degrees. Tried, each still touching: the stick handed over as pull
  rather than trim; her rate at 3; the pull capped at the one that lifted
  her nose wheel, scaled with dynamic pressure; the rate aimed at a 10-degree
  take-off attitude (the F-35B then left 19.5 kt late); and releasing the
  rate to the attitude law once her gear carried under a quarter of her
  weight - that let her fly herself off, but released the fighters, the
  787-8, the 747-400 and the PA-28 far too early: the F-35B left 50 kt
  late, and the 787-8 and the B-2A handed over then touched down.
- **The F-22A's own pitch loop winds up on her wheels**: her stick commands
  a pitch rate, its integrator winds to full stabilator while her nose wheel
  holds the nose, and she came up at ten degrees a second whatever her stick
  did. A ground mode in her model (the stick direct to her stabilators on
  her wheels, her pitch loop blended back in off them, as Airbus's normal
  law's ground mode) brought her combat loading to 4.4 deg/s, but her
  model's loading still peaked at 13.1, untraced, and moved her measured
  rotation speed from 110 to 140. Left out of this change.
- **The Learjet at three** left 11 kt past her rotation speed at her light
  loading; at four she held, with the F-35B 7.4 kt late and the F-15C 6.9.

**Verification.** Every test matching
"take|rotat|lift|hand|depart|climb_away|strike|lesson|fault", with the
server and client built: 181 tests, 172 pass and nine are skipped for want of a language model's key, none red. The selftest hash does not move:
182dd6c996e0ee4c.

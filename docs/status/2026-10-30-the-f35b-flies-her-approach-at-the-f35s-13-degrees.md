### The F-35B flies her approach at the F-35's published 13 degrees and flares with her throttle closing, 2026-10-30

**What is still not true first.**
- **The 13 degrees and the 152 knots are the F-35A's.** No F-35B approach
  angle of attack or speed is published (the owner, 2026-10-09; searched
  again today). The A's are, and a Lockheed test pilot says the B lands
  like her. The weight that goes with 152 knots is not published either.
  Her combat loading, 39,750 lb, is this project's choice.
- **Her extra lift is a number set to fly that figure.** It is not measured
  aerodynamics. Her lift over the wing's DATCOM slope is 1.6 times
  (`LIFT_FACTOR`, tools/make_f35b.py), for the body, chines and wing root
  that DATCOM leaves out and nothing publishes.
- **Her take-off lift-off speed was not measured again.** `lift_off_kcas`
  189 rests on the 172 kt unstick speed measured before her lift was raised.
  She would now unstick slower. Her ground roll to 189 is measured again,
  but the speed itself is not.
- **The short take-off was not swept again.** It is 475.6 ft at the old
  angles and gains, against 592.5 before.
- **A route may plan her glides from 163 kt, but 163 to 183 kt depart.**
  Her figures' slowest glide is 188 kt.

**Sources** (assets/figures/f35b.xml `<approach>`, tools/make_f35b.py):
- United States Air Force, Aircraft Accident Investigation Board Report,
  F-35A T/N 12-005053, Eglin AFB, 19 May 2020, released 30 September 2020
  (afjag.af.mil; the PDF read from the Internet Archive's copy, SHA-256
  19f54402cda4dace1960d8467a36ecd7cca2daeecff38a3b8ac78e9fd1ee3d21).
  Page 21: "the shallow (5.2) AOA at touch down is below the recommended
  landing attitude of 13 AOA". Page 6: the 202 KCAS touchdown is
  "approximately 50 KCAS faster and 8 degrees AOA shallower than the on
  speed parameters for the MA gross weight at the time of landing". That
  puts on-speed at about 152 KCAS and 13 degrees, at a weight the report
  does not give.
- Lockheed Martin, Code One, "F-35 Lightning II Flight Testing At NAS
  Patuxent River", 1 December 2012. The test pilot Dan Levin: "Landings in
  the A and B are similar to the F-16 in terms of speed and angle of attack."
- Cross-check: Eurocontrol's Aircraft Performance Database gives an F-35's
  Vat as 150 kt IAS, with no variant named. Marked "only indicative".
- Searched and not usable: F-35B NATOPS (not public); DOT&E reports;
  Aviation Week's 2010 F-35C first-flight story, quoted on forums as "155 kt
  for the F-35A and B" (paywalled, so not read); the F-35C's on-speed AoA
  (no primary source found, and the C lands at a lower AoA, Levin says);
  F-16.net forum estimates.

**Why her model needed 19.5 degrees.** Her lift was DATCOM's slope for her
trapezoidal wing alone: 2.97 a radian at Mach 0.2 on 460 sq ft, which is
0.67 at 13 degrees. Coming down a 3-degree path at 152 kt and 39,750 lb, she
needs about 1.05. At 1.3 times her measured stall, 159 kt, she flew 17.1 to
21.6 degrees of alpha. That left her flare no nose to raise before her
tail's 21.6 degrees, so the lander opened her throttle to arrest the sink.
No flap or droop within her flaperons' travel makes up the difference: 20
degrees of droop is worth 0.16.

**What changed.**
- **tools/make_f35b.py: `LIFT_FACTOR` = 1.6** on her lift table at every
  Mach, and on the lift slope her transition law reads
  (`fcs/transition-lift-slope-per-rad`). 1.55 gave 12.0 to 14.8 degrees
  down the approach. 1.6 gives 11.6 to 14.4, centred on 13.
- **assets/figures/f35b.xml: `<approach kcas="152" loading="combat">`.**
  The approach autopilot flies her at 152 KCAS, scaled for her weight, in
  place of 1.3 times her stall.
- **Her measured figures, measured again** (linux-release; each figure's
  text gives the old value):
  - stall: 97.44 KCAS, was 122.24
  - short take-off: 475.6 ft, was 592.5; still inside the required 600
  - take-off ground roll: 1,880 ft, was 2,008
  - climb at 200 kt: 20,646 ft/min, was 20,390
  - slowest plan speed: 163 kt, was 204
  - slowest glide: 188 kt, was 204
  - landing distance: 8,500 ft, was 9,200 (`glideslope_cli land f35b`
    stops her 2,523 m in after touching 350 m in at 177 ft/min; it was
    2,720 m, after a 437 ft/min touch)
  - maximum Mach (1.61) and range (1,388 nm) stay in range: her drag is on
    the lift's square, so at a given lift they do not move.
- **The test**:
  `the_f35b_flies_her_approach_at_the_f35s_landing_incidence_and_flares_with_her_throttle_closing`
  (tests/unit/test_lesson.cpp). It checks that her figures give 152 KCAS,
  and that from two miles out to fifty feet she holds 13 degrees within
  two. From the flare's first step to the touch, her throttle is never
  opened past where the flare found it, and she touches with less. Then she
  is landed by the book, to a stop. Now: alpha 11.6 to 14.4, throttle
  0.457 at the flare, never higher, 0 at the touch.
  **Seen red on the old model.** With the old lift and figures (approach at
  159 kt), it failed on alpha 17.1 to 21.6. On that flight the flare's
  throttle went from 0.441 to 0.703 and was 0.511 at the touch, so the
  throttle checks would also have failed. With the new figures and the old
  lift, she could not fly 152 kt at all (alpha to 26, 0 of 3 stages).
- **The FlareWatch in test_lesson.cpp** now also records the throttle at the
  flare's start, at its most, and at the touch.
- Renamed: `the_f35b_glides_round_its_tightest_orbit_from_188_to_204_kt_without_stalling`
  (was "at 204 kt"). Its CI costs are five times the old, for five speeds
  in place of one. The copilot's F-35B runway need is 8,500 ft.
- **src/sim/lander.cpp is unchanged.**

**The selftest hash** does not move: the selftest flies the C172P.

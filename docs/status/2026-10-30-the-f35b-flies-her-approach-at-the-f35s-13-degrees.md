### The F-35B flies her approach at the F-35's published 13 degrees and flares with her throttle closing, 2026-10-30

**What is still not true first.**
- **Her extra lift is fitted to the accident report's figure, not
  derived.** `LIFT_FACTOR` 1.6 (tools/make_f35b.py) is the number that
  makes her fly 13 degrees at 152 KCAS at her combat loading. The 13
  degrees is its input, not a check of it.
  - Raymer's wing-body method can't produce it: the exposed-area share
    times the fuselage factor 1.07 (1 + d/b)^2 is held under 1.
  - No figure for the vortex lift of her chines and wing root, or of any
    chined forebody like hers, was found.
  - Lockheed's flight-test lift curve (AIAA 2018-3679, figure 11) has no
    numbers on its lift axis.
  - No F-35 stall or minimum speed is published, so her new measured 97
    KCAS stall (was 122) has nothing to check it against.
  - **The owner accepted the factor on 2026-10-30, asked directly**, as it
    is stated here: fitted to the accident report's 13 degrees of angle of
    attack at about 152 KCAS.
- **The 13 degrees and the 152 knots are the F-35A's.** No F-35B approach
  angle of attack or speed is published (the owner, 2026-10-09; searched
  again today). The A's are, and a Lockheed test pilot says the B lands
  like her. The weight that goes with 152 knots is not published either.
  Her combat loading, 39,750 lb, is this project's choice.
- **Her take-off lift-off speed was not measured again.** `lift_off_kcas`
  189 rests on the 172 kt unstick speed measured before her lift was
  raised. She would now unstick slower. Her ground roll to 189 is measured
  again, but the speed itself is not.
- **The short take-off was not swept again.** It is 475.6 ft at the old
  angles and gains, against 592.5 before.
- **A route may plan her glides from 163 kt, but 163 to 183 kt depart.**
  Her figures' slowest glide is 188 kt.
- **The multi-process tests were last run on the first commit**, in the full
  linux-release suite. Every F-35B server and client test passed then. The
  second round's run left them out.

**The second round's draft CI (run 40992479828) was red on two linux-debug
tests; the third round looked at both.**
- `every_aircraft_at_every_loading_at_three_heights_and_three_speeds_is_trimmed_as_a_players_is`
  timed out at 900 s, where it usually takes 374. **The F-35B is not why.**
  - Timed point by point on this branch (linux-release), the test passes in
    about 54 s, its usual cost. All 36 of her points together take 0.09 s.
    The S.23 takes 46 s.
  - That shard ran slow throughout, on tests that fly no F-35B: the 737-300's
    orbit took 373 s (usual 145), the F-22A's fastest orbit 284 (180), and
    the approach lesson 274 (159). That is 1.6 to 2.6 times usual, and 374
    times 2.4 is 900. Nothing was changed for it, and the timeout was not
    raised.
- `a_client_joining_an_f35b_mid_bank_with_the_first_datagrams_lost_still_begins_on_the_servers_law_state`:
  her predicted pitch was 0.624 degrees off, against its 0.5. The test failed
  on main once too (0.592, run 40800355368). Run ten times each, linux-debug,
  the machine otherwise idle, on main's model and circuit and on this branch's
  (pitch within, degrees, in run order):
  - main: 0.138, 0.202, 0.200, 0.216, 0.285, 0.190, 0.204, 0.299, 0.205,
    0.175. All passed; median 0.203, most 0.299.
  - this branch: 0.206, 0.227, 0.205, **0.575**, 0.335, 0.211, 0.217, 0.213,
    0.204, 0.166. One failed; median 0.212, most 0.575.
  - The typical run is the same, but the branch's tail is longer. Ten runs
    each cannot say whether her lift causes it, but it may widen the
    spread. The same law-state drift fails on main.
  - Round 3's draft CI (run 41006534168) failed it again, at 0.572. That
    makes three CI failures for the one cause #273 traced: 0.592 on main
    (40800355368), then 0.624 (40992479828) and 0.572 (41006534168) here.
    It failed again on main's model in #283, which moved it to nightly;
    this pull request leaves it there.

**Four tests her new lift turned red, fixed in the second round.** In the
first commit's full linux-release suite, 7 of 1,417 tests failed: these
four, the crosswind sweep's naming of her (below), and one windowed-client
test (last item). None of the four was loosened; each was red before it was
fixed.
- **Her circuit's final** (`an_instructor_demonstrates_a_circuit_and_hands_it_over`,
  `the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief`). She
  joined final at 136 kt with her nose at its stop, against her 152 vref
  and the lesson's floor of 142.
  - The go-around circuit (`sim::GoAroundCircuit`, src/sim/circuit.cpp)
    flew base and the turn on to final at 1.4 times her landing stall. That
    is the approach speed's 1.3 and a little only while the approach speed
    is 1.3 times the stall.
  - `base_kts` now keeps the same margin over a manual's faster approach
    speed: her vref times 1.4/1.3. Only the F-35B is changed by it. The
    F-15C's manual speed is below 1.3 times her stall, and no other model
    gives a manual approach speed.
  - Her final is now 148 to 164 kt.
- **The over-rotated take-off**
  (`an_over_rotated_take_off_strikes_the_tail_and_its_debrief_says_so`).
  - With her lift, her stabilator cannot raise her nose on the runway until
    about 150 kt, and by then she flies off at 16 degrees. Held fully back
    from the roll at any throttle from 0.3 to full, she never struck.
  - The situation is now built in STOVL mode. She converts on the brakes
    with her short take-off's nozzle and fan at 42 degrees. From brake
    release her stick and her thrust split are fully nose up.
  - She strikes at 21.8 degrees on her wheels at 15 kt (her contacts say
    21.6), and her debrief names it.
- **The climb-speed raise**
  (`the_speed_the_autopilot_raises_for_a_climb_never_passes_the_fastest_she_may_hold`).
  - At her figures' speed she climbs round her circuit with her nose short
    of its stop, so nothing was raised (0 kt) and the clamp went untested.
  - She is now told an approach speed of 1.3 times her stall. Her circuit
    is flown at 146 kt, and the raise reaches the clamp's 11.00 kt and no
    further.
- **The pitch-envelope recovery**
  (`an_autopilot_handed_a_fighter_climbing_past_its_pitch_envelope_brings_her_back_to_her_height`).
  - Pulled to 25 degrees she now climbs 7,303 ft/min, not 5,100. She
    overshot by 1,186 ft, then came down at the autopilot's 700 ft/min
    descent. That is 341 ft off 90 s on whatever the seeding, because the
    time is too short to lose 1,186 ft at that rate.
  - She is now pulled to 20 degrees, as the F-15C is. She climbs 4,736
    ft/min, overshoots by 845 ft and is 79 ft off 90 s on. The bounds are
    unchanged.
- **The windowed client's segfault is not hers.**
  `the_client_with_the_window_joins_the_server_server_txt_names`:
  - The client of its second run exited with a segmentation fault, after
    "tiles.maps.eox.at/.../7/88/234.jpg: given up, unfinished".
  - The test serves no aircraft (`--ai 0`, "the server gave this client no
    aircraft").
  - Run again on this branch it passed, in 45.4 s. It was not tried on
    main.

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
- Searched for her lift, not usable: D. G. Parsons, A. G. Eckstein and J. J.
  Azevedo (Lockheed Martin), "F-35 Aerodynamic Performance Verification",
  AIAA 2018-3679 (SHA-256 of the copy read
  d861779794dbe84707f867a06e2449c5e49b763735eb944c454ec1024bc1e137): its
  flight-test lift curve has no lift scale. Raymer's wing-body lift-slope
  method (via a course page's reproduction of section 12.4.1). WRDC-TR-89-3079
  on forebody chines (chines add lift; no number for hers). The 2012 F-35A
  high-AoA reports (50 degrees, no minimum speed).
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
- **The crosswind test no longer names her** (`approach_named` in
  tests/unit/test_autopilot.cpp is empty). It failed once she held her
  height. Now, in her landing configuration from 152 to 163 kt, all 4
  speeds are judged, with height within 193 ft, and 29 of 29 plan speeds
  are flown.
- **src/sim/lander.cpp is unchanged**; src/sim/circuit.cpp (`base_kts`) is the one simulation change, above.

**The selftest hash** does not move: the selftest flies the C172P.

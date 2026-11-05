### An AI take-off raises her gear on a positive rate, 2026-11-05

**What it does not do, first.**
- **Two copilot recordings are red and must be recorded again**:
  `take_off_climb_to_3000_ft_and_orbit_the_cbd_is_planned_for_the_747-400_by_anthropic_as_recorded_and_flown`
  and its F-22A twin. The prompt carries each aircraft's climb speed, which
  moved (below), so the recorded exchange no longer matches it. Recording
  spends the owner's Anthropic key, which is not done without his say-so;
  the item stays open until it is.
- **The whole suite was not run locally**: the linux-release run stopped
  at its 30-minute cap at 1,227 of 1,575 tests. The take-off subset (548
  tests) was run in full, before and after.
- **The fighters' and the B-2A's gear limits are not published.** Their
  flight manuals (T.O. 1F-15A-1 and the rest) are not public in the part
  that has them, and no search found a figure. The test holds them to 250 kt,
  the lowest of the airliners' and the Learjet's, named as a stand-in.
- **Several of the published VLEs are crews' copies, not the manufacturer's
  page**: the 737-300's, 747-400's, 787-8's, A320's, A380's and Learjet
  35A's come from operators' limitations sheets and crews' study sets that
  agree with one another. Only the Mosquito's is read from the manual
  itself (A.P. 2019E, the copy hashed in `docs/ASSETS.md`: "Maximum speeds
  in knots ... Undercarriage down ... 155").
- **The gear limit is checked by the test, not flown to.** Nothing in the
  autopilot holds her under VLE with the gear out; she is simply never
  there. The Mosquito comes closest: her model's gear takes 15 s to come up,
  and she reaches 154 kt against 155 in it.
- **The fighters are flown by the airliners' rule.** Their manuals' take-off
  procedures are not public either.

**What it does.** The take-off autopilot (`Departure::raise_the_gear`,
`src/sim/departure.cpp`) puts the gear lever up once she is off the ground,
5 ft above where she stood, and has climbed at more than 100 ft/min for a
second together: a positive rate, confirmed, not a bounce. The rule is the
same for every type whose model's gear retracts (`Aircraft::gear_retracts`):
- Boeing's 737 Flight Crew Training Manual, as crews quote it: "After a
  positive rate of climb is indicated on the altimeter, retract the landing
  gear."
- Airbus: the PM calls "positive climb" and the PF orders the gear up.
- The FAA's Airplane Flying Handbook warns against raising it before a
  positive rate is established.
- The Mosquito's Pilot's Notes (A.P. 2019E, take-off): "When comfortably
  airborne, brake the wheels and raise the undercarriage."

100 ft/min is what a circuit joined from a take-off already raised it on
(`circuit.cpp`). A take-off taken over in the air keeps the gear lever where
it found it, as it keeps the flap lever. Fixed gear (the 172P, 182, Cub and
Cherokee) has no lever, and nothing is moved.

**Why.** The take-off set the gear down on every step, and the autopilot
that follows it holds the controls it is handed; the navigator never moves
the gear. So every planned AI flight climbed and cruised with her wheels
down, the fighters at 337 to 384 kt, and the autopilot's best-climb floor,
which is only for a clean aeroplane, stayed off for every retractable.

**Verification.**
- `every_landplane_whose_gear_retracts_raises_it_on_a_positive_rate_after_any_ai_take_off_and_is_never_past_her_gear_extended_speed_with_it_out`
  (tests/unit/test_lesson.cpp): each of the 15 landplanes, asserted, taken
  off twice at her take-off figures' weight - to a plan at her fastest plan
  speed, and to no plan - and flown three minutes past the hand-over; the
  Short S.23 is named and left out. The 11 whose gear retracts have the
  lever up 1.0 s (Mosquito) to 4.7 s (747-400) after lift-off, at 14 to 28
  ft climbing 337 to 1,377 ft/min, bounded at 6 s; the gear stays up; and
  none is past her VLE with any gear out (fastest: 747-400 180 kt of 320,
  F-15C 231, F-22A 224 and F-35B 239 of 250, Mosquito 154 of 155). The four
  with fixed gear have no lever moved. Every retractable has a VLE named,
  asserted. Red on the old code: the 737-300's lever never went up, and she
  cruised at 301 kt gear down.
- `a_take_off_taken_over_in_the_air_never_puts_down_a_gear_it_found_up`
  (tests/unit/test_departure.cpp): each of the 11 retractables, gear up at
  500 ft, given to a take-off for 20 s, never has the lever put down; 11 of
  16 aircraft retract, asserted. Red on the old code: the 737-300's lever
  went to 1.

**What moved.** Measured on linux-release, against the base branch.
- **Take-off figures re-measured** (`the_measured_take_off_speeds_in_the_747_and_f22_figures_are_what_the_measurement_makes`):
  the slowest climb away rose with the gear up, so the 747-400's written
  climb speed is 175 kt (170.8 measured; was 169.2, written 170) and the
  F-22A's 230 kt (226.6; was 222.7, written 225). Rotation speeds are
  unchanged.
- **The no-plan hold**: the F-22A no longer goes 386 ft past her 3,000 ft
  level-off but none, so her own 450 ft bound is taken out and she is held
  to the 100 ft the rest are. Every landplane is handed over a little
  sooner, lower and faster, climbing harder: the 737-300 at 2,730 ft
  climbing 2,702 ft/min (was 2,766 and 2,345), the F-22A at 403 kt (384).
- **The circuit lessons** (`the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief`,
  `an_instructor_demonstrates_a_circuit_and_hands_it_over`): the Mosquito,
  climbing faster and so handed over further short of the take-off's 650 ft,
  was left 2.7 minutes and 7 nm upwind while the autopilot closed the last
  4 ft, and ran out of the lesson's fifteen minutes on her rollout. The
  test's take-off is now asked for 750 ft, so she is joined to the circuit at
  650 climbing, as a take-off joined to one is.
- **The airliners' low hold at take-off flap and gear**
  (`every_airliner_at_her_take_off_flap_and_gear_holds_a_low_height_settled_inside_a_minute`)
  builds its situation, gear down at the 35 ft screen, by holding the lever
  down itself: the take-off now has it coming up by then. Its figures are
  unchanged to a tenth.
- **Figures that moved and still pass**: every retractable's circuit and
  straight-in separation runs (climb-out speeds a few knots faster, the
  Learjet's 241-246 to 229-245 kt); the hand-over-at-lift-off tests; the
  Sydney 16R plans to the orbit: 50 of the 548 take-off tests print other
  figures, all but those named here unchanged in pass or fail.
- **The F-22A's glides**: her climb speed is the fastest a glide may ask,
  so she is now asked 225 and 230 kt, and her band is 225 to 230
  (`the_f22a_glides_round_its_tightest_orbit_from_225_to_230_kt_without_stalling`,
  renamed from `..._at_225_kt_...`); she glides round at both.
- **The selftest hash does not move**: the selftest flies a pilot's input
  log, not an AI take-off.

### An AI take-off raises her gear on a positive rate, 2026-11-05

**What it does not do, first.**
- **The whole suite was run locally in three parts** (linux-release): the
  first stopped at its 30-minute cap at 1,227 of 1,575 tests, and the 363
  it never reached were run after it, all green but for the few that
  skip for want of a display or a platform; the 8 that ask a model now are
  left out by label. Of the first part's 1,227, the three red ones - the two
  recordings and the F-22A's glide count - are fixed (below) and green.
- **The F-22A's, F-35B's and B-2A's gear limits are not published.** Their
  flight manuals are not public, and no search found a figure. Their
  catalogue files give 250 kt, the F-15C's and the lowest of the jets'
  published limits, named there as a stand-in.
- **Several of the published VLEs are crews' copies, not the manufacturer's
  page**: the 737-300's, 747-400's, 787-8's, A320's, A380's and Learjet
  35A's come from operators' limitations sheets and crews' study sets that
  agree with one another. Two are read from the manual itself: the
  Mosquito's (A.P. 2019E, the copy hashed in `docs/ASSETS.md`: "Maximum
  speeds in knots ... Undercarriage down ... 155"), and the F-15C's (T.O.
  1F-15A-1, the archive.org scan `docs/ASSETS.md` names, section V,
  Operating Limitations, Systems Limitations, Airspeeds: landing gear
  extension, retraction or flight with gear extended, 250 knots at +2.0 g;
  read in the scan's text, `F15 Manual_djvu.txt`, SHA-256
  `ad7d9478d00c24fb31147f0462a9b6d43129efa3972dc895f3cf6bc014d2da22`).
  Each aircraft's is her catalogue file's `gear-extended`, with its source
  beside it.
- **The 5 ft, the second and the 15 kt are chosen, not sourced.** The
  manuals say "positive rate", "confirmed" and "established", and give no
  time or height; nor does any say how far under VLE to climb with the gear
  in transit.
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
(`circuit.cpp`). **While any of her gear is out the climb away is held
15 kt under her gear-extended speed** (`gear_transit_margin_kts`), where her
catalogue file gives one: the Mosquito's gear takes 15 s to come up, and
climbing away at 148 kt she reached 154 in it against her 155. Held to 140,
she reaches 147. No other aircraft climbs away within 15 kt of hers, so
none else is touched. A take-off taken over in the air keeps the gear lever where
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
  lever up 1.0 s after lift-off, taken as 5 ft over where she stood, each
  climbing 337 to 1,377 ft/min then, bounded at 1.5 s; the gear stays up;
  and none comes within 5 kt of her VLE with any gear out (fastest:
  747-400 180 kt of 320, F-15C 231, F-22A 224 and F-35B 239 of 250,
  Mosquito 147 of 155). The same in an x86-64-v2 build (`-march=x86-64-v2`,
  no FMA, as CI's runners): Mosquito 147 kt, every lever up at 1.0 s. The four
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
  test's take-off is now asked for 750 ft, for every aircraft's circuit, so
  each is joined to the circuit at 650 climbing, as a take-off joined to one
  is. The creep itself, a no-plan take-off's last few feet, is a Phase 10a
  tail.
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
- **The 747-400's and F-22A's CBD-orbit plans were recorded again**
  (`tests/data/copilot/cbd-orbit-747-400-anthropic.jsonl`, `...-f22-...`):
  the prompt carries each aircraft's climb speed, which moved, so the old
  exchanges no longer played back. Asked of Anthropic once each, live, by
  the owner's standing word of 2026-10-31 that recordings are made again
  when their prompts change: `GLIDESLOPE_LIVE_MODEL=1 ctest --preset
  linux-release -R "orbit_the_cbd_is_planned_for_the_(747-400|f22)_by_anthropic_now_and_flown"`,
  whose recordings, written to `build/linux-release/tests/plans_flown/`,
  were copied into `tests/data/copilot/`; no key is in them. Both
  `..._as_recorded_and_flown` tests play back and fly green.
- **The selftest hash does not move**: the selftest flies a pilot's input
  log, not an AI take-off.

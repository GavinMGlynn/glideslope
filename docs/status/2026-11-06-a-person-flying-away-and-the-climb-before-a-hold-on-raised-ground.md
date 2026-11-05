### A person flying away from the circuit, and the climb before a hold on raised ground, 2026-11-06

**What it does not do, first.**
- **A person who lines up and rolls with an AI aircraft 2 nm out is kept
  apart from her only in the four pairs flown on every run.** The AI
  breaks off her approach for a person lined up on the runway
  (`sim::ahead_of_an_approach_by_hand`) and flies her circuit 700 ft over
  the person's climb (`sim::circuit_over_departure_ft`). The slowest and
  the fastest each with each stayed apart: least 3,401 m within 500 ft and
  603 ft within 1.5 nm (623 ft with the circuit raised over her; the raise
  was not seen needed by these four). Swept over every pair it was not: a
  person's C182 under eleven landplanes lost the minimum (the least 24 m;
  under the 747-400 797 m and 440 ft, her climb drifting 25 degrees left
  as the jet, no longer giving way past where she counts as departing,
  turned crosswind across it), and a PA-28 behind a person's F-15C, F-22A
  or F-35B did not land within the hour. So the sweep leaves this case
  out, named, and the item stays open on it.
- **A person who lines up under an AI aircraft already at 200 ft is the
  conflict she makes**: she lifts off within 1.5 nm of the one going
  around over her, and every pair loses the minimum, without any
  sidestep: Cub under Cub 670 m and 0 ft (lift-off 862 m, 196 ft); F-15C
  under Cub 1,074 m and 0 ft (1,074 m, 280 ft); Cub under F-15C 856 m and
  195 ft (861 m, 196 ft); F-15C under F-15C 665 m and 0 ft (369 m, 855 ft).
  The sidestep, tried, made it worse (90 m and 0 ft for two F-15Cs) and is
  taken out. Under the owner's ruling of 2026-10-09 - an item that cannot
  be met closes on today's measured figures with the shortfall written
  down - this case is closed on these figures; the item stays open on the
  2 nm case swept above.
- **A hold out of her reach is held at her best height, where she is**
  (`HoldOutside::out_of_reach`): where her climb gains less than 100 ft in
  two minutes she gives the hold up, holds the best height she reached and
  circles there, over ground her circuit was cleared for. Nothing diverts
  her to another runway or airport; a plan item. A Cub under ground
  20,000 ft high climbed to 14,100 ft, gave it up at 3,474 s, and held it
  within 2 ft, 0.26 nm from where she left.
- **A person's place is dropped only by distance**, 10 nm from the
  threshold. A person who flies away and comes straight back in under that
  keeps it; one who comes back after it is not in the sequence, and is
  given way to only while she is straight in or departing.
- **Every landplane with every other** flies the new situations in the
  fifteen gust_sweep tests (`every_landplane_with_a_person`): flying away
  on all eight bearings and the climb on raised ground, every pair
  (linux-release, -j4, 444 to 1,493 s a test). Every flight of those two
  kept apart and landed. Lining up 2 nm ahead is left out, named (above).
- **A racetrack reaches past the ground its circuit sampled**: an F-15C
  following another, her circuit raised 700 ft, flew her racetrack into a
  plateau 10 nm out that her circuit had not seen (it samples 8 nm before
  the threshold). A plan item; the test's plateau is 12 nm out.

**What it does.**
- **A person who flies away from the circuit loses her place**
  (`sim::left_the_circuit_by_hand`): further than
  `sim::left_the_circuit_nm`, 10 nm, from the threshold, her place is
  dropped, and an AI aircraft holding outside behind her comes back by its
  legs to final and lands. 32 flights - the J-3 Cub and the F-15C each
  with each, the person flying away level on each of eight bearings, every
  45 degrees off the runway's heading, once the one behind is round her
  hold's fix: every place dropped at 10 nm, every one behind on the runway
  and clear of it, the longest 800 s after the drop (stated:
  900 s), the least apart 3,821 m within 500 ft and 686 ft within 1.5 nm.
- **The climb before a hold on raised ground is flown before the hold**,
  and measured against a person in the circuit. Leaving her circuit from a
  racetrack's outbound leg, she was within 2 nm of her hold's fix with it
  behind her, counted a lap at once and skipped the climb where she was:
  she flew the hold's outbound leg on towards the high ground below its
  height. Now she is not round the fix until she has climbed to within
  200 ft of the hold's height, in right turns where she is. Over a plateau
  2,500 ft high from 12 nm out, a person staying in the circuit ahead, all
  four pairs climbed 1,243 to 1,772 ft before setting off, and while they
  did were never within 500 ft of the person nor within 1.5 nm of her.
  The whole runs: least 686 ft within 1.5 nm, 3,821 m within 500 ft.

**Researched first** (owner's standing rule). Searched: a go-around with
traffic on the runway, sidestep and offset (AIM 4-3-2, the Airplane Flying
Handbook, the ACS's go-around task, pilots' forums); JO 7110.65 on
missed approaches and departures on one runway (3-9-6, 3-10); ICAO Doc
4444 on a departure beneath a missed approach; when a tower drops an
aircraft from the pattern and the Class D surface area's radius; terrain
clearance in a climbing turn and climb-in-hold (AIM 5-3-8, the AIP's ENR
1.5). FAA's own pages refused several fetches (403, 429).
- CAA of New Zealand, Vector 2017, "Go-arounds": "If the go-around was
  initiated due to traffic on the runway ... it is advisable to perform the
  procedure to the right of centreline. This ensures that any aircraft on
  the runway, or climbing out, remain in sight". The sidestep was built on
  this, measured, and taken out (above).
- An FAA notice (N JO 7110.531): the order does not prescribe separation
  for a go-around against a departure; it is a situation not covered.
  JO 7110.65 has a missed approach by a heavy flown under 3-9-6, so that
  it "does not overtake or cross in front of a departing aircraft" - the
  geometry a person rolling under her makes unavoidable.
- AIM 4-3-2: outside the Class B, C and D surface areas a pilot need not
  ask to leave the tower's frequency. A Class D surface area is usually
  4 nm round (5 statute miles). Used: the drop is past that and past every
  circuit (6.5 nm measured) and the legs to final (6 nm), with twice the
  1.5 nm minimum: 10 nm, chosen. At 8 nm an F-15C flying away down the
  approach was 2,673 m and 474 ft from the F-15C coming back.
- Climb-in-hold (AIP ENR 1.5): a published hold may be climbed in, its
  protected area enlarged for it. Departure procedures assume no turn below
  400 ft and 200 ft a mile. Used: the climb is flown in turns where she is,
  over ground her circuit was cleared for, before the hold - as the hold's
  rule already said; the fix makes it so.

**What changed.**
- `sim::left_the_circuit_by_hand`, `sim::left_the_circuit_nm`; the
  controller drops a person's place past it (`src/sim/controller.cpp`).
  The server needs nothing new: it asks `behind_a_person` of every place.
- `sim::HoldOutside::modes`: no lap before the climb.

**Verification** (tests/unit/test_lesson.cpp, the two-going-around harness,
the person flown by a stand-in).
- `an_ai_aircraft_behind_a_person_who_flies_away_from_the_circuit_square_to_the_runway_comes_back_and_lands_kept_apart`
  (every run) and `..._between_the_squares_...` (gust_sweep): the bearings
  0, 90, 180, 270 and 45, 135, 225, 315; each pair on each, the count
  asserted. `the_two_tests_of_a_person_flying_away_fly_every_bearing_once`
  holds that the halves are the eight.
- `on_raised_ground_an_ai_aircraft_climbing_before_her_hold_is_kept_apart_from_a_person_in_the_circuit`:
  four pairs, each asserted to have climbed 500 ft or more before her fix.
- linux-debug, two or three at once: each half of the flying-away flights
  1,060 and 1,042 s; the raised ground 247 s. Timeouts 5,400 and 3,600 s.

**Seen to fail.**
- The drop at 8 nm: an F-15C flying away down the approach ahead of an
  F-15C, 2,673 m and 474 ft - the reason for 10 nm.
- The drop taken out: an F-15C flying away straight out ahead of an F-15C: her place was never dropped, and the one behind never came back ("not dropped as she passed 10 nm", "not back on the runway ... within 900 s").
- The lap before the climb (the old rule): two of four pairs never
  climbed before their fix, "no climb before the hold to measure".
- A descent of 2,400 ft put in place of the climb: all four failed.
- The plateau at 8.5 nm: every circuit raised 2,500 ft, nothing to measure;
  at 10 nm, the F-15C's racetrack flew into it.

**The review of #351's round.**
- The break-off for a person lined up: without it, an F-15C 2 nm out
  went around only at 400 ft and came 1,760 m and 347 ft from a Cub just
  lifted off; breaking off only once she rolled faster than 30 kt, 2,375 m
  and 480 ft - both seen red.
- The coverage check of the flying-away tests reads the registry: with
  one of the two tests not registered it failed (seen).
- The hold's out-of-reach end taken out: the Cub never gave the hold up
  (seen).
- Per run, the flying-away flights are the four pairs down the approach
  (287 s on linux-debug beside a -j4 sweep); the other seven bearings are
  gust_sweep (211 s on linux-release). The lining up 313 s, the conflict
  325 s, the out-of-reach 79 s, all linux-debug beside the sweep.
- The 200 ft at which she sets off is `HoldOutside::sets_off_within_ft`;
  one `least_figure` prints every least.

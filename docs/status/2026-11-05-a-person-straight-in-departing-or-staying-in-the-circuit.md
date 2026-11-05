### A person straight in, departing or staying in the circuit, 2026-11-05

**What it does not do, first.**
- **A person who takes off under an AI aircraft going around is not kept
  apart from her.** Nothing holds a person, and no turn takes the AI clear
  in time: an F-15C rolling as another F-15C went around over her climbed
  through her height 684 m from her (169 ft) within 15 s of lift-off, and
  one rolling as a J-3 Cub turned downwind climbed through the Cub's
  height abeam, 1,700 m off. Such a take-off breaks 14 CFR 91.113(g) - the
  landing aircraft has the right of way over one on the surface - and a
  tower would not release it (JO 7110.65 3-9-6). The tests' person takes
  off when a tower would release her: the one going round on her downwind
  leg, the minimum short of the threshold. So the item about a person
  departing stays open, with this named.
- **The AI behind a person who stays in the circuit leaves and does not
  come back.** She turns 45 degrees away from the circuit at her raised
  height and flies on, on the plain autopilot, her landing given up. Nothing
  brings her back to land once the person has gone, nor sends her to
  another runway or airport. Nothing is said to the person either: no
  server-to-client message carries a tower's words today.
- **A person's aircraft is judged only by how she flies.** Straight in is
  heading within 30 degrees of the runway, within 1.5 nm of its extended
  centreline, before the threshold and within 5 nm, and no higher than a
  three-degree path and 700 ft; departing is rolling down the runway faster
  than 30 kt, or in the air past the threshold, within 1.5 nm past its far
  end. A person flying her own circuit from a take-off by hand has no place
  in the sequence: she is known only while she is straight in or departing.
  A place is a person's only when she was taken over in the circuit.
- **A person's place is kept until her wheels touch**, as before, even if
  she flies away from the circuit.
- **No server run yet has a person in the sequence.** The server asks every
  aircraft by the same functions the tests call (`sim::straight_in_by_hand`,
  `departing_by_hand`, `behind_a_person`; the controller's own
  `clear_of_departures_by_hand`), but the verification is the unit tests
  below.

**What it does.**
- **A person straight in or departing is given way to**, as an AI's is: an
  AI circuit that has not turned base follows her - offset right on her
  upwind leg, and down her downwind leg until the person is down or climbed
  out. Turned base, the AI is committed.
- **An AI departure behind a person in the circuit is released on her
  downwind leg**, as one behind an AI is: her place is marked clear of
  departures while she is left of the centreline, heading within 60 degrees
  of the runway's reciprocal and the 1.5 nm minimum short of the threshold
  - judged afresh every step, so turned base she holds it again. A person
  who stays in the circuit holds it at most one of her circuits, not until
  she lands.
- **An AI aircraft behind a person who stays in the circuit leaves it**
  (`Controller::leaves_with`): on her second racetrack's outbound leg -
  outside the circuit, 700 ft over the highest the person flies hers - she
  turns 45 degrees away from the runway, gives up her place and flies on
  at that height.

**Researched first** (owner's standing rule). Searched: JO 7110.65 3-8-1,
3-8-2, 3-9, 3-10 and 2-1-21; holding a departure for pattern traffic and
for how long; a tower re-sequencing arrivals around pattern work, "extend
downwind" and "depart the pattern"; giving an arrival another runway
("change to runway"); AIM 4-3-2 and AC 90-66C on leaving the pattern; 14
CFR 91.113(g). FAA's own pages refused the fetch (403); the order was read
from pointsixtyfive.com's copies and FAA notices.
- JO 7110.65 3-8-1: "Establish the sequence of arriving and departing
  aircraft by requiring them to adjust flight or ground operation" -
  phraseology EXTEND DOWNWIND, NUMBER ... FOLLOW, CIRCLE THE AIRPORT, MAKE
  LEFT/RIGHT THREE-SIXTY, GO AROUND. Every one of them is an instruction; a
  person cannot be given one here, so the AI is the one that adjusts.
- JO 7110.65 3-8-2: a touch-and-go or low approach is an arriving aircraft
  until it touches down or crosses the threshold, and departing after. Used:
  a person over the runway past the threshold, on its heading, is departing.
- JO 7110.65 3-10-6 and 3-10-5: "number two following ..."; "change to
  runway" for a changed landing runway. No runway but one in the tests, so
  none is offered.
- JO 7110.65 3-9-6: a departure does not roll until the one before has
  crossed the runway end or turned to avert any conflict.
- 2-1-21 (traffic advisories): issue them when proximity may become less
  than the separation. Only secondary quotes were found; there is no
  message to carry one.
- How long a tower holds a departure for pattern traffic: no published
  figure. An AOPA account of weekend touch-and-go traffic: about three
  minutes at the hold line before a gap. Gate-hold procedures aim at 15
  minutes or less (FAA facility order), not the same thing.
- AIM 4-3-2 and AC 90-66C: depart the pattern straight out or with a
  45-degree turn, away from it, once at pattern altitude; a go-around for
  traffic on the runway may manoeuvre parallel to it.
- 14 CFR 91.113(g): an aircraft on final or landing has the right of way
  over others in flight or on the surface.
- The Pilot/Controller Glossary's final approach (VFR): the landing
  direction along the extended centreline, from base or straight in.

**Chosen.** Sourced and chosen apart:
- Straight in and departing judged by how she flies: the sources'
  definitions (final approach; 3-8-2); the 5 nm, 30 degrees, 30 kt and the
  700 ft over the path are chosen.
- The departure released on a person's downwind leg: the AI's rule
  (sourced as 3-9-6's "turned"), applied to how a pilot flies a downwind.
- Leaving the circuit, not another runway or asking the person to land: a
  person cannot be asked here, and there is one runway. The 45-degree turn
  away is sourced (AIM 4-3-2, AC 90-66C); after one whole racetrack is
  chosen - no figure for how long to hold was found.

**What changed** (`sim/circuit.hpp`, `sim/controller.hpp`,
`src/frontend/server/main.cpp`):
- `sim::straight_in_by_hand`, `departing_by_hand`,
  `clear_of_departures_by_hand`, `behind_a_person`; `CirclePlace::by_hand`.
- `Controller::leaves_with`, `left_the_circuit_s`,
  `racetracks_behind_a_person`; a person's place marked clear of
  departures afresh every step.
- The server's `straight_in_to` judges a person's aircraft (no controller,
  or her pilot's) by how she flies; `behind_a_person` is asked of every
  controller.
- **Fixed on the way**: a person's aircraft stopped beside the runway, lined
  up with it, counted as departing, and an A380 straight in, landed and
  taxied clear, held every AI circuit in racetracks for an hour - two of
  them flew into the ground. Departing off the runway is now in the air
  only.

**Verification** (tests/unit/test_lesson.cpp). Each situation built
explicitly, the person's aircraft flown by a stand-in - an AI controller of
her own whose intentions nothing else is told.
- `a_circuit_gives_way_to_a_person_straight_in_and_is_kept_apart_from_her_departing_the_slowest_and_the_fastest_each_with_each`:
  the three situations of the AI's test (2026-11-05, a go-around's circuit
  gives way to a straight-in), the straight-in or the departure a person's.
- `an_ai_aircraft_behind_a_person_who_stays_in_the_circuit_leaves_it_kept_apart_the_slowest_and_the_fastest_each_with_each`:
  two going around together, the one ahead taken by her pilot on her
  downwind leg and flown round again from every approach below 300 ft for
  the whole run; it ends 300 s after the one behind has left.
- `an_ai_departure_behind_a_person_who_stays_in_the_circuit_departs_kept_apart_the_slowest_and_the_fastest_each_with_each`:
  a person who stays as above, the departure lined up on the threshold and
  given its take-off as she turns final - the longest hold there is.
- These three run the J-3 Cub and the F-15C each with each (20 flights) on
  every run. `<id>_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart`:
  fifteen tests, every landplane with every other in each of the five
  situations - 1,125 flights - labelled gust_sweep.
  `every_landplane_has_a_test_with_every_person_straight_in_departing_or_staying_in_the_circuit`
  fails if a landplane is added without its test; each sweep asserts every
  pair flown. The flying boat is named and left out.
- In every flight, every step both are in the air, they are 500 ft or 1.5
  nm apart; nothing is wrecked.
- `the_one_ahead_of_two_going_around_taken_by_her_pilot_on_downwind_is_still_followed`:
  the person now stays five minutes, not fifteen, and lands from her next
  approach (471 s), short of the 801 s a C172P behind her holds; the one
  behind follows her throughout and lands after her.

**The least, across all 1,125** (all sixteen tests green, linux-release,
4 to 8 minutes a test):
- A person straight in or departing, 675 flights: 3,321 m horizontally
  within 500 ft and 541 ft vertically within 1.5 nm (both a C182 in the
  circuit with a person's J-3 Cub straight in as she goes around). 222 of
  the person's straight-ins went around early behind the AI and followed
  her round.
- Behind a person who stays, 225 flights: 3,090 m and 618 ft (a person's
  J-3 Cub ahead of a 747-400). Every one behind left the circuit, the
  latest 1,190 s after the person was taken (a PA-28 behind a person's
  F-35B); the test's bound is 1,500 s.
- A departure behind a person who stays, 225 flights: 5,289 m within 500 ft
  (an F-22A departing behind a person's J-3 Cub), never within 1.5 nm. The
  longest held was 455 s (a 737-300 behind a person's PA-28); the test's
  bound is 600 s. Every one climbed out.

**Seen to fail**, each rule taken out and the failure watched, in the three
small tests:
- `straight_in_by_hand` never true: the F-15C in the circuit did not give
  way to a person's J-3 Cub straight in.
- `departing_by_hand` never true: all four circuits did not give way to the
  person's climb out.
- Leaving taken out: all four behind a person who stays never left the
  circuit.
- `clear_of_departures_by_hand` never true: all four departures were held
  for the whole run and never climbed out.
- Before the person's stand-in waited for a release, as above: 6 of 12
  failed - four departures rolled under the go-around and lost separation
  (667 m and 0 ft the closest), and a person straight in who went around
  behind the AI, following nobody, came within 2,095 m and 322 ft of her
  (her stand-in now follows whom she sees ahead, as a pilot would).

Unchanged and green: every test matching person, go-around, going around,
circuit, straight-in, separation, vacate, taxi, kept apart, controller,
runway, selftest and the CI shard checks, the gust_sweep label aside - 132
of 132 on linux-release (one skipped, as it needs a key). The server and
the tests build on Windows (windows-debug).

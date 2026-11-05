### A go-around's circuit gives way to a straight-in and holds a departure, 2026-11-05

**What it does not do, first.**
- **A person's aircraft is not in it.** A person flying straight in, or
  taking off by hand, is not known to the AI as a straight-in or a
  departure: the circuit does not give way to them, and nothing holds
  them. The separation monitor and the runway's rule still keep those
  apart. A person taken over in the circuit keeps her place (as before),
  but her place is never marked clear of departures, so a departure from
  her runway is held until she lands.
- **No server run yet has a straight-in or a departure meet a circuit.**
  The server asks every aircraft it flies, by the same functions the tests
  call (`sim::close_ahead_on_approach`, `sim::departure_held`,
  `Controller::straight_in`, `Controller::departing`), but the verification
  is the unit tests below.
- **The straight-in's place is not chosen by a tower.** One on the approach
  is always ahead of a circuit that has not turned base, however far out
  she is: the one in the circuit may fly racetracks for her a long while.
- **A departure is not held for an arrival straight in**: the runway's
  rule still sends the arrival round if the departure is on the runway.

**What it does.**
- **A straight-in lands first** (14 CFR 91.113(g): an aircraft on final
  approach has the right of way over other aircraft in flight). One in the
  circuit who has not yet turned base, with an AI aircraft on a straight-in
  approach to her runway, follows it as she follows a circuit ahead: her
  upwind leg offset 1.5 nm right, and her downwind leg extended (at most
  2 nm, then the racetrack) until it has landed. Her downwind leg is flown
  wide, 3,278 m left of the centreline the straight-in flies down.
- **Turned base, she is committed**, and a straight-in behind her is the one
  to give way.
- **Traffic close ahead on an approach sends her round early** (the
  Airplane Flying Handbook, FAA-H-8083-3C, chapter 8: with a conflict on
  final "an early go-around may be in order"). Close is: ahead of her along
  the runway, within 1.5 nm of its centreline, within 700 ft of her height,
  and nearer than 1.5 nm plus the greater of half a mile and a minute's
  closing. Gone around, she is in the circuit's sequence behind it.
- **A departure is held for a go-around** until the one going round is on
  her downwind leg 1.5 nm or more short of the threshold, and released then.
  Held, she stands on the runway with the brakes on and the power off.
- **A departure climbing out is given way to**, as a straight-in is.

**Researched first** (owner's standing rule). Searched: JO 7110.65 3-9-6
same-runway departure separation; 3-10-3 same-runway arrivals; 3-8-1 and
3-8-2; AIM 4-3 straight-in against pattern traffic; how a tower holds a
departure for an aircraft going around; how a tower sequences a straight-in
behind a circuit. FAA's own pages refused the fetch (403); the order's text
was read from pointsixtyfive.com's copies and FAA notices.
- JO 7110.65 3-9-6, Same Runway Separation: a departure does not begin its
  take-off roll until a preceding departing aircraft "has departed and
  crossed the runway end, or turned to avert any conflict" (or is airborne
  and 3,000 to 6,000 ft down the runway, by category, where landmarks show
  it), and a preceding landing aircraft is clear of the runway.
- JO 7110.65 3-8-2: an aircraft on a low approach is an arriving aircraft
  until she crosses the landing threshold, and thereafter a departing one.
  An FAA notice of 2012 (N JO 7210.819) says the order does not cover a
  go-around's change from arrival to departure explicitly, and that it is
  handled "as a situation not specifically covered".
- JO 7110.65 3-10-3: an arrival does not cross the threshold until a
  preceding departure has crossed the runway end (or is airborne and 3,000
  to 6,000 ft down it), or a preceding arrival has landed and is clear.
- JO 7110.65 3-8-1: "Establish the sequence of arriving and departing
  aircraft by requiring them to adjust flight or ground operation, as
  necessary, to achieve proper spacing."
- 14 CFR 91.113(g): an aircraft on final approach, or landing, has the
  right of way over other aircraft in flight or on the surface; of two
  approaching to land, the lower, but it may not cut in front of one on
  final. AC 90-66 (and AIM 4-3, as pilots quote it): a straight-in has no
  priority over traffic in the pattern and must comply with 91.113(g). The
  rule chosen: a straight-in on the approach is on final, and ahead of a
  circuit not yet on base.
- The Airplane Flying Handbook, chapter 8: with a conflict on final, "an
  early go-around may be in order"; the offset to the right when the
  go-around was caused by another aeroplane (as #336 cites).
- Pilots' and controllers' practice (Pilots of America, "ATC straight-in and
  wide downwind"; VATSIM and Infinite Flight tower guides): "extend
  downwind", "number two, follow", a wide downwind for a straight-in. No
  published distance for a wide downwind was found; a VATSIM manual says a
  departure behind a go-around is given a diverging course.

**Chosen.** Sourced and chosen apart:
- **The straight-in first** (sourced: 91.113(g)). Not past base (chosen).
- **The wide downwind, 3,278 m**: the 1.5 nm minimum and 500 m in hand for
  the autopilot's turns. Chosen; no figure was found.
- **The early go-around's closeness**: 700 ft is the minimum and the 200 the
  monitor keeps in hand; the distance is the minimum and the greater of
  half a mile and a minute's closing, because her go-around climbs at
  500 ft/min or more, so she has 500 ft before she could close to 1.5 nm.
  Derived, not sourced. Closing is judged from each one's airspeed along
  the runway, calm air's ground speed.
- **When a departure is released**: 3-9-6's "turned to avert any conflict"
  is sourced. Which turn is chosen: on her downwind leg the minimum short of
  the threshold, so that a departure climbing out ahead of the threshold
  is never nearer her than 1.5 nm along the runway. Released abeam the far
  end instead (tried first), an F-15C departing climbed through the height
  of a J-3 Cub starting her downwind, 1,700 m from her.

**What changed** (`sim/circuit.hpp`, `sim/controller.hpp`,
`sim/departure.hpp`, `src/frontend/server/main.cpp`):
- `GoAroundCircuit::follow(ahead, straight_in)` and `wide()`;
  `wide_downwind_m`.
- `sim::close_ahead_on_approach`, `sim::close_m`; `Controller::spaced_with`
  goes around from an approach (the lander's or the learnt one's) with
  traffic close ahead, "traffic close ahead on final".
- `CirclePlace::clear_of_departures`, `sim::departure_held`;
  `Controller::holds_with`, `rolling`, `departing`.
- `Controller::straight_in`, `gives_way_with`, `why_gone_around`.
- **Fixed on the way**: an approach gone around and done with it in the
  same step - from high up - was put in no circuit's sequence, so a
  straight-in sent round from 1,600 ft followed nobody and flew her circuit
  at the one ahead's height.
- The server tells every controller it makes of every other aircraft it
  flies, wrecks aside.

**Verification.**
- `<id>_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order`
  (`a_c172p_...`, `an_f15c_...`): fifteen tests, one for each landplane
  taught the approach in the circuit, each with every landplane straight in
  or departing, in each of three situations - 675 flights, all 225 pairs in
  each (the flying boat is named and left out: it has no runway).
  `every_landplane_has_a_test_of_her_circuit_with_every_straight_in_and_departure`
  fails if a landplane is added without its test, and each test asserts
  that every pair was flown in every situation. Labelled gust_sweep: every
  night on every preset, and on no pull request's run. 2 to 13 s a flight
  on linux-release, about 12 minutes a test.
- `a_circuit_and_a_straight_in_or_a_departure_the_slowest_and_the_fastest_each_with_each_are_kept_apart_and_in_order`:
  the J-3 Cub and the F-15C, each with each, in all three situations -
  twelve flights, the widest closings both ways - on every run, pull
  requests' included. 55 s on linux-release; a 1,800 s timeout for Linux
  debug's runners.
- **The situations, built explicitly.** The one in the circuit goes around
  from 200 ft on the glidepath, as in the test of two going around together.
  - *A straight-in as she goes around*: established on the glidepath 4 nm
    from her aim point. **Order**: the straight-in lands first, the one in
    the circuit having given way, wide - unless the straight-in closes on
    her and goes around early, and then she lands second. Of 225 pairs, 181
    went the first way and 44 the second; no straight-in went round for
    any other reason.
  - *A straight-in as she joins final*: made as she is handed from her
    circuit to her approach, 2 nm behind her on the glidepath (636 ft over
    her). **Order**: she is committed, does not give way, and lands first.
    178 of the straight-ins went around behind her - 177 early, one for
    the runway not clear - and landed after.
  - *A departure lined up as she comes down*: standing on the threshold,
    engines running, so that the runway's rule sends her round in her first
    step; given its plan (the take-off, straight out to 3,000 ft, and 20 nm
    on) as she goes around. **Order**: she crosses the runway first; the
    departure is held until she is clear of it, lifts off before she lands,
    and climbs out. She gives way to it while it climbs out: no racetrack in
    101 pairs, one in 88, two in 36.
- In every flight, every step both are in the air, they are 500 ft apart
  vertically or 1.5 nm horizontally; both land (or the departure climbs
  out) and taxi clear; nothing is wrecked; the one in the circuit flies no
  further out than her base point and 2 nm.
- **The least, across all 675**: 3,670 m horizontally within 500 ft (a C182
  in the circuit with a Mosquito straight in as she goes around), 558 ft
  vertically within 1.5 nm (a J-3 Cub in the circuit, a 747-400 straight in
  as she joins final). A departure was never within 1.5 nm, nor nearer than
  5,304 m within 500 ft.

**Seen to fail**, each bug put in and the failure watched:
- **No giving way to a straight-in** (`follow` ignoring it): the small
  test failed, three pairs not giving way. All the C172P's 45 flights:
  five did not give way, and the C172P with a J-3 Cub straight in as she
  went around lost separation, 2,195 m and 461 ft.
- **No early go-around** (`close_ahead_on_approach` never true): the J-3
  Cub in the circuit with the F-15C straight in lost separation in both
  straight-in situations - 729 m and 0 ft as she went around, and 1 m and
  0 ft as she joined final, where the F-15C landed first.
- **No departure held** (`departure_held` never true): all four departures
  in the small test rolled under the go-around and lost separation, the
  closest 667 m and 0 ft.
- **No wide downwind** (`wide_` never set): the C172P's 45 flights failed on
  four pairs not giving way wide. None lost separation: the wide downwind is
  not shown needed by these situations, and is kept for one that joins the
  approach while she is on her downwind leg, which none here builds.
- **Found on the way, each seen as a failure first**:
  - A straight-in sent round from 1,600 ft was in no sequence: the lander's
    go-around was over in the step it began, so she followed nobody and flew
    her circuit level with the one ahead, 0.55 nm apart (the fix above).
  - Traffic stopped beside the runway, 114 m off its centreline, counted as
    close ahead, and a C172P went around from it every time round for an
    hour: now it must be over 100 ft above the runway.
  - The early go-around climbed into traffic above: a PA-28 straight in went
    round under a J-3 Cub crossing on her crosswind leg 525 ft above, and
    climbed to within 2,714 m and 265 ft of her. Now only traffic no higher
    than she is sends her round.
  - Released abeam the runway's far end, an F-15C departure climbed through
    a J-3 Cub's height 1,700 m from her, as above.
  - An A380 given a bare take-off to 3,000 ft and no plan was left on the
    plain autopilot, and flew into the ground at 455 s, 225 kt: the test now
    gives every departure a plan, as a server's has. That a plain autopilot
    after a take-off flies an A380 into the ground is a tail.

Unchanged and green: every test matching go-around, circuit, straight-in,
separation, vacate, taxi, kept apart, controller, runway, selftest and the
CI shard checks, 130 of 130 on linux-release (one skipped, as it needs a
key), the gust_sweep label aside - its fifteen new tests were run above,
all green. The server and the tests build on Windows (windows-debug).

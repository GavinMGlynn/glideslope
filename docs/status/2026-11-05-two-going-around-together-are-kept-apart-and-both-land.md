### Two going around together are kept apart and both land, 2026-11-05

**What it does not do, first.**
- **The sequence covers go-arounds only.** An aircraft flying an approach
  straight in, or a departure, is not in it: the one in the circuit does
  not wait for it, and it does not wait for her. The separation monitor
  (sim/separation.hpp) and the runway's rule (sim/vacate.hpp) still keep
  those apart. That is a tail.
- **A person's aircraft ahead is waited for until she is down.** Taken
  over in the circuit, she keeps her place. A person who never lands holds
  the one behind in her racetrack for as long as she flies.
- **No server run yet has two go around together.** The server asks every
  aircraft it flies, by its own clock, but the verification is the unit
  tests below. They call the same `sim::circuit_ahead_alt_ft` the server
  does.
- **The circuit is still left-hand only.** The order does not change once
  set: number two waits for number one even where she could have landed
  first.
- **The F-15C wrecks after four racetracks.** Behind a C172P that goes
  around a second time, at 34,478 lb, she strikes the runway with her
  airframe on her roll-out. Her landing at that weight is a tail.

**Researched first** (owner's standing rule):
- FAA JO 7110.65BB 3-8-1, Sequence/Spacing Application: "Establish the
  sequence of arriving and departing aircraft by requiring them to adjust
  flight or ground operation, as necessary, to achieve proper spacing."
  Phraseology: EXTEND DOWNWIND, NUMBER (sequence), FOLLOW (traffic), GO
  AROUND, MAKE LEFT/RIGHT THREE-SIXTY.
- JO 7110.65BB 3-10-3, Same Runway Separation: an arrival does not cross
  the threshold until the one before has landed and is clear of the runway
  (or, by day, is 3,000 to 6,000 ft down it by category).
- JO 7110.65 7-9-4: 500 ft or 1.5 nm, the minimum the project already keeps
  (REQUIREMENTS.md 6.5, sim/separation.hpp).
- FAA-H-8083-3C, Airplane Flying Handbook, chapter 8: pattern altitude
  1,000 ft, larger aeroplanes higher. The downwind leg is 1/2 to 1 mile
  out. After a go-around the upwind leg is flown. With a conflict on final,
  "an early go-around may be in order".
- The offset on a go-around: a flight-training go-around checklist, citing
  the handbook's pages 8-11 to 8-13: "If go-around was caused by another
  airplane, offset and pass to the right unless it will conflict with other
  traffic (a non-standard pattern), or tower directs otherwise." Mike
  Goulian's go-around note says the same: offset to keep the other in
  view.
- AIM 4-3-3: propeller aircraft fly the pattern at 1,000 ft; large and
  turbine aircraft "not less than 1,500 feet AGL or 500 feet above the
  established pattern altitude". AIM 4-3-12: a low approach is flown
  straight ahead.
- The Pilot/Controller Glossary's upwind leg is "used when issuing control
  instructions for separation, spacing or sequencing" (quoted on Pilots of
  America). In ATSB AO-2016-107 a tower sends one round to "maintain 1,000
  ft and depart on upwind".
- Pilots' practice (Boldmethod, Flying Magazine, Pilots of America): the
  one behind extends her downwind to space herself on the one she follows.
  GASCo's bulletin argues against extending and for going around. No
  published limit on an extension was found. One pilot's account has a
  tower's "extend downwind" run to three or four miles. Another suggests
  asking for a 360 instead, and a CFI's notebook says never to fly a 360
  in the pattern.
- Other simulators: Just Flight puts MSFS's AI go-arounds down to its
  default ATC leaving too little separation on approach. BeyondATC's AI
  goes around from an occupied runway and flies a full circuit to try
  again. Neither publishes a technique with a distance or a height.

**Chosen.** Which parts are sourced and which are chosen:
- **700 ft over the one ahead.** Derived, not sourced: the server's 500 ft
  minimum plus the 200 the monitor keeps in hand (sim/separation.hpp). It
  is kept over the highest the one ahead has flown her circuit, not only
  its set height. A PA-28 ahead overshoots hers by about 190 ft levelling
  off, which took three pairs to 476 ft.
- **Offset to the right on the upwind leg, 1.5 nm.** The side is sourced
  (the handbook's offset, as cited above); the distance is the separation
  minimum. Every leg of a left-hand circuit ahead lies on or left of the
  centreline, so on her upwind leg she is never nearer it than 1.5 nm. She
  turns crosswind only within 100 ft of her raised height, so she crosses
  the legs ahead 600 ft or more above them. Without the offset, a PA-28
  behind a C172P climbed too slowly and lost separation (368 ft, 1,323 m)
  under the C172P's crosswind leg. Five knots slower on the climb instead
  of twenty over did not help (352 ft); that was tried and taken out.
- **Extend downwind until the one ahead is down.** The technique is
  sourced (3-8-1's "extend downwind"; 3-10-3's landed before the next
  crosses the threshold). Waiting until touchdown is chosen.
- **At most 2 nm of extension past her base point** (chosen, not sourced:
  `GoAroundCircuit::longest_extension_nm`). There she flies a racetrack of
  right turns, out away from the runway, back up a leg parallel to it and
  in again. She is still at her raised height, and she joins the downwind
  leg again where she would have turned base. The turn away begins two
  turn radii short, because the autopilot carries round it about 1.4
  radii (measured).

**The sequence is the order they went around in**, by the caller's clock.
On a server that is the server's clock, which every aircraft shares; an
aircraft's own clock starts when it is made, so a later one's would put
it ahead. Her place is kept through another go-around and through her
pilot taking her. Two in the same step go in the order of how far down
the runway they were, then the server's order. Her place ends when her
wheels touch, whoever flies her, or when she is handed back to the AI.

**What changed** (`sim/circuit.hpp`, `sim/controller.hpp`,
`src/frontend/server/main.cpp`):
- `sim::CirclePlace` records her runway, when she went around, how far
  down it she was and the highest she has flown her circuit.
  `Controller::circling()` holds it.
- `sim::circuit_ahead_alt_ft` gives the highest circuit ahead of her, or
  none if she is number one.
- `GoAroundCircuit::follow` raises her circuit, offsets her upwind leg,
  holds her downwind leg and flies the racetrack (`turn_out`, `outbound`,
  `turn_in` legs).
- `Controller::follows_with(query, clock)` asks every step she flies the
  circuit. The server answers from every aircraft it flies, wrecks aside,
  by `now_s()`.

**Verification.**
- `two_going_around_together_behind_the_<id>_are_kept_apart_and_both_land`:
  fifteen tests, one for each landplane taught the approach, each with
  every landplane behind it. That is all 225 pairs (the flying boat is
  named and left out: it has no runway).
  `every_landplane_has_a_test_of_two_going_around_together_with_her_ahead`
  fails if a landplane is added without its test. Each test asserts that
  every landplane flew behind it.
  - The situation is built explicitly: the one ahead on the glidepath at
    200 ft, the one behind two miles further out on it (636 ft higher),
    both told to go around in the same step, and each asked of the other
    whether the runway is clear.
  - Every step, from the go-around until the first touches down, they are
    500 ft apart vertically or 1.5 nm horizontally. The one behind
    followed, the one ahead touched down first, both landed and taxied
    clear, nothing was wrecked, and the one behind flew no further out
    than her base point plus 2 nm.
  - Across all 225 pairs the least was 618 ft vertically within 1.5 nm,
    and 3,090 m horizontally within 500 ft. The one behind flew no
    racetrack in 52 pairs, one in 132, two in 35 and three in 6.
  - 52 to 76 s each on linux-release, about 3.5 s a pair. Each has a
    1,800 s timeout, as the rising-ground go-around test has, for Linux
    debug's runners.
- `the_one_behind_two_going_around_whose_leader_goes_around_again_flies_a_racetrack_and_both_land`:
  the one ahead goes around a second time, below 300 ft on her circuit's
  approach. The one behind flies one to three racetracks, 7.6 to 9.2 nm
  out against limits of 7.7 to 9.3 (her base point plus the test's own
  2 nm). Both land, kept apart (least 684 ft within 1.5 nm). The pairs are the Cub behind the C172P, and the C172P
  behind the Cub and behind the F-15C. The F-15C behind the C172P is named
  and left out (above).
- `the_one_ahead_of_two_going_around_taken_by_her_pilot_on_downwind_is_still_followed`:
  the one ahead is taken by her pilot on her downwind leg; the one behind
  follows her every step of the next minute.
- `two_going_around_are_sequenced_by_the_callers_clock_not_their_own`: an
  aircraft made 30 s before another goes around 5 s before it, by the
  shared clock, and is number one.
- `who_follows_whom_round_a_circuit_is_by_time_then_down_the_runway_then_the_order`:
  six places, each asked. They cover earlier in time, a tie broken by
  distance down the runway, a tie broken by the order, another runway, no
  place in the middle, the highest of several ahead, and past the end.

**Seen to fail**, each bug put in and the failure watched:
- **The sequencing removed** (no controller told whom it follows), on the
  first nine pairs: all nine lost separation. The least horizontal distance
  within 500 ft was 46 m for the Cub behind the Cub, 72 m for the C172P
  behind the C172P and 304 m for the F-15C behind the F-15C. Every pair
  came to 0 ft vertically within 1.5 nm.
- **Each aircraft's own clock** for the place: "the one that went around
  first, by the shared clock, is number one" failed.
- **The place dropped when her pilot takes her**: "the one behind followed
  every step of the minute" failed.
- **No limit on the extension** (2,000 nm): all three pairs flew no
  racetrack and went 14.0, 23.7 and 12.4 nm out, against limits of 7.9,
  7.7 and 9.3. Before the test stated its own 2 nm, it read the circuit's
  figure, and this bug moved that too: only "no racetrack" caught it.
- **No offset**: the PA-28 behind the C172P lost separation, 368 ft and
  1,323 m. **The place's height only its set one**: three pairs with the
  PA-28 ahead fell to 476 ft.

Unchanged and green after the fix round: every test matching go-around,
circuit, land, separation, vacate, taxi, kept apart, lesson, controller and
the selftest, 302 of 302 on linux-release (one skipped, as it needs a key).
The server and the tests build on Windows (windows-debug).

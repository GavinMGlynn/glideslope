### Two going around together are kept apart and both land, 2026-11-05

**What it does not do, first.** The sequence is the go-arounds' alone. An
aircraft flying an approach straight in, or a departure, is not in it: the
one in the circuit does not wait for it, and it does not wait for her. The
separation monitor (sim/separation.hpp) and the runway's rule
(sim/vacate.hpp) still keep those apart, and that is a tail. The server
asks every aircraft it flies for the sequence, but no server run has two go
around together yet: the verification is the unit test below, the same
`sim::circuit_ahead_alt_ft` the server calls. The circuit is still a
left-hand one, and the sequence does not reorder itself. Number two waits
for number one even where she could have landed first.

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
  out. After a go-around the upwind leg is flown, with a turn to the
  upwind side. A pilot crossing the pattern crosses "at least 500 feet
  above pattern altitude". With a conflict on final, "an early go-around
  may be in order".
- AIM 4-3-3: propeller aircraft fly the pattern at 1,000 ft; large and
  turbine aircraft "not less than 1,500 feet AGL or 500 feet above the
  established pattern altitude". A pilot may vary the pattern's size with
  her aircraft's performance. AIM 4-3-12: a low approach is flown straight
  ahead.
- The Pilot/Controller Glossary's upwind leg is "used when issuing control
  instructions for separation, spacing or sequencing", quoted on Pilots of
  America. In ATSB AO-2016-107 a tower sends one round "maintain 1,000 ft
  and depart on upwind": the go-around standard is to climb to circuit
  height and keep separation from the other aircraft.
- Pilots' practice (Boldmethod, Flying Magazine, Pilots of America): the
  one behind extends her downwind to space herself on the one she follows.
  GASCo's bulletin argues against extending and for going around. Turning
  base once the traffic followed is behind the wing is a rule of thumb.
- Other simulators: Just Flight puts MSFS's AI go-arounds down to its
  default ATC leaving too little separation on approach. BeyondATC's AI
  goes around from an occupied runway and flies a full circuit to try
  again. Neither publishes a technique with a distance or a height.

**Chosen.** Two techniques together. *A different height*: the one behind
flies her circuit 700 ft over the highest circuit ahead of her. That is the
500 ft minimum the server keeps, plus the 200 the monitor holds in hand.
It is also the AIM's and the handbook's 500 ft over the pattern, for a
second pattern and for one crossing it. *Extend
downwind* (3-8-1): she does not turn base until everything ahead of her has
touched down (3-10-3: no arrival crosses the threshold before the one ahead
has landed). The runway's rule sends her round again if the one ahead is
still on it. So she is the height apart on every leg, and comes down
through the other's height only once the other has landed. The server does
not count a landed aircraft as traffic.

**The sequence is the order they went around in**, from the step each
did: her place is kept through another go-around. Two in the same step go
in the order of how far down the runway they were, and then the server's
order. Her place ends when she touches down.

**What changed** (`sim/circuit.hpp`, `sim/controller.hpp`,
`src/frontend/server/main.cpp`):
- `sim::CirclePlace` records her runway, when she went around, how far
  down it she was and her circuit's height. `Controller::circling()` holds
  it from the go-around until the touch.
- `sim::circuit_ahead_alt_ft` gives the highest circuit ahead of her, or
  none if she is number one.
- `GoAroundCircuit::follow` raises her circuit (and how far out she leaves
  downwind) and holds the downwind leg while there is traffic ahead.
- `Controller::follows_with` asks every step she flies the circuit. The
  server answers from every aircraft it flies, wrecks aside.

**Verification.**
`two_going_around_together_behind_the_slowest_are_kept_apart_and_both_land`,
`..._behind_the_c172p_...` and `..._behind_the_fastest_...`
(tests/unit/test_lesson.cpp). The ones flown are the slowest approach of
every landplane taught it (the J-3 Cub), the C172P, and the fastest (the
F-15C), each ahead of each. That is nine pairs, three to a test: a slower
one behind, a faster one catching up, and one behind its own kind. Each
test asserts its three were flown. The flying boat is named and left out:
it has no runway.

The test builds the situation explicitly: the one ahead on the three-degree
glidepath at 200 ft, the one behind two miles further out on it (636 ft
higher), and both told to go around in the same step. The two are asked of
each other whether the runway is clear and where they may turn off. From
the go-around until the first touches down, every step, they are 500 ft
apart vertically or 1.5 nm apart horizontally. The one behind followed,
the one ahead touched down first, both landed and taxied clear, and nothing
was wrecked.

| ahead | behind | least ft within 1.5 nm | least m within 500 ft |
|---|---|---|---|
| Cub | Cub | 683 | never |
| Cub | C172P | 700 | never |
| Cub | F-15C | 724 | never |
| C172P | Cub | 630 | never |
| C172P | C172P | 607 | never |
| C172P | F-15C | 773 | never |
| F-15C | Cub | 856 | 4,230 |
| F-15C | C172P | 966 | 4,171 |
| F-15C | F-15C | 687 | 3,820 |

Every one behind flew one circuit, with no second go-around. (linux-release)

**Seen to fail** with the sequencing removed (each controller not told
whom it follows): all nine lost separation. The least horizontal distance
within 500 ft was 46 m for the Cub behind the Cub, 72 m for the C172P
behind the C172P and 304 m for the F-15C behind the F-15C. Every pair came
to 0 ft vertically within 1.5 nm. In four pairs the one behind landed
first: C172P ahead of the F-15C, F-15C ahead of the F-15C, and the Cub
ahead of the C172P and of the F-15C.

Unchanged and green: every test matching go-around, circuit, land,
separation, vacate, taxi, kept apart, lesson, controller and the selftest:
287 of 287 pass on linux-release (one skipped, as it needs a key). The
three new tests take 13 to 18 s each. The server and the tests build on
Windows (windows-debug).

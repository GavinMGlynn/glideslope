### The circuit lessons flown by the go-around's circuit: one set of circuit rules, 2026-10-08 — fixes a tail

**What it is not, first.** The circuit itself is unchanged in what it does
not do (below, "A go-around is flown round a circuit and landed"): no
traffic, no right-hand pattern, no wind correction, its terrain a height.

**What changed.** The circuit lessons' AI (`fly_a_circuit` in
`tests/unit/test_lesson.cpp`) no longer has its own copy of the circuit's
legs: from the take-off at 650 ft it is flown by `sim::GoAroundCircuit`,
joined at the crosswind leg (`sim::CircuitEntry` - `from_take_off`, the
take-off's climb speed, seven tenths of the published climb up to 2,000
ft/min, and the take-off flap) where a go-around joins at upwind with twenty
knots over the reference and half the landing flap. The sizes, the legs and
where each ends are the class's alone. One rule moved to agree: crosswind now
ends far enough out to turn base and final from (the go-around's) **and**
within 100 ft of circuit height (the lessons'), since downwind is flown
level. The runway frame (`sim::along_runway_nm`, `sim::across_runway_m`) is
one function each, used by the class and the lesson test.

**Verification.** `the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief`
now also holds that each of the fourteen flew all five of the circuit's legs
from crosswind to final, in order; **its bands are unchanged** - no limit in
the lesson files or the test was moved - and every aeroplane is inside them. `a_circuit_flown_low_downwind_is_named_in_the_debrief`,
`an_instructor_demonstrates_a_circuit_and_hands_it_over`, both go-around
circuit tests, the runway-occupied go-around and the gusts test pass (7
tests, linux-debug). **Seen to fail** with the circuit's downwind leg never
ended (base 100 nm further out): "737-300 flew every stage of the circuit: 4
of 8"; reverted.

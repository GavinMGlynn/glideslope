### The circuit tests fly the simulation's instructor, 2026-11-28

**What now works.** `fly_a_circuit` in `tests/unit/test_lesson.cpp`, which
the circuit-bands test
(`the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief`) and the
downwind-sink test (`a_circuit_flown_low_downwind_is_named_in_the_debrief`)
measure the circuit through, no longer builds its own aircraft, controller and
go-around circuit: it constructs a `sim::Instructor` for the circuit lesson,
steps it, and watches from outside, up to the lesson's end. The tests' copy of
the instructor's circuit (about 150 lines: the loading, the take-off to 650 ft,
the join, the flap carried into the pattern, the hand to the approach) is
deleted. Neither test's name or checks changed; nothing was loosened. With
this, no test in `test_lesson.cpp` flies its own copy of the instructor.

**Instructor change, small** (src/sim/instructor.hpp): `pattern()` (the
go-around circuit once joined, else null), `on_the_approach()`, and
`fly_the_pattern_with(fn)`, a hook given each step's pattern modes before the
autopilot is, which the sink test uses to inject its 250 ft sink along the
downwind leg. Without a hook set, the instructor flies as before; the selftest
hash is untouched (the selftest flies no lesson).

**The leg count from outside.** The test counted the pattern's legs by the
leg before and after each `modes()` call; it now compares the leg before and
after each instructor step, starting from crosswind, where a pattern joined
from the take-off begins (GoAroundCircuit's constructor). Same count.

**Verification.** Both tests pass unchanged (linux-release), and every line
they print - per aeroplane, stages, touch-down point and speed, climb-out,
downwind and final bands, the debrief, the sink - is identical to what the
tests' own copy printed on the same build (111 lines, diffed). Two deliberate
breaks in `sim::Instructor`, built together: the fault hook not called turned
the sink test red ("737-300 sinking along the downwind leg is not
faultless"), and the light aeroplane's take-off flap not carried into the
pattern turned the circuit-bands test red ("pa28 ... said 1 things");
reverted.

From the review of #435: the step-count check (the test and the instructor
count the same steps) is made once, after the loop; the instructor's count
broken (`tick_ += 2`) turned the sink test red on it, reverted. 48/48 of
`ctest -R "instructor|lesson|debrief"` green after.

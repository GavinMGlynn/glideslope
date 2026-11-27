### The tests' demonstrations fly the simulation's instructor, 2026-11-28

**What now works.** Every `an_instructor_*` test in `tests/unit/test_lesson.cpp`
flies through `sim::Instructor` (one helper, `demonstrate_with_the_instructor`):
turns, take-off, approach, climb and descent, stall, the B-2A left thirty
seconds in a stall, and the circuit. The tests' own copy of the recipes
(`demonstrate_in_the_air`, the take-off's and approach's hand-built aircraft and
controller, the circuit's `demo` branch of `fly_a_circuit`) is deleted. Names
and checks are unchanged; no figure was loosened.

**Not part of it.** `fly_a_circuit` itself stays: other, non-instructor tests
(the circuit's bands, the downwind sink fault) measure the circuit with it and
it is not a demonstration.

**What changed in the flights** (all pass the same checks): the instructor
hands over the step the lesson ends, where the tests' copy waited a second (half
a second for the approach and circuit), so the pilot's hold and the tail after
taking back are counted from the hand-over; the pilot's hands are the
instructor's (`hands_at_rest`), and an aeroplane starts at the lesson's own
runway and height rather than the test's Sydney point.

**Instructor change, small.** Three additions to `sim::Instructor` for the
stall-recovery test: `leave_in_the_stall(seconds)`, `keep_showing_while(fn)` and
`recovering()`. Defaults leave behaviour as it was.

**Verification.** `ctest -R an_instructor_` (7 tests) green. A deliberate break
in `sim::Instructor` (the climb asked for 100 ft above instead of 1,100 ft) turned
`an_instructor_demonstrates_a_climb_and_descent_and_hands_it_over` red; reverted.
The selftest hash is untouched (no simulation change).

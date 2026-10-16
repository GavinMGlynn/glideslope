### A lesson figure that is the aeroplane own, 2026-09-22

**What is still missing: one lesson, for one class.** The take-off, for light
aircraft. No circuit, no stalls, no turns, nothing for the other six classes.

**The limitation written down an hour ago is gone.** A lesson figure may now
name a speed the aeroplane publishes rather than a number: `rotate`, `climb`,
with an optional offset - `rotate-3` - worked out per aeroplane by
`sim::departure_speeds` from its own `assets/figures/<id>.xml`. One line of
data holds the Cub to 31 knots and the Cessna to 52.

**This is what teaching a class means.** A lesson is written for a class of
aeroplane, and the four light aircraft rotate between about 34 knots and
about 55. A plain number can only be the slowest of them, so the take-off
lesson watched a 172 come off at 47 knots - against the 55 or so it should
use - and said nothing. With `rotate-3` the debrief names it: "Let her reach
the rotation speed before easing the nose up".

**The early rotation now says two things, and both are true.** She came off
early, and the attitude wandered while she did it - which is what hauling an
aeroplane off before its speed actually does. A debrief naming only one would
be hiding the other. The throttle fault, flown by the book in every other
way, still says exactly one thing.

**The parser refuses eighteen ways of being wrong now**, the two new ones
being a reference there is none of - `stall` - and one with an offset that is
not an offset, `rotate*3`.

**A false green, for the third time today, and the same shape each time.** A
`python3 - <<PY` script with an apostrophe escaped as `\\'` inside a
single-quoted string is a syntax error; the script writes nothing, exits
non-zero, and the `&&`-chained compile or test that follows runs against the
*unmodified* file and passes. "lesson.cpp compiles clean" was printed about a
file that had not been changed. Every script here now re-reads the file it
wrote and asserts the change is in it, which is what caught this one.

### What kind of flying an aeroplane is for, 2026-09-22

**What is still missing: the lessons themselves.** There is no lesson format,
no debrief, and nothing that flies a circuit, a stall or a turn to a
standard. What there is now is the thing a lesson has to be written against.

**The classes are data.** `REQUIREMENTS.md` section 4.2 names seven in a table
and no aircraft file said which it was, so a lesson - written per class,
because a circuit in a Cub and a circuit in a 747 are not the same lesson -
had nothing to attach to. Every one of the sixteen now carries a `class` line,
one of light-aircraft, seaplane, second-world-war, business-jet, airliner,
fighter or bomber; a file without one is refused where it is wrong.

**The test holds the data to the document, aircraft by aircraft.** It reads
`REQUIREMENTS.md`, finds the line each aeroplane is named on, and fails if
that row is not the class the file claims. Watched failing: with the F-22
marked a bomber it said "f22 says it is a bomber, and REQUIREMENTS.md names it
on the line: | Fighters | F-15 Eagle, F-22 Raptor | JSBSim's |". All seven
class names round-trip, a name that is not one of them is refused, and all
sixteen are counted.

**A verification that was not one, found and corrected.** The three checklists
written from handbooks earlier today were each reported as "8 of 8 checklist
tests pass". `CMakeLists.txt` copies `assets/` into `build/<preset>/data` **at
configure time**, and the tests read that copy - so running `ctest` after
editing an asset, without reconfiguring, exercises the *previous* file and
passes on it. All three of those runs were against stale data and proved
nothing about the new files.

It was found only because an unrelated `.aircraft` edit happened to trigger a
reconfigure and the cost was worth asking about. The files turn out to be
sound: after a reconfigure, with `diff` showing the copy matches the source,
all nine checklist tests pass - including the one that flies every aeroplane
to see that each band is reachable. **The fix is not to trust the green
tick**: after an asset edit, show the copy is current before believing a test.

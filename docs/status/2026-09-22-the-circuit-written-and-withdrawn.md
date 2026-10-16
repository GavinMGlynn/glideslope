### The circuit, written and withdrawn, 2026-09-22

**What is missing: the circuit lesson.** It was written, it would not fly,
and it has been taken out rather than shipped with a test that does not
demonstrate it.

**Two things the attempt produced, and they are kept.**

`lesson/turned-deg` is the runner own property: how far the aeroplane has
turned since the stage began, signed, and free of the wrap at north. A circuit
goes right round, so a raw heading cannot express one of its turns - a heading
of 340 is not less than 70 less eighty, and the stage would never end. Turning
is a difference, and differences are what the runner can work out and the data
cannot. The turns lesson used to end on `attitude/psi-deg <= start-60` and
passed only because its test happened to turn 90 degrees to 0 without crossing
north; it uses the difference now.

And a fault in the lesson itself: **a left circuit has four turns of ninety
degrees, not three.** The first draft had three and put the downwind leg
inside the turn on to it, so the leg began while she was still rolling out and
ended twenty degrees later - after which the aeroplane spent the rest of the
circuit being flown by a lesson that thought it was somewhere else. A leg
begins when the turn before it ends, which is why the turn on to downwind now
ends on the wings coming level rather than on ninety degrees of turn.

**The autopilot was suspected and cleared.** Commanded from 218 degrees to
250 - thirty two degrees to the right - while descending at 400 feet a minute
and holding 85 knots, it turned the *short* way, was on the heading inside
twenty seconds, held the speed to a tenth of a knot and descended at 396 feet
a minute. It was built by linking a standalone probe against the libraries
rather than by reading the code, because two readings of the heading loop had
already failed to settle it.

**So the fault is in the test that flies the pattern, and it is not found.**
The aeroplane climbed through downwind while the autopilot own modes read "no
altitude, -400 feet a minute" - the very configuration the probe descends in.
That is as far as it got. Guessing further would have cost more than stopping.

**Five of the six lessons stand**, all for light aircraft: the take-off, the
approach and landing, turns, climbs and descents, and stalls.

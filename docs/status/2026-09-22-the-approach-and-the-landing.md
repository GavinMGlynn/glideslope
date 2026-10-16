### The approach and the landing, 2026-09-22

**What is still missing: four of the six lessons, and six of the seven
classes.** There is a take-off and an approach, for light aircraft. No
circuit, no stalls, no turns, no climbs and descents.

**The approach lesson is flown by all four light aeroplanes with an empty
debrief**, down a three-degree glidepath from two miles, each at its own
reference speed: the 172 at 60 knots, the 182 and the PA-28 at 64, the Cub at
43. `vref` is the third figure an aeroplane can publish to a lesson, after
`rotate` and `climb`, and it comes from `sim::approach_speeds` - 1.3 times the
published stall with everything down.

**Flown twenty knots fast, the debrief names two things and both are true**:
hold the approach speed, and cross the threshold on speed. The aeroplane flies
a perfectly good approach the whole way down - it is simply doing it at a
speed it has no business using, which is what an approach flown fast is. The
approach autopilot is handed a reference speed the aeroplane has not got; the
lesson resolves `vref` from the published figures and sees the difference the
autopilot cannot.

**Two drafts of the bands were wrong, in opposite directions, and the
measurement settled both.**

The first held a speed band down to five feet and faulted every correct
landing: the PA-28 crosses its threshold at 64 knots and touches at 37, which
is a good landing. **The flare is where the speed is meant to go** - throttle
closed, nose up, flown onto the wheels as the speed decays - so the speed is
watched to twenty feet and let alone after that.

The second was a knot too tight at the bottom and far too loose at the top.
Against their own `vref` the four hold between `vref-2` and `vref+5` on the
approach; the band was `vref-5` and the PA-28 failed it by one knot, while a
top of `vref+25` would not have caught an approach flown twenty knots fast at
all. It is `vref-8` to `vref+12` now - about six knots of margin either side
of what correct flying actually does - and the measurement is written into
the lesson beside it.

**Per-stage instrumentation is what found it.** The first measurement took the
speed over the whole flight and mixed the approach, the threshold and the
flare into one range, which said only that something somewhere was out of
band. Recording the speed against the stage it happened in said which stage,
which aeroplane, and by how much.

### The autopilot's holds, 2026-09-18 — item done (CI run 35363959900)

**What is missing first:** it flies only what it is told. Nothing yet tells
it - no flight plan, no key in the client, no `--autopilot`; those are the
rest of Phase 4. Its gains are the Cessna's, found for it, and will not suit
a jet; its airspeed is held on the throttle alone, so in a climb the Cessna's
power, not the autopilot, sets how fast it can go up at a speed.

**The autopilot** (`sim::Autopilot`) is loops within loops on the aircraft's
state: heading to bank, 25 degrees at most, with an integral near the heading
that finds the bank the propeller's torque and slipstream need, to aileron,
damped by the roll rate; the ball to rudder; altitude to vertical speed, at
the climb rate asked for at most, to pitch - with an integral that winds only
while the pitch it asks for is the pitch it gets - to elevator, damped by the
pitch rate, with an integral that finds the trim; and airspeed to throttle.
It engages holding what the aircraft is doing, each integral set so that the
first controls it gives are the ones the aircraft had, and its bank, pitch
and throttle move at a pilot's pace from there.

**The tests:** at 4,000 ft and 100 KCAS, each hold's step, in calm air and in
moderate Dryden turbulence. Calm: heading 0 to 90 overshoots 2.2 degrees and
is within 2 for good at 40 s; altitude up 500 ft does not overshoot and is
within 20 ft at 68 s; airspeed up 10 kt overshoots 0.4 and is within 2 at 9 s;
climb 0 to 500 ft/min overshoots 0.2 and is within 50 at 8 s. In turbulence,
on ten-second averages: heading within 5 degrees at 29 s; altitude within 50
ft at 56 s; climb within 150 ft/min at 21 s; airspeed within 5 kt at 12.5 s,
then as much as 8.3 off, as the altitude hold trades speed for height through
the vertical gusts. Engaging it in a climbing turn moves no control more than
0.0025 of its travel in a step. **Watched to fail:** the trim not carried over
at engaging, the bank the wrong way, the throttle backwards, and no climb to a
new altitude - each caught.

**Found on the way:** without an integral the heading hold settled a degree
short of every heading; and a heading averaged in degrees from 0 to 360
wraps, so the test averages it from north.

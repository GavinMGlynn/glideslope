### The circuit, and a bounce that switched the steering off, 2026-09-23

**What is still missing from the lessons item: take-offs, approaches, climbs,
stalls and circuits for the ten aeroplanes that publish no stall speed and no
rate of climb** - the airliners, the fighters, the bomber and the flying boat.
Those wait on figures, not on lesson-writing.

**The circuit is the sixth lesson, and the first that is a whole flight.**
Eight stages - the take-off, the climb out, the crosswind turn, the turn on to
downwind, the downwind leg, base and the turn on to final, final, and the
landing - flown off a runway and back on to the same one. The take-off
autopilot flies her off, the plain autopilot flies the pattern a heading and a
height at a time, and the approach autopilot brings her back. All four light
aircraft and the Mosquito fly it to an empty debrief and stop on the runway.

`lesson/flown-nm` is the runner's second own property: how far over the ground
she has flown since the stage began. The downwind leg is level and straight,
so height, speed and heading read the same at both ends of it and nothing
about the aeroplane's state says how far along it is.

**A faster aeroplane flies a bigger circuit, and the two numbers that make it
are one number.** The downwind leg is left where a three-degree glidepath
passes through circuit height, so the approach autopilot is handed an
aeroplane on its path. A Mosquito flown round the Cessna's thousand-foot
circuit and given the approach at the Cessna's distance arrived low, still
turning, and put itself into the ground.

**The take-off autopilot latched "unstuck" on a bounce and never unlatched.**
The Mosquito skips once at about 97 knots; `unstuck_` went true, the departure
handed her to its airborne law - bank to hold a heading - and she came back
down and rolled on at 115 knots with nothing steering the wheels. Fourteen
seconds later she was forty-five degrees off the centreline with the rudder
sitting at a tenth of its travel, because the airborne law had only a little
sideslip to answer. **On the ground is the roll, whatever the stage said** -
the mirror of the rollout rule the landing autopilot already had, and for the
same reason: the wheels decide. `unstuck_` itself is untouched, because where
she first came off is where the ground roll ends and the published take-off
distances are measured from it - and all of those tests still pass unchanged.

**No take-off lesson watched the heading**, which is why a forty-five degree
swing had never been caught: they watched the throttle, the rotation speed,
the attitude and the climbing speed. Both take-off lessons now hold
`lesson/turned-deg` to twenty degrees through the roll, measured against the
1.1 to 11.1 degrees the five aeroplanes taught a take-off actually swing.

**Two structural faults in the lesson data, both found by flying it.** A stage
that ends on a turn begins part-way through the one before: the turn on to
downwind ends its stage eighty degrees in and the aeroplane is still turning,
so the last twenty degrees fall inside the downwind leg. Ending that leg at
twenty degrees of turn gave the Cub a four-second downwind leg, and the
Cessna passed by a single degree - which is luck, not a test. It ends at fifty
now. And the downwind leg used to end after a mile and a half, which is not
where the leg ends, so three miles of straight and level flying fell inside a
stage called "base, and the turn on to final" - and a sink along the downwind
leg was judged by a stage that watches no height, and went unremarked.

**Landing is asked of the wheels, not of the altimeter.** `position/h-agl-ft`
is measured to the aeroplane's reference point, and a Mosquito parked on the
runway reads seven feet, so a lesson asking for five could never be satisfied
by one however well it was landed. The lesson asks `gear/wow` now.

- `the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief` - five
  aeroplanes, eight stages each, empty debriefs, stopped on the runway.
- `a_circuit_flown_low_downwind_is_named_in_the_debrief` - let sink 250 ft
  along the leg, the debrief holds one thing and it is the circuit height.

**Seen to fail.** With the bounce latch put back, the take-off lesson reports
`mosquito-fb6: [The roll] Keep her straight on the take-off roll` and three
tests go red.

The selftest hash does not move - it replays a pilot input log and never
engages the departure autopilot.

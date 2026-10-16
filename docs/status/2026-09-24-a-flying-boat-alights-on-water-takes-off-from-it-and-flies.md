### A flying boat alights on water, takes off from it, and flies the circuit, 2026-09-24

**The Short S.23 flies the approach and alighting lesson to an empty
debrief**, down a three-degree glidepath on to open water. `Aircraft::in_water`
is JSBSim's `hydro/active-norm`, and `sim::Lander` counts the hull in the
water as touching as it counts weight on a wheel. The lesson is the FAA's
Seaplane, Skiplane, and Float/Ski Equipped Helicopter Operations Handbook
(FAA-H-8083-23), chapter 6. Fourteen aeroplanes now land in the approach
lesson.

**And takes off from it, and flies the circuit back on to it.** The S.23's
take-off is two stages, over the hump and on the step, so that a planing
attitude band (6 to 14 degrees; she planes at 7.7 to 12.2) watches the step
and not the plowing below it. Rotating early is not a flying boat's fault in
this model: held fully back she planes at the running attitude anyway and
comes off at the same 78 knots, the water and not the elevator setting her
attitude. The fault flown instead is the FAA seaplane handbook's (chapter 4,
"Porpoising"): the nose held low on the step, which porpoises her between
-5 and +7 degrees in growing oscillations and never lets her off. Its
debrief names the planing attitude and nothing else. The take-off harness
now takes the lift-off speed at the end of the first stage that ends on
height, not of the first stage.

Her circuit ends, as her approach does, off the step below 20 knots on the
water - afloat with the engines idling she is never quite still - within the
water lane and 30 m of its line; she touches 8.9 m from it. Her downwind
band is the light aircraft's 120 ft, where a provisional 200 was; sinking
250 ft is named. Fourteen aeroplanes take off by the book and fourteen fly
the circuit.

**The circuit is demonstrated and handed over, too.** The instructor item's
verification asks it of each lesson, and there were demonstrations of the
take-off, the approach, the climb and descent, the stall and the turns, but
not the circuit. `an_instructor_demonstrates_a_circuit_and_hands_it_over`
flies it for all fourteen aeroplanes taught it through a `Controller`, then
hands over and takes back: 0.0083 of a control's travel over and nothing
back, against a hand's 0.017 a step. Watched to fail with the handover made
to jump to the pilot's hands: the 737 stepped 1.0.

**The lessons are complete**: take-off, the circuit, climbs and descents,
turns, stalls, and approach and landing, for all seven classes - 42 lesson
files - each flown by the book to an empty debrief by every aeroplane whose
figures give the speeds it names, and each flown with a stated fault in one
aeroplane of every class, naming that fault and no other. Named exceptions:
the 747-400 and F-22A have no stall or climbing speed and are taught turns
alone; the F-15C, F-35B and Learjet are not flown rotating early (their
noses do not come up until well past their rotation speeds - a tail), their
classes' take-off fault being part throttle.

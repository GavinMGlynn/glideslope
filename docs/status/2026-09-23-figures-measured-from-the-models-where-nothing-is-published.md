### Figures measured from the models, where nothing is published, 2026-09-23

**What is still missing: the airliners, the fighters, the bomber and the
flying boat are taught climbs and descents and turns, and nothing else yet.**
Their take-off, approach, stall and circuit lessons are not written.

**The project owner decided on 2026-09-23 that a lesson's reference speed may
be measured from the flight model** where no handbook publishes one. Ten of
the sixteen aeroplanes have no public manual, so they could be taught only
turns - the one exercise that names no speed. Sixteen figures now carry
`from="measured"`, which is a new attribute on a figure and a different claim
from a published one: **a published figure holds the model to the world, a
measured figure holds the model to itself.** `figures.hpp` says so and
`docs/ASSETS.md` records every one.

- Stall speeds for eight: 737-300 105.72, 787-8 113.71, A320 113.33, A380
  104.86, B-2A 95.42, F-15C 151.06, F-35B 122.24, Short S.23 66.32 KCAS.
- Rates of climb and the speeds they are flown at, for eight: 737-300 4,438
  ft/min at 260 kt, 787-8 5,457 at 300, A320 5,804 at 300, A380 4,773 at 270,
  B-2A 8,271 at 230, F-15C 24,569 at 200, F-35B 20,390 at 200, Learjet 35A
  8,097 at 240.

**A measurement that moves with how you approach it is not a measurement.**
Each stall speed was taken from three entry speeds a good margin above the
stall, and the eight vary by at most 1.2 knots between them. **The 747-400 and
the F-22A do not**: the same aeroplane came out anywhere between 146 and 160
knots, and between 128 and 149, depending only on where the deceleration
started. They get no stall speed, and no climbing speed either - the climbing
speed is floored at 1.3 times the stall speed, so without one there is nothing
to anchor it to, and the 747-400's unfloored answer lands at 360 knots, within
five of its maximum operating speed. **An aeroplane's reference speeds hang
together, and those two have none of them.** They are taught turns alone.

**Making the deceleration stop only on a sustained rise was tried and
reverted.** It changed nothing at all for the eight that already held still -
the same numbers to a hundredth of a knot - and widened the 747-400's spread
to 21.8 knots and the F-22A's to 84.3, one run mushing down to 57. What those
two do near the stall is the models, not a late break in the measurement.

**A reference speed belongs to a weight, and the lesson now flies the
aeroplane at the weight its figures were measured at.** Without that the two
are about different aeroplanes: the F-35B's speeds were measured at its 39,750
lb combat loading and the lesson flew it at the 45,259 lb the model loads by
default. `sim::Aircraft::load` was already there; nothing was calling it
outside the figure flights.

**The climbing speed is the lowest speed that still climbs within ninety-five
per cent of the best rate**, never below 1.3 times the stall, **and never
below a speed the aeroplane can actually be climbed at**. That last condition
is not the same as the first two and it caught the F-35B: the sweep measures
at full throttle, where a fighter climbs from almost any speed, but a lesson
asks the autopilot for a speed *and* a rate, and near the stall it runs out of
nose before it runs out of thrust. At its 160-knot floor the F-35B pinned the
nose at the fifteen degrees it is allowed, sat at nineteen degrees of alpha
and sank at 2,000 ft/min, arriving on the runway still doing 160 knots. 180
still pins the nose and manages 112 ft/min; 190 climbs; 200 is taken, where
the nose sits at 13.6 degrees and there is room. The peak itself
is no use for teaching: a jet's rate goes on rising to speeds it is never
operated at - the A320's peaks at 400 knots - and a fighter's is flat within a
few per cent over hundreds of knots, so for the F-15C and the F-35B the floor
is what chose the speed. The answers land where the real aeroplanes climb:
260 knots for a 737, 300 for an A320, 240 for a Learjet.

**A lesson belongs to a class; the figures belong to the aeroplane.** The two
do not always meet, and until now nothing noticed: `sim::cannot_be_taught`
reads the figures a lesson names and says which one an aeroplane has not got,
and the tests leave that aeroplane out **by name, with its reason**, rather
than flying it with a reference speed of zero. That had been happening - a
lesson naming a figure the aeroplane lacked resolved it to zero and the stage
ended on the first tick, which is a test that passes without testing anything.

Climbs and descents are now taught to the airliner, fighter, bomber, seaplane
and business-jet classes as well as the light aircraft and the Mosquito -
fourteen aeroplanes fly the lesson to an empty debrief, and the two that
cannot are named in the test's own output with the figure they lack.

**Two more things the weight turned up.** A demonstration used to ask every
aeroplane for 600 ft/min; a fully loaded Piper J-3 Cub climbs at 450, and it
had only ever managed 600 because it was being flown lighter than its own
figures. Demonstrations ask for seven tenths of the rate the aeroplane has
now, capped at the 600 the jets were already being given. And **the PA-28
loses 321 feet entering a stall where the 172 loses 61**, at the same 2,400
lb, flown by the same autopilot - so the light-aircraft stalls lesson carries
a band four hundred feet wide on one aeroplane's account. The lesson file says
so rather than passing it off as a tolerance, and it is a tail.

**Two tests named the B-2A as the aeroplane that publishes nothing**, which it
no longer is. They use the 747-400 now, whose stall speed is the one that
would not hold still.

**A stalls lesson for the airliners, fighters and bomber was written and
withdrawn the same day.** A lesson's `stall` is the landing-configuration
stall speed, and the exercise is flown clean. For a light aeroplane those are
close enough that slowing to six knots above the landing stall works; for a
737-300 the landing stall is 105.7 knots and clean it gives up at about 155,
so an entry ending twenty-five knots above the landing stall asks for a speed
it cannot reach without stalling first. Traced from 20,000 ft it holds its
height down to 159 knots at 12.6 degrees of alpha, and thirty seconds later it
is at 52 degrees of alpha, 26 degrees nose down and falling at 21,000 ft/min,
and nothing recovers it. **A swept wing past the stall departs; it does not
mush like a Cessna.** The exercise has to be flown in the configuration its
reference speed is measured in, which no lesson handles yet, so it is a tail
rather than a lesson with wider bands.

**Still at the model's default weight: the take-off lesson, the approach
lesson, their demonstrations and the circuit**, which each build their
aeroplane directly rather than through `airborne`. They pass, which means the
difference is small for those aeroplanes and not that it is absent. A tail.

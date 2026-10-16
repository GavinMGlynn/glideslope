### The AI pilot can take off, 2026-09-23

**What is still missing: the approach, the climb and the stall have no
demonstration.** The machinery is there for the approach - the AI pilot can be
handed a runway to land on - and the test is not written. The climb and the
stall are demonstrated by driving an autopilot directly rather than through a
`Controller`, which is not a thing an instructor can hand over.

**`sim::Controller`'s AI could only be handed an aeroplane already flying.**
Its AI was the autopilot and the navigator; a take-off and an approach were
flown by `Departure` and `Lander` from a frontend, outside any controller. So
a take-off had no demonstration to hand over *from* - which is why the
instructor item could only cover turns.

`to_ai_take_off` and `to_ai_approach` hand the AI a runway. When the take-off
or the landing is over the plain autopilot engages **from the controls it
left**, so the aeroplane does not lurch at the moment the AI stops taking off
and starts flying.

**Four aeroplanes demonstrate a whole take-off** - the Cessna, the PA-28, the
Cub and the Mosquito - all three stages of the lesson with an empty debrief,
then the controls go over at 0.0083 and come back at 0.0018 to 0.0020, where
a pilot's hand moves 0.0207 in a frame.

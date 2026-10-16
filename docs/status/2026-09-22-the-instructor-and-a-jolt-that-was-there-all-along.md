### The instructor, and a jolt that was there all along, 2026-09-22

**What is still missing: the instructor can demonstrate one lesson.** Turns.
`sim::Controller`'s AI is the autopilot and the navigator; it does not wrap
`Departure` or `Lander`, so a take-off, an approach and a stall have no
demonstration to hand over from.

**What works, for four classes.** The Cessna, the Learjet, the Mosquito and
the A320 each fly all three stages of their turns lesson with an empty
debrief, then the controls go to a pilot whose hands are nowhere near where
the AI had them, and come back three seconds later. All seventeen controls are
measured at both swaps: **0.0083 handing over, 0.0017 to 0.0019 taking back**,
against the 0.0207 a pilot's hand moves in a frame.

**The verification found a defect that had been there since the autopilot was
written.** Taking the controls back stepped the elevator by up to **0.80 of
its travel in a single frame** - forty times a hand's pace - and the
`Controller`'s own header promises that handing over steps nothing.

The cause: the autopilot seeded its pitch command to the attitude the
aeroplane had, and then `fly()` clamped that command to the envelope it is
allowed to ask for. Handed an aeroplane at 33 degrees nose up when the limit
is 15, the command snapped to 15 in one frame and the elevator jumped by the
difference. **The envelope now bounds the target rather than the command**, so
the command walks to it at the loop's own pitch rate - which is the autopilot
taking over rather than grabbing.

**Three wrong measurements before the right one**, each worth naming because
each looked like a defect and was not. Commanding a ninety-degree turn in the
same frame as the swap measured the command, not the handover, and read 1.29.
Taking the worst step over the second after the swap measured the autopilot
flying and called it a jolt. And a "gentle" pilot input held for fifteen
seconds rolled the aeroplane into a spiral - pitch -26, bank -62 - so what was
being measured was an autopilot recovering from a dive. A swap is one frame,
and that is what "no step" means.

**And the fix moved a figure in a lesson.** The 172 left late in a stall now
loses less height, because the entry is flown more gently, so
`light-aircraft-stalls.lesson` records thirty-five seconds where it recorded
twenty-five, and says why.

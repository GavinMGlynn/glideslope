### The approach demonstrated, and a handover that broke on its own limit, 2026-09-23

**What is still missing: the climb and the stall have no demonstration an
instructor can hand over.** Both are flown by driving an autopilot directly
rather than through a `Controller`, so there is nothing to hand over from.
The turns, the take-off and the approach are demonstrated.

**The approach.** Four aeroplanes - the Cessna, the PA-28, the Learjet and
the Mosquito - start two miles out on a three-degree glidepath, and
`Controller::to_ai_approach` flies all three stages of the approach lesson to
an empty debrief. The controls then go to a pilot whose hands are nowhere
near where the AI had them, and come back. Handing over 0.0083; taking back
**0.0000**, exactly.

**It took two defects to get to that 0.0000, and the second was the
interesting one.** Taking the controls back stepped the Learjet's elevator by
0.685 of its travel in one frame, and the Mosquito's rudder by its *entire*
travel - 1.000, in one frame, where a hand moves 0.0083.

Three of the autopilot's loops seeded an integral with a term that cancelled
the law's own damping, so that the first step after engaging reproduced the
controls it had been handed:

```
rudder_integral_ = controls.rudder + rudder_per_degree * beta;
...
c.rudder = -rudder_per_degree * beta + rudder_integral_;   // cancels, in theory
```

**The cancellation breaks whenever the damping term is larger than the
integral's own limit.** The integral is clamped to a control's travel. The
Mosquito, skidding in its landing roll, had a sideslip of about ninety
degrees: the seed wanted to be 9, kept 1 of it, and what came out was
`-9 + 1` clamped to the stop. The same arithmetic stepped the Learjet's
elevator off a large pitch rate.

**The loops no longer derive the cancellation; they measure it.** The first
call to `fly()` computes what the laws actually give, takes the difference
from the controls it was handed as an offset, and fades that offset out over
two seconds - the treatment the aileron channel already had. There is no term
to get wrong and no limit for it to fall foul of, and the offset is exact by
construction. On top of that **no loop may move a control faster than a
pilot's hand at any time**: 1/120 of its travel in a step. That bound was
being broken by 2.0 - one stop to the other - by the rudder law following
sideslip while recovering from a spiral.

**The test that should have caught this flew one Cessna, in one gentle
climbing turn, and passed.** It now walks **every aeroplane the data holds
through every state one can be handed over in**: trimmed and level, a
climbing turn, gliding, skidding on full rudder, rolling on full aileron,
bunted hard nose down, pulled into a stall, and a spiral on crossed controls.
16 aeroplanes x 8 states = 128 handovers, and it states those numbers and
fails if the walk does not cover them.

- `the_autopilot_never_moves_a_control_faster_than_a_pilots_hand` - 128
  handovers; worst first step **0.0000**, worst step in the five seconds
  after **0.0083**, which is the bar itself.
- `an_instructor_demonstrates_an_approach_and_hands_it_over` - four
  aeroplanes, three stages each, empty debriefs.

**Seen to fail.** The old autopilot was put back and the walk reports first
steps of 0.84, 1.70 and **1.80** of travel across the fleet - and 0.110 for
the 737-300 in the *gentle* climbing turn that was the only case the original
test ever flew.

**A flight model is only asked what it can answer.** JSBSim asserts on a table
lookup outside its range, and a 1930s flying boat put into a spiral at 20,000
ft left its own aerodynamic tables and aborted the run. The fly-in now stops
at the edge of what a model covers and the autopilot is handed the most
extreme state that model can actually produce. Stopping at the same bound the
measurement needs left 19 of the 128 handovers with nothing measured at all,
and the test said so rather than passing on an empty walk; the fly-in stops
inside it instead.

The selftest hash does not move: the selftest replays a pilot input log and
never engages the autopilot.

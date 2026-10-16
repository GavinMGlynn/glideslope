### The stall recovery measured on #155's stack, and an elevator limit on the angle of attack and the load tried and not kept, 2026-10-10 — item still open, no code changed

**What is still missing, first** (the plan item, unchanged): left thirty
seconds, the A320 (1.95 g) and the Mosquito (2.22 g) are over 2 g; at the
warning the F-15C, F-35B, Learjet 35A and Short S.23 are past their lesson's
height and the Mosquito is never level again; and the AI pilot does not
notice a stall. Nothing was committed but this entry and the plan's text.

**Measured on this base** (PR #155's branch, on #153's climb seed and #154's
bank mode; linux-release; lost from the hand-over / bound, peak g): the stack
moved the figures a little and fixed none.

| Aeroplane | at the warning | left 30 s |
|---|---|---|
| 737-300 | 773 / 1,300, 1.15 | 1,223 / 2,476, 1.72 |
| 787-8 | 657 / 1,300, 1.16 | 766 / 1,740, 1.20 |
| A320 | 610 / 1,300, 1.20 | 1,240 / 1,584, **1.95 named** |
| A380 | 908 / 1,300, 1.30 | 1,484 / 2,908, 1.66 |
| B-2A | 112 / 300, 1.14 | 998 / 2,001, 1.61 |
| C172P | 75 / 300, 1.05 | 75 / 568, 1.28 |
| C182 | 71 / 300, 1.05 | 38 / 539, 1.53 |
| F-15C | **522 / 500 named** (was 543), 1.51 | 1,424 / 3,577, 1.57 |
| F-35B | **773 / 500 named**, 1.31 | 3,252 / 6,257, 1.68 |
| J-3 Cub | 29 / 300, 1.05 | 42 / 308, 1.26 |
| Learjet 35A | **385 / 350 named** (was 410), 1.24 | 434 / 1,359, 1.26 |
| Mosquito FB.VI | **not recovered, 3,095 lost, named** | 1,258 / 1,899, **2.22 named** |
| PA-28 | 131 / 300, 1.20 | 120 / 515, 1.26 |
| Short S.23 | **184 / 200 named** | 146 / 640, 1.33 |

**What the A320's 1.95 g is, traced.** Handed over left thirty seconds at
156 kt, 26.9 degrees of alpha against a peak at 16.0, sinking 10,100
ft/min, her elevator stays nose-up the whole recovery (+0.92 falling only to
+0.55) although the pitch asked is 12 to 14 degrees below her nose: the
elevator's trim integral wound to its stop holding the height into the
stall, and the pitch error's 0.05 a degree does not outweigh it. Her wing
comes back through its peak at 168 kt, and pulls 2.05 g for an instant. The
Mosquito's is different: her elevator is already 0.5 nose-down when she
pulls 2.1 g at 140 kt with full flap and the gear down - her own pitching
moment, not the autopilot's trim.

**Tried and not kept** (AC 120-109A's template: nose-down pitch control,
then nose-down trim as needed, "smooth, deliberate, and positive control
inputs ... to avoid unacceptable load factors and secondary stalls"):
- **The elevator pushed by the angle of attack over the angle held**
  (0.1 of travel a degree) **and the trim unwound while over it** (0.05 a
  second a degree), the angle led by half a second of its rate: the A320 at
  1.79 g, inside 2 g with the 10% in hand; the Mosquito 2.12. But the F-35B
  left thirty seconds lost 6,544 ft against her bound of 6,257, twice what
  she does now, and the 737-300, A380 and B-2A 20 to 60% more: unwinding the
  trim unloads a wing the recovery then dives on.
- **With a load-factor term too** (1 of travel a g over 1.6, led a quarter
  of a second, the trim unwound 0.5 a second a g): the Mosquito 2.02, the
  A320 back to 1.99 and 1,601 ft against 1,584, the F-35B 7,251.
- **Without the lead on the angle**: the A320 2.38 g and the F-35B 2.22 g.
- **The trim's unwinding alone** (no push on the elevator): the A320 1.84 g,
  the Mosquito 2.13, the F-35B 6,094 ft, which with the 10% in hand is
  still past her 6,257.
None changed the heights at the warning by more than a few feet. The same
gains do not suit both an airliner diving at 27 degrees and a fighter
mushing at 29; what is wanted is an angle-of-attack hold on the elevator
with its own integral and anti-windup (back-calculation of the trim against
the limit), not more terms on the pitch loop. Not attempted in the time.

**The F-15C and F-35B at their warnings are a lesson's entry, not only a
recovery.** Both are handed over already sinking (the F-35B 4,400 ft/min at
130 kt, 28.9 degrees of alpha, her nose at the envelope's 15 degree top):
the altitude hold that enters the stall cannot hold her height at stall+8
in the landing configuration. At 130 kt against a 122 kt stall she has
(130/122)² = 1.13 g to arrest that sink with, so most of her 773 ft is
gone before the speed comes; a recovery law cannot give that back.

**Verified**: both stall checks run on the base as above (both pass with
their names). No code changed, so no test was re-run beyond them, and the
selftest hash does not move.

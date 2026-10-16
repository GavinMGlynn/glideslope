### The rollout judges her lift by her airspeed in all air: thirteen roll out into a steady headwind, the S.23 does not, 2026-10-13 — item still open

**What is still missing, first**: **the Short S.23 skips 1.03 ft off the
water** after alighting into a steady 15 kt headwind (7.58 ft before). She
alights at 1.6 degrees, her hull's planing pitches her up to 4 against
forward elevator, and at 80 kt, her reference speed, her wing lifts her off
for about a second and a half. The cause is her hull and her flat touch,
not how the rollout judges her lift. The item waits on Phase 9's "The
S.23's hull is not a planing hull" (open PR #184). She is named in the new
test, with a note to judge her again once her hull is fixed, and the item
stays open on her.

**Found on the way, a new item: the rollout's autobrake winds up when there
is nothing to slow.** The autobrake adds brake at 0.1 times the shortfall
from its target deceleration on every step, with no limit on the windup.
A C172P creeping sideways with no deceleration to meet therefore has her
brakes wound on. At a73844b2 they went from 0.31 to 0.58 and put her on her
nose (below). Holding the brakes below 3 kt, tried and not kept (below), is
one answer; one that does not leave a creep unstopped is still to be found.
Verification: a rollout held at low groundspeed with no deceleration keeps
her brake bounded and her nose above -5 degrees.

**The change** (`sim::Lander`'s rollout): whether she still flies is judged
by her airspeed along her nose, or her groundspeed where that is more, in
steady air as well as in gusts. Judged by her groundspeed in steady air,
into a steady 15 kt headwind the C172P rose 2.66 ft, the C182 4.79, the Cub
6.59, the PA-28 4.58 and the S.23 7.58 after touching; the C172P, C182 and
PA-28 struck their airframes.

**Why airspeed was kept to gusts: the learnt landing's strongest left
crosswind.** Reproduced at a73844b2 (the gusts commit) with the same one-line
change: one of its 32 corners (1.60 nm, -60 m across, +20 m high, +5 deg,
67.6 kt) did not stop. It was neither the policy's observation nor the
hand-off: the C172P rolled out, ended five degrees right of the runway
and across the wind, and was blown sideways at 1.5 kt, so she never got
below the 1 ft/s that is stopped. The autobrake, meeting no deceleration,
wound her brakes from 0.31 to 0.58, her nose rose to 9.6 degrees, and the
gear threw her on her nose (pitch -81) and off the runway. The rollout then
saw her off the ground and flew her away at full power. At today's main, the
lander's later changes give that corner a different rollout, and all 32 of
every wind stop. **Tried and not kept**: holding the brakes below 3 kt.
That stopped the corner at a73844b2, but it left a half-fuel learnt landing
in a 10 kt crosswind creeping and not stopped.

**Verification** (linux-release):
- New: `in_a_steady_15_kt_headwind_every_aeroplane_lands_and_rolls_out_without_lifting_off_again`.
  Every aeroplane taught the approach (14, asserted) lands in a steady
  15 kt down the runway, with no turbulence or gust factor. None may go
  around, and each must stay upright and on her wheels, rise no more than
  0.5 ft after touching, and stop. Thirteen are judged; the S.23 is named.
  Seen red on the old law: the C172P, C182, Cub, PA-28 and S.23 above.
  Now the thirteen rise at most 0.13 ft (F-15C); the F-35B rises 0.06 and
  the rest 0.00.
- New: `in_a_steady_10_kt_tailwind_every_landplane_lands_and_rolls_out_without_lifting_off_again` (renamed `..._every_aeroplane_...` on 2026-10-14, when the S.23 was judged in it).
  This is the same check in a 10 kt tailwind, and again the S.23 is named.
  All thirteen pass: the most is the F-35B's 0.22 ft, and every one stops.
  It guards against a regression and was never red, because in a tailwind
  her groundspeed is the higher figure and the old and new laws agree. The
  touch sinks here are not judged; the F-15C's is 549 ft/min and the
  F-35B's 571.
- Regression, kept: the learnt landing's corners pass in every wind, 32
  each. The left crosswind's worst is 1.51 m across at 130 ft/min. Its
  fuel, crosswind and server tests pass too.
- Re-run: every test matching land, rollout, crosswind, learnt, gust, wet,
  lesson or stop (478 matched), all green, but for one client test left out
  because no client was built here. Also the gusts tests' eight seeds, the light-and-heavy
  calm landings, and every crosswind and wet-runway test (51).
- The selftest hash does not move, `182dd6c996e0ee4c`.

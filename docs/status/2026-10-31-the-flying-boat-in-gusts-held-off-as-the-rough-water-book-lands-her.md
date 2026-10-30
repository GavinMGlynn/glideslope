### The flying boat in gusts, held off as the rough-water book lands her: no skip past 2.7 ft and no wreck, but she still rises past a foot on some seeds, 2026-10-31 - item still open

**What is still missing, first**: the item. The S.23 still skips in gusts
on two or three of the eight seeds - she rises 0.82 to 1.02 ft off the
water against the gust test's 0.75 (a foot, less its quarter-foot margin)
in the best variant - and stays named in the gust tests. Nothing in the
code on `main` changed: docs only. (Later the same day the item landed: her hull damped
from its first touch, the last row's variant passes every seed -
docs/status/2026-10-31-the-flying-boat-lands-in-gusts-her-hull-damped-from-its-first-touch.md.)

**The owner's decisions, 2026-10-31**:
1. A gust's climb in a level hold-off is not a balloon if she holds level:
   it is judged by her height and her touch, not by the flare's climb
   limit.
2. Her hull may touch at the seaplane handbook's flatter rough-water
   attitude rather than at the landplanes' touchdown attitude; the judge's
   change applies to the hull only, and the landplanes' limits are
   unchanged.

**The rough-water attitude, sourced.** FAA-H-8083-23 (2004), ch. 6,
"Rough Water Landing" (p. 6-7, part 3, SHA-256 `91208b29...`, its row in
docs/ASSETS.md): "make the touchdown at a somewhat flatter pitch attitude
than usual. This prevents the seaplane from being tossed back into the air";
and level off just above the water with power "to maintain a rather flat
attitude ... and then reduce the power to touch down". It gives no figure,
and neither does its chapter 8 (open sea, part 4, SHA-256
`8a80b1c94150421d4f09b590e6f0377cf6de4b69e01a2eb517f307f39c4742f9`), which
holds "the landing attitude" to the water. **The floor is NACA TN 923**
(Benson, 1943, its row in docs/ASSETS.md), "Concluding Remarks", item 5:
"Abnormally low trims (possibly 3° or less) should be avoided at speeds
approaching get-away and on landing to prevent low-angle porpoising" - a
trim referred to the forebody keel, which on the S.23's model is level in
her body axes (tools/make_short_s23.py, `KEEL`), so her pitch is her trim.
So "flatter than usual" is flatter than her calm touch (4.2 degrees) and
not below 3; the hold-offs below were flown with the nose capped at 4.0 to
6.5 and held at 3.5 on the step. (Searched besides: the Sunderland's and
the S.23's pilot's notes, the US Naval Academy's seaplane appendix, the
AOPA and ACS rough-water guidance - none gives an attitude in degrees; the
ACS study guide has the touch "slightly more nose-up", against the FAA's
flatter.)

**Built and measured** (`sim::Lander`'s flare and on-step hold, a hull in
gusts only; the gust tests' eight seeds, linux-release, the S.23 alone).
The landplane's gust laws - half her power held to the touch, aimed at 150
ft/min, the nose let down on a coming climb - taken off the hull; her flare
flown to a hold height over the water and held level there (owner's
decision 1: no climb limit while she holds level, her nose not raised while
she climbs; it rose at most 0.35 degrees in any climb). Rise is off the
water after the touch, limit 0.75; sink at the touch, limit 335.

| variant | touch, deg | touch, kt | rose, ft | sink, ft/min |
|---|---|---|---|---|
| hold 6 ft until her smoothed airspeed is Vref, then the calm flare (round 1's) | 4.1-6.4 | 70-87 | 0.25-2.70, 4 over | 48-274 |
| hold 3 ft, ends 4 kt under Vref, nose capped 4.0 after it, on-step held at 4.0 | 3.3-4.0 | 72-80 | 0.00-1.31, 4 over | 143-339 |
| hold 6 ft, ends 7 kt under Vref, cap 4.0, on-step 4.0, the gust cushion's power kept | 3.2-4.3 | 68-76 | 0.00-1.90, 3 over | 142-433, 2 over |
| hold 1.5 ft, ends 7 kt under, cap 4.5, cushion | 3.8-6.0 | 68-77 | 0.00-2.67, 1 over | 73-381, 2 over |
| hold 1.5 ft to the touch, cap 4.0, on-step 3.5 kept between skips, cushion | 2.8-4.5 (one under TN 923's 3) | 72-86 | 0.00-1.15, 2 over | 82-256 |
| the same, cap 5.0 | 3.5-5.1 | 64-81 | 0.00-1.02, 3 over | 25-313 |
| the same, cap 6.5 | 4.1-5.9 | 67-77 | 0.00-2.45, 2 over | 77-368, 1 over |

Every variant had a seed fail and none wrecked her; round 1's (main's)
figures were rises of 0.89 to 10.04 ft and two wrecks.

**What the traces show** (per-step, seeds 3, 4 and 6). Two ways she rises,
and they pull against each other:
- **Fast, she flies off the water.** Touching at 80 kt at 4 degrees and 60
  to 90 ft/min, her hull's lift and her wing's together lift her straight
  back off - the climb reversed from -86 to +116 ft/min in a fifth of a
  second - and a gust over her wing at 80 to 86 kt holds her up for one to
  four feet. Her flaps, commanded up at the touch, have moved 2 per cent in
  the seconds that takes.
- **Slow, she bounces off a hard touch.** Held off until she is 7 to 10 kt
  under Vref, a lull at the last foot puts her on at 300 to 430 ft/min and
  the hull throws her back up a foot or more.
- **And between skips, the landplane's gust stick lowered her nose**: off
  the water and less than a foot up she was held at four degrees under the
  attitude she touched at, down to 0.8 degrees, below TN 923's 3, and she
  porpoised. Holding her on-step attitude between skips (in the variants
  from the fifth row) took that away.

With a 15 kt gust factor her airspeed at the touch varies about seven
knots either side of what she is held to, which is a fifth of her wing's
lift either way: a touch slow enough that a gust does not fly her off is
one a lull drops on to the water.

**What would decide it**, for the owner: (a) whether the rise after a
hull's touch in gusts may be judged against the seaplane book's own bounce
rule ("If severe bounces occur, add power and lift off", p. 6-7) rather
than the landplanes' foot; (b) a hull's sink at the touch damped as the
water damps it - her model's heave on the step (tools/make_short_s23.py)
has no damping of its own, and a rebound larger than the arrival says it
may be too stiff; that is a model change, sourced or not made; (c) the
gust seed's turbulence near the water, which is what a lull at the last
foot is.

**Verification**: none landed. The figures above are from the gust tests'
own output (`in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_8`, linux-release, the S.23 alone), with each variant built in; and a per-step trace of her keel's height, attitude,
airspeed, sink, elevator, throttle and flap for seeds 3, 4 and 6.

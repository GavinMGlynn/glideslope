### Why the learnt landing flies fast, and two rounds of training it stabilized, 2026-10-10 — item moved to Later

**The owner's decision, 2026-10-10:** the item moves to Later as a larger
project - the flare trained as its own stage, or started from the approach
autopilot's flare. These findings land as they are, and the tools still make
the committed policy. **The speed costs are an option, off by default**:
`Flier(..., speed_costs=False)`, `LandingEnv(speed_costs=False)` and train.py's
`--speed-costs`. The default reward is the one the committed policy was
trained with, so train.py run with the command and seed recorded in its header
trains it again. **Checked:** six episodes of the training's own starts
(seeds 160000 to 160005, flown by the committed policy, every other one with
noise of 0.3 added to its actions), 7,398 steps through main's env.py (at
fca3494a) and through this one side by side. Every observation and ending is
identical, and so is every reward with the default: 0 of 7,398 differ. With
`speed_costs=True`, 4,048 differ, by up to 2.6. (A scratch script, not
committed.) The `--every` option keeps its default of a million, where the
checkpoints were before.


**What is not done first.** The item. No checkpoint trained here is
stabilized by 500 ft from its gate's corners, so the committed policy is
unchanged, the learnt landing keeps its exemption from the stabilized gate
(controller.cpp's comment still says why), and its tests are unchanged.
What lands is the training's reward for the speed from 500 ft down, and the
gate's own judgement in the training's log; the next part of training starts
from them.

**Why it flies fast.** The approach's speed was shaped only by a potential
(env.py's `potential`, Ng, Harada and Russell 1999), and a potential changes
no optimum: the shaping telescopes to the potential at the end less the one
at the start, and this one fades to nothing at the runway - so in all the
policy was rewarded for the touch alone. It learnt to dive down the
glidepath on power and touch from a fast approach. Traced at the gate's
corner 2.4 miles out, 60 m left, 20 m low (calm, 57 kt): throttle up to 0.92,
pitch down to -5.9 degrees, 91.6 kt at 466 ft, back to 61 kt only at 62 ft.
The observation has what it needs - the speed off the reference is one of
its inputs (`(kcas - vref) / 10`) - so option 2 of the item (the observation
lacks it) does not apply.

**The committed policy at the gate's 160 corners** (a scratch script flying
them in JSBSim's Python bindings, judged as sim::unstabilized judges, two
seconds running between 500 and 50 ft): 160 of 160 unstabilized; at most
35.3 kt fast and 9.5 kt slow; all 160 land within the limits (worst 190
ft/min, 2.14 m). In C++, with the gate judging the learnt landing (a change
made and kept off this branch, below), the calm corners went round at "31 kt fast
at 482 ft" and the like, 41 to 53 s running.

**The reward** (tools/rl/env.py, `STABILIZED`): each tenth of a second
between 500 and 50 ft over the runway, outside +5/-2 kt of the approach
speed - inside the gate's +10/-5, a margin - costs `per_kt` a knot to 30,
and outside the gate's own band `outside_gate` more. The Flier judges the
gate's way too, step by step (`judge_the_gate`, `Flight.most_unstable_s`),
and train.py logs how many of the last 400 flights it would have sent round.

**Tried** (seed 160, four environments, niced, from the committed policy's
checkpoint, step27000004.zip, sha256 ae2920df...; ~3,500 decisions a
second):
1. `per_kt` 0.02, nothing for the gate's band, learning rate 1e-4, the noise
   held as before (`--log-std -2.5257`), four million decisions. The
   training's flights the gate would send round fell from 385 of 400 to 58
   by 29.9 million, landings within the limits 399 of 400; then rose again
   to 380 by 30.8 million. At the corners: 29M 94 of 160 unstabilized (3
   short of the landing limits; at most 18.1 kt fast, 12.7 slow), 30M 86 (0
   short; 15.6 fast, 10.0 slow).
2. From 30M at learning rate 3e-5: the same turn, 86 to 388 of 400 within a
   million; at 31M all 160 corners unstabilized and 110 short of the
   landing limits (off the centreline).
3. `per_kt` 0.05 and `outside_gate` 0.3, from 30M at 3e-5: the same, 370 of
   400 by 31M; at the corners 160 unstabilized, 120 short.

**Why it turns slow.** Traced at 31M (part 3), calm, the near low corner:
on speed from 400 to 120 ft, then the flare begins at about 100 ft and
takes the speed to 49.5 kt by 53 ft - 10 kt slow where the gate still
judges it - and to 45 kt at the touch, 106 ft/min. A flare from 100 ft buys
a soft touch the touchdown's reward pays for (some 80 a touch) more than
the speed's cost below 100 ft takes away. The gate judges down to 50 ft,
where the policy has been flaring for 50 ft. **What next** (not tried, the
time box spent): the flare taught to start lower - the speed's cost made
to outweigh the touch's sink term, or the speed judged in training to the
height the simulation's own flare starts - and the slow corner's pull-up
from 20 m low (52 kt at 469 ft) needs the same. Starting again from the
committed checkpoint rather than from 30M, whose landings were already
turning, is the first thing to try.

**The C++ for the gate, made and taken out.** LearntLander judged by
sim::unstabilized at its policy's own speed each step (its `unstabilized()`
reason once sustained, `most_unstable_s()`), and the controller going
around for it - to the gate's runway, or for `to_ai_learnt_approach`,
which sets no gate runway, the learnt landing's own - counting it as the
gate's go-around; the 160-corner tests also asserting stabilized; and a
new test building the go-around explicitly with 25 kt of wind shear at
400 ft. With the committed policy the corner tests went red as above
(seen to fail). Not merged: with no policy that meets the gate it would
send the learnt landing round from every corner. Kept on the branch
`learnt-landing-gate-wip` (below).

**Verification** (linux-release, DISPLAY and WAYLAND_DISPLAY unset): nothing
under src/ or tests/ changes. The selftest hash does not move
(`182dd6c996e0ee4c`): it flies a pilot's input log and no approach, so a new
policy would not move it either. tools/rl is not built or tested by ctest;
env.py was run by the three trainings above and by the scratch evaluation.

**A second round, the same day: still no policy, and why.** Starting again
from the committed checkpoint (step27000004), as the first round's next step
said:
- **The speed costed down to where the simulation's flare begins**, 20 ft over
  the runway (sim::Lander flares the C172P at a quarter of a foot a knot, 15
  ft, with her wheels about 5 ft below where her height is measured from), not
  the gate's 50 ft.
- **Outside the gate's own +10/-5 kt costs 2.0 a decision.** The touch's sink
  term is 40 exp(-(sink/250)^2) - sink/50: 40 at no sink, 31 at the slow
  flare's 106 ft/min, 3.5 at 300 - so at most 36.5 between the softest touch
  and the limit. The gate's two seconds are twenty decisions, which at 2.0
  cost 40.
- **Going wrong is no way out of it.** In the first try of this round (0.05 a
  knot, learning rate 1e-4), a policy 25 kt fast was charged some 2.6 a
  decision for 700 decisions, far more than the 70 for going wrong in the air.
  Its landings within the limits fell from 399 of 400 to 179 in 1.2 million
  decisions. Going wrong now costs as well the most the speed could still
  have cost from there (`speed_cost_left`, at 300 ft/min), and the knot is
  back to 0.02.

Every one of these runs came apart within a million decisions. The
training's landings within the limits fell from 399 of 400 to 91 (learning
rate 3e-5) and to 291 (1e-4). The checkpoints flew faster, not slower: 37 to
52 kt fast at the corners. **The likely cause, not proven:** costing the
speed down to 20 ft charges the policy's own long flare, which bleeds the
speed from 100 ft. The nearest way out it finds is to come down faster, so
that it is still on speed low down. Learning a later flare, its whole
touchdown, is a much larger change than a fine-tune at these rates makes
before its landings fail.

**Every checkpoint, at the gate's 160 corners** (tools/rl/corners.py, now
committed: "unstabilized" is the gate's two seconds running outside +10/-5
kt between 500 and 50 ft; "outside limits" is evaluate.py's landing limits
of 300 ft/min, 5 m and on the runway):

| run | reward | checkpoint | unstabilized | outside limits | most fast / slow kt | worst sink, across |
| --- | --- | --- | --- | --- | --- | --- |
| committed | potential only | 27.0M | 160 | 0 | 35.3 / 9.5 | 190, 2.14 m |
| 1 (lr 1e-4) | 0.02/kt to 50 ft | 28M | 156 | 28 | 46.9 / 13.7 | 178, 5.87 m |
| 1 | | 29M | 94 | 3 | 18.1 / 12.7 | 197, 6.27 m |
| 1 | | **30M** | **86** | **0** | 15.6 / 10.0 | 177, 4.73 m |
| 1 | | 31M | 52 | 91 | 17.7 / 8.2 | 179, 11.78 m |
| 2 (from 30M, 3e-5) | same | 31M | 160 | 110 | 15.6 / 12.6 | 192, 12.18 m |
| 3 (from 30M, 3e-5) | 0.05/kt + 0.3 outside the gate, to 50 ft | 31M | 160 | 120 | 15.6 / 13.0 | 143, 9.68 m |
| 4 (1e-4) | 0.05/kt + 2.0, to 20 ft | 27.5M | 160 | 43 | 40.6 / 17.2 | 369, 8.63 m |
| 4 | | 28M | 156 | 111 | 52.3 / 20.4 | 850, 28.52 m |
| 5 (1e-4) | 0.02/kt + 2.0 to 20 ft, going wrong charged | 27.5M | 160 | 29 | 49.3 / 16.7 | 288, 5.91 m |
| 6 (3e-5) | same as 5 | 27.5M | 160 | 94 | 24.6 / 17.5 | 206, 5.89 m |
| 6 | | 28M | 154 | 159 | 37.2 / 21.4 | 948, 27.75 m |

All runs used seed 160 and four environments, niced, with the noise held
(`--log-std -2.5257`). Runs 4 to 6 saved a checkpoint every half million
decisions (`--every`) and were stopped once they had come apart. **Best on
both counts: run 1's 30M**, with 0 outside the limits and 86 of 160 sent
round - not good enough to commit. tools/rl/env.py is left with run 5 and
6's reward, which nothing committed was trained with.

**What remains for the item:**
- A policy that holds its speed to the flare and flares late. That probably
  means training the flare as its own stage, or starting from the approach
  autopilot's flare (sim::Lander's, which already lands every corner
  stabilized), rather than fine-tuning this policy into it.
- The gate's C++ judging is kept on the pushed branch
  `learnt-landing-gate-wip` (c22e42d7, not for merging): LearntLander judged
  at its policy's speed, the controller going around for it, the corner tests
  asserting it, and the wind-shear go-around test. It is to be rebased on
  once a policy meets the gate.

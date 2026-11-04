### The learnt landing's approach flown by the approach autopilot, stabilized, 2026-11-04 — item done

**What is still not true first.**
- **The policy no longer flies the approach.** A sim::Lander flies it, at the
  policy's own speed (59.8 kt) and flap, judged by the stabilized gate as
  any approach of the autopilot's is, down to 35 ft over the runway; the
  policy flies the round out and the touch from there. What is learnt is
  now the last 35 ft.
- **Nothing was trained.** The policy (part 13, 2026-11-03) was trained to
  fly from the gate, not from the autopilot's 35 ft. Its landings from there
  are measured, not trained for, and the height is **tuned** (below).
- **The gate's nearest edge moved from 1.6 to 1.7 miles.** Its nearest,
  lowest corner was 494 ft over the runway, under the gate's 500: the
  autopilot, climbing back to the glidepath from 20 m under it, was 5 kt
  slow there with the wind behind or ahead, or 14 kt fast quartering ahead
  from the right, and went round from 6 or 7 of the 32 corners in five of
  the nine winds. From 1.7 (527 ft) it went round from none.
- **Steady winds only.** In gusts the autopilot flies its half gust factor
  over the policy's speed; the policy was trained in none, and its flare
  from there in gusts is not measured.
- **tools/rl is unchanged**: train.py, evaluate.py and corners.py still fly
  the policy from the gate, as it was trained. Their figures are not the
  simulation's; the C++ tests' are.
- **No source found for the split itself.** Learnt flare controllers exist
  in the literature (a DDPG flare controller for a UAV's longitudinal
  channel, cited in arXiv 2302.10997; a 2020 DGLR paper has RL steer a
  classical autopilot to the flare position), but none hands a policy
  trained on the whole approach the last part of it. It is this project's
  choice, by the owner's route of 2026-10-10.

**Why.** Rewarded for the touch alone, the policy dives down the glidepath
on power (2026-10-10). Flown from the gate as before, the corner tests now
measure it: calm, from 500 to 50 ft at most 41.4 kt fast and 10.1 slow,
unstabilized up to 61.4 s running, every one of the 32 corners (the same
with 5 behind and 10 across: 41.2 fast, 12.7 slow). Fine-tuning a speed
cost into it came apart in six runs (2026-10-10).

**Route chosen: start from the approach autopilot, not train the flare.**
Measured first, within the hour, by flying the committed policy from the
autopilot's approach at handover heights of 19 (the autopilot's own
flare), 30, 35, 40, 45, 50, 60, 75 and 100 ft: calm, every height landed
all 32 corners. It cost a build, where training a flare stage costs hours
of runs with no promise (2026-10-10's). Training stays the route if a
height had not been found.

**The height, tuned: 35 ft** (`LearntLander::policy_takes_her_ft`). Worst
sink over the gate's corners in its nine winds, by height:

| handed over at | worst | where |
|---|---|---|
| 19 ft (the autopilot's flare) | 348 ft/min, 32 of 32 over 300 | 15 kt ahead |
| 30 ft | 290 | quartering ahead from the left |
| 35 ft | 245 | 5 behind, 14.1 across from the right |
| 40 ft | 271 | the same |
| 45 ft | 296 | the same |
| 50 ft (the gate's speed floor) | 315, 32 over 300 | the same |

60, 75 and 100 ft were tried in that wind only: 348, 351 and 312. **Held
out**: at 35 ft, a grid of 85 steady winds (every 2.5 kt from 5 behind to
15 ahead and 15 either side, within 15 in all), four corners each, at 25,
50, 75 and 100 lb a tank (full) - 1,360 landings, a scratch test not
committed - all stabilized, all inside the 5 per cent margin: worst 267
ft/min (25 lb a tank), 2.39 m across, every one 181 m or more past the
threshold.

**The speed it is flown at is the policy's, not the one for her weight**
(sim::for_weight): scaled for 1,880 lb the autopilot came down at 52.9 kt,
and the policy, handed her 7 kt under its 59.8, flew on past the runway
from seven corners. The rollout's lander keeps the speed for her weight,
as before.

**The C++** (from the branch `learnt-landing-gate-wip`, rebased by hand):
- LearntLander: an `Approach` - `autopilot` (the default, every session's)
  or `policy` (as trained, for the two tests that hold the simulation to
  training's figures). While the autopilot flies she is the autopilot's: it
  goes round for the gate itself, and `unstabilized()` says why. Once the
  policy has her - deciding at once, its last action the autopilot's
  controls, its drift remembered all along - the gate judges her at the
  policy's speed (below 50 ft only the touchdown zone), and
  `unstabilized()` asks the controller to go round. `most_unstable_s()`
  for the tests.
- Controller: the learnt landing judged; gone round from, to the gate's
  runway or, handed it directly, its own; counted as the gate's go-around.

**Tests.** The nine winds' corner tests are renamed
`the_learnt_policy_is_stabilized_by_500_ft_and_lands_within_its_limits_from_every_corner_of_its_gate_*`
and, with the four inside-the-quarter tests, now assert each corner never
two seconds running unstabilized and not sent round, and print the speed
between 500 and 50 ft. c08 at 10 and 12.5 kt across with 5 behind is no
longer named (150 and 157 ft/min; the two tests are renamed from
`..._every_corner_but_one_named_...` to `..._every_corner_of_its_gate_...`).
The fast corner's test is now
`the_learnt_landing_at_a_fast_corner_of_its_gate_is_stabilized_by_500_ft_and_not_sent_round`,
and a new one, `a_learnt_landing_not_stabilized_below_500_ft_is_sent_round_by_the_gate`,
builds the go-around with 25 kt of wind shear at 400 ft ("20 kt fast at
406 ft"). Held-out start 31 (1.65 miles) is outside the gate now: refused
there, flown as trained, named. ci_costs carry the renamed tests' figures.

**Seen red**: with the approach left to the policy, the calm corners fail
(32 of 32, 61.4 s unstabilized) and so do the fast corner's and the quarter's
tests; with the controller not asking `unstabilized()`, the shear test fails
("NOT GONE AROUND").

**Figures** (linux-release, the gate at 1.7 miles, 32 corners a wind): never
unstabilized for a single step at any corner in any wind; between 500 and
50 ft at most 7.6 kt fast (15 kt ahead) and 4.1 kt slow (the gate's -5 is
0.9 kt away; with the wind ahead or across).

| wind | worst sink, across | most fast / slow, kt |
|---|---|---|
| calm | 165, 2.06 | 2.7 / 3.2 |
| 15 across from the left / right | 213, 1.14 / 233, 2.02 | 3.6 / 4.1 |
| 15 ahead | 115, 1.32 | 7.6 / 4.1 |
| 5 behind | 146, 1.96 | 1.7 / 3.3 |
| quartering ahead, left / right | 220, 0.59 / 116, 2.36 | 6.6 / 4.1 |
| 5 behind, 14.1 across, left / right | 162, 1.76 / 244, 1.93 | 1.8 / 3.3 |
| 5 behind, 5 / 7.5 / 10 / 12.5 across from the left | 138 / 143 / 150 / 157 | 1.7 / 3.3 |

Before (the policy flying from the gate, 2026-11-03): worst 281 ft/min, and
every corner unstabilized.

**Verification.** linux-release, the full suite (`-LE nightly`, `ctest -j4`,
no display): 1483 of 1483 passed (some skipped: no key, device or
service). The selftest flies no approach and its hash is not moved.

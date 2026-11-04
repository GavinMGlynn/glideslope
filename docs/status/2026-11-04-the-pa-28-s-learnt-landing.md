### The PA-28's learnt landing, 2026-11-04 - part of an item, not done

Phase 10b, "Learnt landings for other aircraft": the PA-28's part. The item
stays open.

**What is still not true first.**
- **The PA-28 has no learnt landing.** After two rounds of training (about
  five hours) from the C182's policy, no checkpoint passed her
  verification, so none is committed. What is left, from the best (step
  57,250,000, below):
  - **with full tanks, the policy flying the whole approach, she banks 21
    degrees and pitches 14 nose down after the touch**, from the nine calm
    starts (she touches at 80 ft/min, flat, and wheelbarrows);
  - **handed over at 35 ft in 10 kt across from the left she sinks 335
    ft/min**, from all nine of those starts;
  - from the gate's corners she sinks up to 372 ft/min with the wind
    across from the left, ahead, or quartering ahead from the left.
- **The J-3 Cub has none either.**

**What now works.**
- **tools/rl/aircraft/pa28.txt**: her model, her reference speed - 64.35 kt,
  sim::approach_speeds' 1.3 times her published 49.5 kt stall with 40
  degrees of flap (57 mph CAS, Airplane Flight Manual, Model PA-28-180;
  section I of the Cherokee 180 "E" Owner's Handbook gives the same 57 mph)
  - and her fuel, 150 lb a tank as her model starts (full, 25 US gallons).
  JSBSim trims her from every one of the verification's 27 starts, so she
  is trimmed as the C172P is, not flown to her trim as the C182 is.
  Her weights trained at come to 1,565 to 1,790 lb.
- **train.py `--nose-up W`**: at the touch, W lost for each degree her nose
  is below level, to 15. Off by default, so the C172P's and C182's training
  is unchanged: `export.py --parity-only --aircraft c172p` writes a fixture
  byte-identical to the committed one.
- **The server test's aircraft with no learnt landing is now the J-3 Cub**:
  `a_players_c172p_handed_to_the_learnt_landing_on_final_is_landed_by_the_server_and_a_j3_cub_and_one_outside_the_gate_are_refused`,
  passed (linux-release, 164 s).
- The C182's 27-start helper in tests/unit/test_learnt.cpp names the
  aircraft it flies, for the PA-28's tests to come.

**What training found** (each run resumed from the one before, 6 envs; each
checkpoint flown on the simulation's fourteen PA-28 tests written for her -
the 27 starts full, at 37.5, 75 and 112.5 lb a tank, the approach autopilot
flying to 35 ft, and the gate's 32 corners in its nine winds):
- **The C182's policy flies her 12 to 18 m right of the centreline**, 0 of 27
  in every one of those tests (seen red).
- **6 million decisions at 1e-4, three in ten flights from the hand-over**,
  took her onto the centreline (median 1.3 m), but she touched at 8 degrees
  nose down and 91 kt, flying the whole approach. The flight ends at the
  touch in training, and in the simulation's roll-out she then pitched to
  15 degrees nose down and banked 20: 0 of 27.
- **`--nose-up 4`, then 8 with six in ten from the hand-over at 5e-5**:
  nose up at the touch, and the four whole-approach tests pass 27 of 27
  (step 53,750,000: worst 283 ft/min, 2.4 m). From the hand-over, Python
  agrees with the simulation that she sinks too hard: 321 to 529 ft/min from
  30 to 40 ft in 10 kt across, where calm air gives 63 to 148. Still to
  do: more of training's hand-over flights in crosswinds, or her approach
  autopilot's state at the hand-over reproduced in training.

**The cause found in round two: training's hand-over was not the
simulation's.** JSBSim's initial conditions take no wind - the ground
velocity is the air's - so a training start at 35 ft in 10 kt across began
slipping 9 degrees with the wind arriving after the trim, and its policy's
last action was nothing, where sim::LearntLander hands her over crabbed into
the wind, unslipped, with the approach autopilot's controls (0.31 elevator,
0.21 aileron on her). Flown from a start made like the simulation's, Python
reproduced the simulation's 465 ft/min from the left; from the old start it
had landed her at 45. **Now landed in tools/rl**:
- `--handover-across F`: of the hand-over's flights, this fraction in strong
  wind (env.py's `strong_wind`: half 8 to 15 kt across either way, half 12 to
  15 kt from anywhere the gate allows), down to 6 kt slow;
- with it, **every hand-over start begins as the simulation hands her over**
  (landing.py's `into_the_wind`): trimmed in still air on the crabbed heading,
  started again with the wind added to her ground velocity, one step run, and
  her trimmed controls - the pitch trim's included - as the last action;
  the start after one gets a fresh JSBSim, whose initial conditions otherwise
  carried the ground velocity into the next start and tumbled her.
Off by default. `export.py --parity-only --aircraft c172p` writes a fixture
byte-identical to tests/data/rl/c172p-landing-parity.txt, and the C182's
committed policy flown by evaluate.py in the bindings is unmoved: 27 of 27,
worst 254 ft/min and 0.78 m, as her own file records.

**Her best figures**, step 57,250,000 (round two: resumed at 5e-5 with
`--handover 0.6 --handover-across 0.6 --nose-up 8`), in the simulation's
fourteen tests written for her, 9 of 14 passing:

| test | landed within the limits | worst sink | worst across |
|---|---|---|---|
| full (150 lb a tank), policy all the way | 18 of 27 | 96 ft/min | 0.89 m |
| full, approach autopilot to 35 ft | 18 of 27 | 335 | 0.93 |
| 37.5 lb a tank | 27 of 27 | 218 | 1.87 |
| 75 lb a tank | 27 of 27 | 95 | 1.46 |
| 112.5 lb a tank | 27 of 27 | 93 | 1.30 |

Corners pass in calm air, across from the right, behind, quartering ahead
from the right, and behind and across from either side; they fail across
from the left (337 ft/min), ahead (336) and quartering ahead from the left
(372).

The best checkpoint, its run's logs and the PA-28's tests as written are kept
outside the repository (the session's scratchpad, learnt-landing-pa28/).

**Verification** (linux-release, no display): the server test above, and
every C182 and C172P learnt-landing test it shares a file with, 20 of 20
passed. The C172P's parity fixture is unchanged.

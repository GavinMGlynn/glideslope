### The PA-28's learnt landing, 2026-11-04 - part of an item, not done

Phase 10b, "Learnt landings for other aircraft": the PA-28's part. The item
stays open.

**What is still not true first.**
- **The PA-28 has no learnt landing.** Trained for three hours from the
  C182's policy, no checkpoint passed her verification, so none is
  committed. The best (below) lands her from all 27 starts at every fuel
  load with the policy flying the whole approach, but **handed over by the
  approach autopilot at 35 ft in 10 kt across from the left she sinks about
  465 ft/min** - 9 of the 27 starts fail - and from the gate's corners she
  sinks up to 480 ft/min in the winds from the left and from ahead.
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

The best checkpoint, its run's logs and the PA-28's tests as written are kept
outside the repository (the session's scratchpad, learnt-landing-pa28/).

**Verification** (linux-release, no display): the server test above, and
every C182 and C172P learnt-landing test it shares a file with, 20 of 20
passed. The C172P's parity fixture is unchanged.

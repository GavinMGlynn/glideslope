### The AI flies her approach at the speed for what she weighs, on a server too: the reference speed scaled by the square root of the weight, 2026-10-10 — item done

**Why.** The server loads no loading, so every aircraft it flies is at her
model's own weight, and the approach was flown at `vref_kts` unscaled - a
speed her figures give for one loading. The B-2A's model weighs 327,000 lb
and her 124 kt is for 177,160: on a server she went around from it (below),
and in the trial of #157 deep-stalled. The take-off already scaled its
rotation (`DepartureSpeeds::reference_lbs`).

**What.**
- `ApproachSpeeds::reference_lbs`: the weight `vref_kts` and `stall_kts` are
  for - the loading of the manual's `<approach>` speed where it gives one
  (the F-15C), of the landing stall otherwise, the file's first where the
  figure names none. `ApproachSpeeds::loading` now names the approach
  speed's loading too where that is the source (it was the stall's; the
  same loading for the one aircraft that has both).
- `sim::for_weight(speeds, lbs)`: `vref_kts` and `stall_kts` times
  sqrt(lbs / reference_lbs), and `reference_lbs` made `lbs`, so scaling
  twice is once. A stall speed goes as the square root of the weight (lift
  at the stall is the weight), and so does a speed taken as a margin over it:
  the FAA's Airplane Flying Handbook (FAA-H-8083-3C, ch. 5, weight and the
  stall speed), and a transport's Vref tabled against landing weight. Within
  1 lb of the reference - the tolerance a figures file's loading is held to
  of its stated weight - it is the speeds as given, so an aeroplane at her
  figures' loading flies exactly what she did. Speeds made by hand
  (`reference_lbs` 0) are flown as given. The flare height, flap and
  touchdown sink are not scaled (the flare is already the higher of a
  quarter-foot a knot and the height to arrest the scaled speed's sink).
- Applied wherever an approach speed is flown: the `Lander`'s constructor
  (the approach, the stabilized gate's +10/-5 kt round it, the flare, the
  roll-out, a take-back on the roll), the `GoAroundCircuit`'s (its height,
  turn and speeds; already scaled from a lander), the plan's final legs at
  ten knots over it, and where a start on final is set from it - the CLI's
  `land` and the server's players and AI on final, by
  `Aircraft::loaded_weight_lbs()` (empty, on board and fuel, known before
  `initialize`, where `inertia/weight-lbs` is not).
- **The learnt landing is not scaled.** The C172P's policy was trained at
  59.8 kt from 1,730 lb to her model's 1,880 (`trained_lbs`) and its
  gate admits her within -3/+8 kt of that. Scaled for 1,880 she would come
  down at 52.9 and never meet it - which is what first broke the players'
  and client's learnt-landing tests here. So `to_ai_approach` with a policy
  flies the approach autopilot to the gate at the policy's own speed, and a
  server starts one on final at it; gone around from the learnt landing, or
  landed past its gate, she is flown at the speed for her weight. Its gate
  corners pass unchanged (test_learnt, below).
- The server says, for an AI put on final: "aircraft N, an AI's MODEL at W
  lb, is flown down final at K kt; her figures give F kt for R lb".
- The CLI's crosswind sweep starts at the figures' own speed, unchanged:
  it is #157's measurement at the approach speed's weight. The brief is
  changed (review, below).

**Figures** (linux-release; the speed flown, and the least and most over it
from 500 ft to 50, at each model's own weight, by the AI through a
`Controller` given the published speeds as a server gives them): 737-300
96,540 lb, 137.4 -> 126.5 kt, -0.5/+1.4; 787-8 420,027, 147.8 -> 155.4,
-0.2/+0.3; A320 132,823, 147.3 -> 142.4, 0/+0.1; A380 1,188,253, 136.3 ->
159.4, -0.1/+0.5; B-2A 327,000, 124.0 -> 168.5, 0/+0.1; C172P 1,880, 59.8 ->
52.9, -0.5/+0.3; C182 2,505, 64.4 -> 57.8; F-15C 33,706, 160.0 -> 152.8;
F-35B 45,400, 158.9 -> 169.8 (most alpha 21.3); J-3 Cub 752, 42.9 -> 35.6;
Learjet 35A 16,411, 125.5 -> 129.9; Mosquito 19,800, 122.9 -> 128.8; PA-28
1,790, 64.4 -> 55.6; S.23 34,717, 86.2 -> 79.8. All fourteen touched
inside the touchdown zone at 54 to 404 ft/min and stopped. At their
figures' loadings every speed is the published one, held within -1.0/+0.5.

**Tests changed, and why.**
- `fly_the_approach` (the lessons' light/heavy variants) scaled the
  reference speed for a variant's loading itself: it now says so
  (`reference_lbs` the variant's weight), or the lander would scale it
  twice.
- `the_speed_the_autopilot_raises_for_a_climb_never_passes_the_fastest_she_may_hold`:
  the F-35B burns fuel down her approach, so her circuit is flown at 20 kt
  over the speed for what she weighs at the go-around, not her figures'
  loading's; the fastest is set 11 kt over that, when the circuit begins.
  Red until then: the raise reached 11.25 kt against a fastest 0.25 kt
  nearer than it was set from.

**Verification** (linux-release, locally):
- `the_ai_flies_every_approach_at_the_speed_for_its_models_own_weight_and_lands_within_its_limits`
  and `the_ai_flies_every_approach_at_the_speed_for_its_figures_loading_and_lands_within_its_limits`
  (new): every aeroplane with an approach speed (14; the 747-400 and F-22A
  publish none, named), at her model's weight and at her figures' loading,
  handed to the AI on final with the published speeds; each must be given
  the scaled speed (to 0.1 kt), hold it within +10/-5 kt from 500 ft to 50,
  never go around, touch inside the touchdown zone, unwrecked, upright, and
  stop. Coverage asserted: 14 of 14 each; 14 of 14 at a weight other than
  her speed's in the first, none in the second. **Seen to fail** with the
  lander unscaled: 23 wrongs at the models' weights - every one given her
  figures' speed; the B-2A 9 to 11 kt under the scaled speed and gone around
  for the gate, never touching; the 737-300 wrecked at 632 ft/min; the A380
  13 to 25 kt slow, the F-35B and 787-8 7 to 12.
- `the_servers_ai_{b2,a380,pa28}_on_final_at_its_models_own_weight_is_landed_at_the_approach_speed_for_it`
  (new, tests/cmake/server_ai_lands_at_its_model_weight.cmake): the server
  puts each on final to YSSY 16R (plans in tests/data/plans/*-on-final.plan);
  it must say she is flown down at the unit test's speed for her weight
  within 0.1 kt (168.5, 159.4 and 55.6 kt), never go around or be wrecked,
  and leave the runway. Heavy: the B-2A and A380; light: the PA-28, 610 lb
  under her figures'. **Seen to fail** unscaled: all three at their figures'
  speeds (124.0, 136.3, 64.4), and the B-2A went around.
- Every test in test_lesson, test_lander, test_learnt (the learnt gate's
  corners among them), test_controller, test_departure and test_navigator,
  and every test matching land, circuit, go-around, learnt, gust, approach,
  lander, final, crosswind or stabil - 325 with the new ones, the
  server's and client's landing tests among them, run again on the base
  restacked (#157 at eddac6f1): green but for
  `a_plan_file_asking_a_speed_its_aircraft_cannot_hold_clean_is_refused_and_none_in_the_data_does`
  - red on its base too, which this does not touch: it expects the
  Mosquito's fastest plan speed to be 219 kt and her figures now give 264 -
  and one skipped for want of a key (the Anthropic planner's). The
  players' and the client's learnt-landing tests, which a start on final at
  the scaled speed broke (above), are green.
- **The selftest hash does not move** (`182dd6c996e0ee4c`): it flies a
  pilot's input log and no approach.

**From the review of #158** (same day):
- **The learnt landing is bounded to the weights it was trained at**:
  `trained_lbs 1730.0 1880.0` in assets/rl/c172p-landing.txt - her model
  with 25 to 100 lb a tank, tools/rl/landing.py's `TRAINING_FUEL_LBS` (the
  entry above said 1,680, her model with none; she was never trained
  there). `LearntPolicy::read` refuses a file without it, reversed, or with
  one number; tools/rl/policy_file.py reads and writes it and export.py
  works it out from the model (the committed file round-trips through
  policy_file.py byte for byte). `outside_learnt_gate` refuses her outside
  it, "1882 lb; the learnt landing was trained at 1730 to 1880"; handed to
  the AI with the policy at such a weight, `to_ai_approach` flies the
  approach autopilot at the speed for her weight from the start, and the
  server starts her on final at it.
- **The brief tells the speed the AI flies**, for the loading she will
  have - her model's own, which a server flies: `sim::approach_kts_flown`,
  the policy's speed where she has a learnt landing trained at that weight,
  the speed for her weight otherwise. Briefed, rounded: 737-300 126 (was
  137), 787-8 155 (148), A320 142 (147), A380 159 (136), B-2A 169 (124),
  C172P 60 (60), C182 58 (64), F-15C 153 (160), F-35B 170 (159), J-3 Cub 36
  (43), Learjet 35A 130 (125), Mosquito 129 (123), PA-28 56 (64), S.23 80
  (86). The CLI's brief, the copilot's and the server's planners all use
  `frontend::brief_for`. Every recorded planner exchange is for the C172P,
  briefed 60 kt as before, so no prompt changed and nothing was
  re-recorded.
- `for_weight`'s header says the weight is a snapshot, taken when a lander
  or circuit begins; the server's two `final_approach_start` calls are
  clang-formatted and the long comment line wrapped.

**Verification of the review's fixes** (linux-release, locally):
- `the_learnt_landing_is_refused_a_c172p_heavier_or_lighter_than_it_was_trained_at`
  (new): at its gate with 25 and 100 lb a tank (1,730 and 1,880 lb) inside;
  with 24 and 101 (1,728 and 1,882) refused for her weight, flown by the
  approach autopilot at the speed for it, and never handed to the learnt
  landing in a minute. **Seen to fail** with the check taken out: 1,728 lb
  at the gate.
- `a_policy_file_that_does_not_fit_the_simulation_is_refused`: three more
  wrong files, 21 in all, and the committed policy's range read.
- `every_aircrafts_brief_tells_the_approach_speed_the_ai_flies_her_at_for_her_models_weight`
  (new): every aircraft (16), each of the 14 with an approach speed put on
  final at her model's weight and handed to the AI as a server hands her;
  her brief must be her approach autopilot's speed to the knot, and the
  747-400 and F-22A briefed none. **Seen to fail** briefing the figures'
  speed: the 737-300 briefed 137 and flown at 126.5.
- `an_aeroplane_landing_on_a_runway_another_is_on_goes_around_and_lands_once_it_has_left`
  went red with the bound: its second C172P, handed to the learnt
  landing, was put on final at her figures' 2,400 lb, which the policy was
  never trained at. She is now put there at her model's own weight, as a
  server's is (`put_on_final`'s `model_weight`), and is handed to it.
- The run before, plus every copilot, planner, recording, route and
  catalogue test - 406, with
  `every_aircraft_the_data_holds_that_alights_on_water_does_not_roll_out_on_it`
  (the S.23's alighting, not in the run before): green but for that one
  (fixed, above, and green with every test matching learnt, 31) and two
  recorded-planner server tests that could not listen on their ports while
  another working copy's tests held them, green run again; the tests that
  ask a provider live skipped without keys. Every recording plays back
  unchanged.
- The selftest hash does not move (`182dd6c996e0ee4c`).

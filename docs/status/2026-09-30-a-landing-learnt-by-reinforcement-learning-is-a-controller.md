### A landing learnt by reinforcement learning is a controller, lands on the runway and is stopped on it; not yet on the centreline in a crosswind, and trained with its fuel running out, 2026-09-30 — item done the same day, above

**What is not done first.**
- **The centreline, which is the item's verification.** From the 27
  verification starts the committed policy touches down 10.4 to 18.3 m right
  of the centreline in a ten-knot crosswind from the left, 13.0 to 19.3 m left
  of it in one from the right, and in calm air within 5 m at eight starts of
  nine (5.84 m at the ninth): **8 of 27 within the autopilot's 5 m.**
- **Every policy so far was trained with its fuel running out.** Each
  training environment keeps one JSBSim instance, and JSBSim 1.3.1's RunIC
  does not start the propulsion again (`ext/jsbsim/src/FGFDMExec.cpp`, 634),
  so every episode began with the fuel the last one left: about 22 lb an
  episode-hour from two 100 lb tanks, dry after about nine simulated hours -
  two and a half to three million decisions with eight environments - and an
  engine-out glide, 200 lb lighter, every episode after. **The committed
  policy was trained mostly on part-full tanks and at the end of each run on
  empty ones**, and is flown - by the tests, the fixture and the simulation -
  with full tanks. This was the "gap between training and evaluation" the
  last round could not explain, and the fine-tunes' collapse to about 550
  ft/min at about three million decisions: that is the glide. It was found
  in review of #66. **Fixed for training from here** (below); the committed
  policy is not retrained in this round.
- **The committed policy's centreline depends on its fuel**, which is the
  mark of it: the 27 starts in Python, a fresh aeroplane each, with each tank
  at 25, 50, 75 and 100 lb give 0, 9, 1 and 8 of 27 within the limits (worst
  37.5, 31.4, 25.2 and 19.3 m across). One start - ten knots from the left,
  on the centreline and the glidepath - touches +15.2 m off with full tanks
  and +0.7 m with 50 lb a tank.
- **The committed policy does not use the observations built for the
  crosswind.** The observation grew by four - the drift angle, the wind
  across and along the runway as the instruments estimate it, and a
  remembered drift - and the simulation computes all four, but the committed
  network is the previous one carried over with them weighted nothing
  (`tools/rl/warm_start.py`).
- **Its choice was not blind of the verification.** The carried-over
  policy was chosen among checkpoints by flying the verification's 27
  starts. Since then checkpoints are chosen on forty held-out starts, and it
  was chosen again there (16 of 40) - but that scoring reused one aeroplane,
  its fuel draining down the list. **Scored again with a fresh aeroplane for
  each start** it is 17 of 40, and the best fine-tune checkpoint (the noise
  held small, thirteen million decisions) 22 of 40 (worst 25.8 m, 224
  ft/min). It is not swapped in: it too was trained on draining tanks, and
  the next round retrains.
- **The committed scripts cannot make the committed file again**: its reward,
  inputs and fuel were not what `tools/rl` now has. Its header says how it
  was made and chosen, and gives the SHA-256 of the checkpoint it was exported
  from (kept outside the repository); exporting that checkpoint is
  deterministic and gives the file byte for byte - done again in this round,
  with only the header changing.
- **No client or server offers it.** The CLI does, since the next
  entry's round: `glideslope_cli land AIRCRAFT [--learnt] [--crosswind KTS]
  [--across M] [--high M] [--fuel LBS]` hands the aircraft to the AI at the
  two-mile gate - the approach autopilot, or with `--learnt` the learnt
  landing, through `Controller` as any hand-over - over level ground at sea
  level, and says where it touched and stopped, and how many times the
  policy decided. `the_cli_hands_the_c172p_at_the_gate_to_the_learnt_landing_and_it_is_stopped_on_the_runway`
  runs it in a ten-knot crosswind and requires the policy's decisions in
  what it prints; seen to fail with the CLI handing to the approach
  autopilot instead. `the_cli_refuses_a_learnt_landing_for_an_aircraft_that_has_none`
  holds that the J-3 Cub, which has none, is refused.
- **One aircraft, one runway geometry, steady winds**: the C172P, at sea
  level, no turbulence, gusts or shear.

**The fuel, fixed for training.** `landing.py` fills each tank to the
start's `fuel_lbs` before every RunIC - full, 100 lb a tank, for evaluation,
export and the fixture, as the simulation's aircraft starts; drawn from 25 to
100 lb per episode for training - and `Flier.begin` refuses a start whose
aeroplane does not then weigh its dry weight plus that fuel. Seen to fail:
without the refilling, the second flight is refused ("she weighs 1878.94 lb,
and the start says 1880.0"). `evaluate.py` flies each start in a fresh
aeroplane, as `export.py` does, so that no score depends on the order of the
list, and takes `--fuel`.

**Also for the next round.** `warm_start.py` measures the new inputs'
normalisation from fifty flights of the old policy (71,900 decisions)
instead of a placeholder mean of 0 and variance of 1 that the shared count
of ten million would have held for as long again. `train.py`'s log reports
the last 400 episodes, flown by older weights than the checkpoint it saves;
it is a trend, not a score.

**A controller.** `Controller::to_ai_learnt_approach(runway, speeds, policy)`
hands an aeroplane to the learnt landing as `to_ai_approach` hands it to the
approach autopilot: the policy flies from where she is to the touch, the
controls reached from the ones she had at a hand's pace, full travel in a
second. While they are on their way the policy's `previous` input is its own
last action, not the controls she has - which it never met in training.
Handed back to the pilot it is dropped.

**The rollout is the approach autopilot's.** `sim::LearntLander` keeps a
`sim::Lander` that watches the whole landing and is given her at the touch;
it brakes her to a stop on the centreline. **At the touch, not later**: the
policy was trained to five seconds past it, and in those seconds it let her
swing from 057 to 040 on a runway of 070 and every landing stopped 30 to 67
m off the centreline. Given her at the touch the autopilot stops every one
within 3.4 m of it.

**The policy file is checked as it is read**: a NaN or an infinity anywhere
(`std::stod` reads both as numbers), a count that is not a whole number or
is out of range - a fractional decision interval, a negative or enormous
layer - and anything cut short are refused, and `policy_file.py` will not
write a number that is not finite.

**The verification's starts, flown by the simulation with the committed
policy, full tanks:**

| | calm | 10 kt from the left | 10 kt from the right |
| --- | --- | --- | --- |
| touchdown sink, ft/min | 120 - 134 | 87 - 99 | 141 - 147 |
| across the centreline at the touch, m | -5.84 to +1.59 | +10.36 to +18.28 | -19.30 to -13.02 |
| along from the threshold at the touch, m | 209 - 320 | 69 - 145 | 536 - 615 |
| stopped: along, m | 413 - 525 | 262 - 339 | 729 - 804 |
| stopped: across, m | -2.84 to -2.00 | -3.41 to -3.07 | -0.93 to +0.47 |

The starts: a gate two nautical miles out, on the centreline and 50 m either
side of it, on the glidepath and 15 m above and below it, in calm air and a
ten-knot crosswind from either side - 3 x 3 x 3, at the reference speed,
trimmed, pointing down the runway.

**Verification run.** Six tests, all passing:
- `the_learnt_landing_sees_and_acts_in_the_simulation_as_it_did_in_training`:
  192 recorded decisions, 46 on the ground; observation and action agree
  with training's to 3.8e-15 and 9.2e-16; the drift memory and the
  action-to-controls mapping checked by value.
- `the_learnt_policy_touches_down_in_the_simulation_where_it_did_in_training`:
  all 27, within 0.28 ft/min, 0.019 m across and 0.37 m along of Python's.
- `the_learnt_policy_touches_the_c172p_down_on_the_runway_under_300_ft_a_minute_and_it_is_stopped_on_it_from_every_start`:
  all 27, counted, nine in each wind; on the runway under 300 ft/min, down
  and upright for five seconds, stopped within 15 m of the centreline. **The
  5 m is printed, not asserted**, because it does not hold.
- `an_aeroplane_handed_to_the_learnt_landing_at_the_gate_is_landed_and_stopped_with_no_step_in_its_controls`:
  no control moves more than 1/120 in a step at the switch (0.00833).
- `the_wind_the_learnt_landing_estimates_is_the_steady_wind_blowing`: calm,
  ten knots across either way and five down the runway either way, each
  estimated within 0.3 m/s (worst 0.18) after ten seconds flown.
- `a_policy_file_that_does_not_fit_the_simulation_is_refused`: eighteen
  wrong files, counted, in a directory of its own removed however it ends.

**Seen to fail**, each reverted: the finite check taken out ("a policy with
a NaN weight is refused"); the whole-number check taken out ("a fractional
decision interval is refused"); the wind across with its sign flipped (-5.15
for +5.14); the hand-over without the hand's pace (1.0018 in one step); the
drift memory's sign reversed; the rollout never handed over (27 of 27 "did
not stop"); and in the first round a sign flipped in the observation, a
weight moved in the policy file and the ailerons reversed.

**The first round, the same day**, before any of this: a gym-style
environment of our own around JSBSim's Python bindings on the committed
C172P at 120 Hz, PPO from stable-baselines3 (every package pinned in
`tools/rl/requirements.txt`, jsbsim 1.3.1 as `ext/jsbsim`, in a virtual
environment outside the repository), the policy exported as an MLP's weights
and normalisation and evaluated in plain C++. Four things were learnt: a
reward paid every tenth of a second is farmed (the agent flew over the
runway; a potential cannot be farmed); a reward that saturates says nothing
past it (landings settled 60 m off); short of the threshold must cost more
than anything on the runway (at a flat 20, one run learnt to land 800 m
short); and PPO's own process, not JSBSim, is the bottleneck (6,400
decisions a second on a core; about 5,000 in training).

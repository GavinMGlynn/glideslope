### A learnt landing holds the centreline in a crosswind, a quarter to full tanks, and the CLI flies it, 2026-09-30 — item done

**What it is not, first.** One aircraft - the Cessna 172P. One runway
geometry - at sea level, over level ground - and steady winds only: no
turbulence, gusts or shear. **No client or server offers it**: the CLI does
(`glideslope_cli land c172p --learnt`), and `Controller` can be handed an
aircraft for it, but no session hands one over. **The new observations are
not what holds the centreline**: with the estimated wind across or the
remembered drift blinded in the simulation, all 27 verification starts
still land within 5 m (worst 2.72 and 2.94 m); what changed was the
training (below). **Training is not reproducible**, and the committed
policy's lineage includes parts trained with the fuel bug; its header says
so, part by part.

**The item's verification is met.** From all 27 starts - a gate two miles
out, on the centreline and 50 m either side, on the glidepath and 15 m
above and below, in calm air and a ten-knot crosswind from either side - the
simulation's C++ lands the C172P within 5 m of the centreline and under 300
ft/min, on the runway, down and upright, and the approach autopilot's
rollout stops her on it. With full tanks (the verification test):

| | calm | 10 kt from the left | 10 kt from the right |
| --- | --- | --- | --- |
| touchdown sink, ft/min | 204 - 216 | 206 - 217 | 242 - 247 |
| across the centreline at the touch, m | +1.24 to +1.74 | -0.03 to +0.26 | +2.64 to +2.73 |
| along from the threshold at the touch, m | 182 - 230 | 177 - 234 | 197 - 228 |
| stopped: along, m | 438 - 486 | 471 - 513 | 422 - 454 |
| stopped: across, m | -2.58 to -2.52 | -2.62 to -2.38 | +0.91 to +0.96 |

**With any fuel.** The same 27 at 25, 50, 75 and 100 lb a tank: 27 of 27
each, worst 4.38, 3.85, 3.29 and 2.73 m across and 183, 208, 230 and 247
ft/min (in Python, and at 25, 50 and 75 in the simulation too - a test of
one a load, 81 landings, worst 4.38 m and 230 ft/min). The sink grows with
the weight; 247 ft/min at full tanks is the least margin, 53 ft/min.

**Chosen on held-out starts only.** Forty starts drawn once with their own
seed - gates 1.6 to 2.4 miles out, up to 60 m across, 20 m off the glidepath
and 5 degrees off the heading, in steady winds of up to fifteen knots from
anywhere with no more than five behind - each flown in a fresh aeroplane, at
full tanks and a quarter; the candidate then at 50 and 75 too. The
committed checkpoint: 40, 40, 40 and 39 of 40 at 100, 75, 50 and 25 lb a
tank; the one miss 5.06 m off with a quarter of the fuel in 13.4 knots
across and 4 behind, past the verification's winds. At full tanks the
worst of the forty is 3.74 m across (13.8 knots across) and 291 ft/min
(13.4 knots across, 4 behind). The verification's 27 were flown only by the
export, for the fixture, and by the tests.

**What made the difference: looking further ahead.** The discount went
from 0.995 to 0.999, and each tenth of a second off the centreline inside
1,500 m of the threshold costs a hundredth a metre, to fifty. From the gate
the touch is some 1,100 to 1,300 decisions away; 0.995 to that power is
0.0015, so the touch's reward - the centreline's most of all - was invisible
for all but the last twenty seconds. The first million decisions at the
new discount took the held-out score at full tanks from 0 to 31 of 40.
Flights also end at the touch now, since the simulation hands the rollout
to the autopilot there.

**Training, this round** (tools/rl at 4f6fc88, `nice -n 10`, eight
environments, CPU): from the fine-tune that scored 22 of 40 last round
(13 million decisions), a learning rate of 1e-4 with the action noise held
at a standard deviation of 0.08, stopped a little past 18 million; then,
from its checkpoint at 17 million, 3e-5, stopped a little past 26 million.
Held-out at full tanks, each checkpoint by the decisions it had flown:

| run | 14M | 15M | 16M | 17M | 18M | 19M | 20M | 21M | 22M | 23M | 24M | 25M | 26M |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1e-4, from 13M | 0 | 31 | 22 | 37 | 12 | | | | | | | | |
| 3e-5, from 17M | | | | | 16 | 10 | 23 | 23 | 38 | **40** | 38 | 28 | 36 |

The committed policy is the 23-million checkpoint, from the second run. The scores swing from one checkpoint
to the next; choosing on the held-out starts at several fuel loads is what
keeps that honest. Two other runs this round, at 0.995 with four
environments each (from that fine-tune, and from the 21-input policy
carried over with measured statistics), scored 0 and 2 of 40 at full tanks
after a million decisions and were stopped. Across the round the first
layer's weight on the four new inputs grew from about 0.17 to 0.24, 0.24,
0.49 and 0.33 against 1.6 for the old ones on average - used, but not
needed, as above.

**Offered outside the tests.** `glideslope_cli land AIRCRAFT [--learnt]
[--crosswind KTS] [--across M] [--high M] [--fuel LBS]` hands the aircraft
to the AI at the gate through `Controller` and says where it touched and
stopped and how many times the policy decided: in ten knots of crosswind,
214 ft/min, +0.09 m across, stopped 500 m along, 1,066 decisions.

**Verification run.** Nineteen tests, all passing in linux-debug (sanitized) and linux-release:
- `the_learnt_policy_lands_the_c172p_within_5_m_of_the_centreline_under_300_ft_a_minute_in_calm_air_and_a_ten_knot_crosswind`:
  the item's verification, 27 starts counted, nine in each wind, and each
  stopped on the runway.
- `the_learnt_policy_lands_the_c172p_within_the_limits_from_every_start_with_a_quarter_of_its_fuel`,
  `..._with_half_its_fuel` and `..._with_three_quarters_of_its_fuel`: 27
  landings each, counted, the weight checked for each.
- `an_aeroplane_handed_to_the_learnt_landing_at_the_gate_lands_within_5_m_of_the_centreline_with_no_step_in_its_controls`:
  0.00833 at the switch; +0.02 m across, 216 ft/min, in ten knots across.
- `the_learnt_policy_touches_down_in_the_simulation_where_it_did_in_training`:
  within 0.44 ft/min, 0.003 m across and 0.21 m along of Python's.
- `the_learnt_landing_sees_and_acts_in_the_simulation_as_it_did_in_training`:
  118 recorded decisions, 4 at the touch; 3.6e-15 and 1.2e-15.
- `the_wind_the_learnt_landing_estimates_is_the_steady_wind_blowing`, worst 0.06 m/s.
- `a_policy_file_that_does_not_fit_the_simulation_is_refused`.
- `the_cli_hands_the_c172p_at_the_gate_to_the_learnt_landing_and_it_is_stopped_on_the_runway`:
  now requires under 300 ft/min and under 5 m in what it prints.
- `the_cli_refuses_a_learnt_landing_for_an_aircraft_that_has_none`.
- `a_learnt_landing_handed_another_aircraft_is_refused_and_the_pilot_keeps_her`.
- `the_cli_hands_the_c172p_at_the_gate_to_the_approach_autopilot_and_it_is_stopped_on_the_runway`.
- `the_cli_land_refuses_a_word_for_a_number`, `..._a_number_with_text_after_it`,
  `..._a_nan`, `..._an_infinity`, `..._fuel_below_nothing` and
  `..._more_fuel_than_a_tank_holds`.

**From the review of #69.**
- **A property by name is found once.** Profiled in the sanitized debug
  build, a learnt landing spent half its samples in JSBSim walking property
  paths - `Aircraft::property`, `state`, `set_controls` and the weather,
  called by name every step by the learnt lander and the approach
  autopilot - and a quarter in the sanitizer's allocations that walk made;
  JSBSim's own step was about a sixth. `Aircraft` now keeps each property's
  node from the first time it is asked for. One landing to the stop in the
  debug build: 3.48 s before, 1.25 s after (release 0.30 s); the release
  selftest hash is unchanged (d36123c1eecc3e23), since every value read and
  written is the same. Locally in linux-debug, at `-j4`: the verification
  test 41.7 s, the simulation-against-training test 38.0 s, each fuel load
  34.8 to 44.0 s. CI measured the verification test at 472 s on Ubuntu
  linux-debug before this (run 36695190570), about five times these.
- **The fuel test is one test a load** - a quarter, half and three
  quarters - each of 27 landings, so that none comes near ctest's 900 s on
  the slowest build. `tests/ci_costs` gains the learnt tests as run
  36695190570 measured them, before the speed-up, and each fuel load as the
  verification test; to be measured again once CI has run them.
- **The fuel test holds the weight**: each load is checked in every tank
  and in `inertia/weight-lbs` - the empty aeroplane, what is on board and
  the fuel - so a load that stopped taking effect fails at the first start
  (seen: "she weighs 1880 lb, not 1730" with `Aircraft::load` made to do
  nothing).
- **A policy flies only its own aircraft**: `LearntLander`, and so
  `Controller::to_ai_learnt_approach`, refuses a policy trained on another
  model before anything is handed over - the pilot keeps her, controls as
  they were (seen to fail with the check taken out).
- **`land` takes only what it can use**: a word, trailing text, a NaN or an
  infinity for a number, and fuel below nothing or above what each of the
  model's tanks holds (185 lb for the C172P's), each refused with a test;
  `--fuel` fills every tank the model has; fuel below nothing is refused
  before the tanks are looked at, and a model with no tanks refuses it
  (untested: every aircraft here has tanks). The node cache makes even a
  const `Aircraft` write, so its header says it is used from one thread.
  Seen to fail: "10kt" read as 10
  with the whole-word check taken out.
- **`Lander::touched()`** replaces `land`'s guess from the touchdown
  figures, and `land c172p` without `--learnt` has a test (seen to fail
  with `touched()` made to say no).

**Seen to fail**, each reverted: the verification test and the fuel test
with #66's policy file in place of this one (19 of 27 and 71 of 81 short of
the limits); the verification test with the observation's distance across
the centreline sign-flipped (27 of 27 short, one 478 m off); the CLI test
with the CLI handing to the approach autopilot instead.

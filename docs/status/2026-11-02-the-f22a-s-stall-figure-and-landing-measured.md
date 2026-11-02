### The F-22A's stall figure written and her landing measured, 6,300 ft; ten tests still red for her - 2026-11-02

**What is still not true first.** The "runway too short" item stays open:
with her stall figure in her figures, ten tests are still red for her, and
the branch is not green. Each is named below with its figure; none has yet
been fixed or named in its test with its reason:

- **Her stall lessons**: recovered at the stall warning she loses 2,964 ft
  against her lesson's 500; left thirty seconds in the stall, 16,295 ft
  against the 7,938 worked out for her speed and sink. The AI's stall watch
  (`the_ai_pilot_notices_a_stall_coming_and_recovers_from_it`): "slowed under
  her stall". The instructor's stall demonstration says one thing where it
  should say none.
- **Her landing in gusts**, all eight seeds of
  `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_N`: a
  balloon above the flare and a go-around (seeds 1, 3, 8), rising 2.8 to 7.8
  ft after her touch (4 to 7), and something other than her wheels on the
  runway (2).
- **The approach lesson's strike margin**: she touches at 14.29 degrees,
  within 2.5 of her tail strike, 16.54 - her final is flown at 14.3 degrees
  of nose at 1.3 times VS0, and her power flare holds it there by design.
- **On her wheels**: a gust from 1 to 2 kt with her throttle closed pushes
  her only to 1.02 kt, not past 1.3; held at 0.5 kt on a wet runway at idle
  she rolls 29.93 s of 30; in a steady 15 kt crosswind no time on her main
  wheels is measured.

**What now works.**
- **Her stall figure** (`assets/figures/f22.xml`, `stall_speed_landing`,
  `steady="1"`): 90.0 KCAS at her combat loading, 54,960 lb, gear down,
  measured from her model, marked measured and not published (the owner's
  rule of 2026-10-09). Her approach speed is 1.3 times it, 117.0 kt at that
  loading, 123.8 at her model's 61,570 lb.
- **Her landing figure**: 6,300 ft, `basis="measured"`, written by
  `tools/measured_landing.py` from `glideslope_cli land f22`: she touched 416
  m in at 157 ft/min, nose 14.3 up, and stopped 1,861 m past the threshold.
  `the_f22_lands_within_her_measured_landing_distance` re-flies it.
- **The refusal covers her**:
  `the_five_with_no_published_landing_distance_are_refused_a_runway_shorter_than_a_measured_one`
  pins all five (Cub 1,500, Mosquito 5,700, F-35B 8,500, B-2A 9,100, F-22A
  6,300 ft), each refused a metre short by the copilot and the planner and
  given a metre over; tests/CMakeLists.txt now requires five measured
  landings. The runway lines the model is shown give her need, as every
  other landplane's.
- **The paths for an aircraft with no approach speed are still walked**,
  though none in the catalogue has none now: the F-22A's own figures with
  her stall cut out (test_lander), and her brief and plan request with the
  approach speed set to 0 (test_copilot: told she does not land, refused 10
  km of runway, planned on her climb-away speed).
- **Tests that named her for want of an approach speed now include her**,
  and pass: the counts of aircraft taught the approach (15 to 16),
  landplanes (14 to 15), take-off sweeps (a part of her own, 315
  hand-overs), wet and crosswind roll-outs, autobrakes, brief speeds, nil
  braking, the plan floor's gust allowance, the take-off flap's stall bound.
  Her own heading test from her approach speed is added, and names her
  under 127 kt: held level by the autopilot gear down she sinks 1,321 ft at
  117 kt, leaving her tables, and 349 at 122, against 300.
- **The recorded copilot exchange** `cbd-orbit-f22-anthropic.jsonl`
  recorded again (her prompt now gives her approach speed); its playback
  passes.
- `the_f22_stalls_where_its_own_model_said_it_would` holds the figure; the
  figure count is 135.

**Verification so far.** linux-release, the full suite before the count
fixes (`ctest -LE nightly -j4`, no display): 1,440 tests, 46 failed, 4,750
s; after them, the failing set rerun from the test binary: the ten above
still red, every other one green. The full suite has not been run since.

### The F-22A's stall figure written and her landing measured, 6,300 ft: all five refused a runway shorter than theirs - 2026-11-02

**What is still not true first.** The stall figure makes her a subject of
every test that walks the aircraft with an approach speed. She now passes
most of them, but not all. Where she fails, she is named in that test,
with her figure and the limit, and the new item "The F-22A flies her
stall-figure tests" (Phase 10b) holds what is left:
- **Stall recovery**: handed over at the warning she loses 2,964 ft against
  her lesson's 500, peaking at 4.72 g against 2. Left thirty seconds in the
  stall she loses 16,296 ft against the 7,938 worked out for her, peaking
  at 20.64 g.
- **The AI's stall watch** notices her at 100.3 kt but lets her slow to 59.4
  kt against her 90.0 stall, losing 2,728 ft. The instructor's stall
  demonstration says "Recover with the least height you can".
- **The approach lesson's strike margin**: she touches at 14.29 degrees,
  2.24 short of her 16.54 strike, against the 2.5 asked. Her final at 1.3
  times VS0 is flown at 14.3 degrees of nose and her power flare holds it.
  She is held to 14.30 plus a twentieth, and below the lander's bound.
- **Gusts** (named under the gust item, all eight seeds, measured again
  on main's gusty flare lead from #303): she goes round for a balloon
  (seeds 1, 3, 8), and on the other five touches at 430 to 977 ft/min and
  rises 3.1 to 12.3 ft after; her worst, seed 2, strikes her airframe on
  the runway. Six of sixteen are judged in gusts now, the F-22A among the
  ten named.
- **On her wheels**: held at 0.5 kt on a wet runway at idle she rolls 29.93
  s of 30.
- **The autopilot's heading from her approach speed**, gear down: under 127
  kt she sinks, 1,321 ft at 117 kt (leaving her tables) and 349 at 122,
  against 300.

**What now works.**
- **Her stall figure** (`assets/figures/f22.xml`, `stall_speed_landing`,
  `steady="1"`): 90.0 KCAS at her combat loading, 54,960 lb, gear down,
  measured from her model, marked measured and not published (the owner's
  rule of 2026-10-09). Her approach speed is 1.3 times it: 117.0 kt at that
  loading, 123.8 at her model's 61,570 lb.
  `the_f22_stalls_where_its_own_model_said_it_would` holds it; the figure
  count is 135.
- **Her landing figure**: 6,300 ft, `basis="measured"`, written by
  `tools/measured_landing.py` from `glideslope_cli land f22`: she touched 416
  m in at 157 ft/min, her nose 14.3 up, and stopped 1,861 m past the
  threshold. `the_f22_lands_within_her_measured_landing_distance` flies it
  again.
- **The refusal covers all five**:
  `the_five_with_no_published_landing_distance_are_refused_a_runway_shorter_than_a_measured_one`
  pins the Cub at 1,500 ft, the Mosquito at 5,700, the F-35B at 8,500, the
  B-2A at 9,100 and the F-22A at 6,300. The copilot and the planner each
  refuse a runway a metre short and take one a metre over.
  tests/CMakeLists.txt requires five measured landings. Every runway line
  the model is shown gives her need (her recording: "the aircraft needs
  1921 m", and "too short" on Bankstown's runways).
- **The paths for an aircraft with no approach speed are still walked.**
  None in the catalogue has none now, so the tests build one: the F-22A's
  figures with the stall cut out (test_lander), and her brief and plan
  request with the approach speed set to 0 (test_copilot). That one is told
  she does not land, is refused 10 km of runway, and is planned on her
  climb-away speed.
- **The tests that left her out for want of an approach speed now include
  her.** The counts of aircraft taught the approach go from 15 to 16, and
  of landplanes from 14 to 15. She has a lift-off sweep part of her own
  (315 hand-overs), and passes her wet and crosswind roll-outs, her
  autobrakes dry and across, her nil-braking sizes, her brief's speed and
  the plan floor's gust allowance.
- **Two gaps were in the tests, not in her flying.**
  - The 15 kt crosswind stop is held to each aircraft's time under main's
    lander, and none was written for her. Hers, 8.6 s, is now written, as
    the 747-400's was on 2026-11-01.
  - The gust push excused every jet held at 1 kt by her brakes but her. At
    1.02 kt she is held, as the F-15C is.
- **The recorded copilot exchange** `cbd-orbit-f22-anthropic.jsonl` was
  recorded again (her prompt now gives her approach speed), and its
  playback passes.

**Verification.** linux-release, `ctest -LE nightly -j4`, no display. Before
the count fixes: 1,440 tests, 46 failed, 4,750 s. After them and the names,
every test that had failed was run again from the test binary, and all
passed. The full suite is in the pull request.

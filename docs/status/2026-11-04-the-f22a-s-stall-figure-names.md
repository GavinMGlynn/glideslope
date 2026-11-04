### The F-22A's stall-figure names: the stall watch and the wet walking pace come off; the wet crosswind roll-out and her gusts still named - 2026-11-04

**What is still not true.** The item stays open. Still named, with their
figures:
- **the wet crosswind roll-out**: 6.13 m from the centreline (2.35 dry),
  unchanged. Its cause is found but not fixed (below);
- **her gusts**, on forty seeds, as
  2026-11-04-the-f22a-s-gear-down-alpha-limit-tried.md has them. Another
  item's tail; not flown again here.

**Taken off, each passing with her name gone:**

1. **The stall watch** (`the_ai_pilot_notices_a_stall_coming_and_recovers_from_it`).
   - **Before**: noticed at the rule's 5%, 126.8 kt at her model's 61,570
     lb, she slowed to 118.2 against her 120.8 stall at that weight, and
     lost 303 ft.
   - **Cause**: noticed there, the AI's recovery held her at her gear-down
     alpha limit (17 degrees) for about four seconds while her engines
     spooled up from idle. At the limit she sinks at about 0.84 g, so she
     flies under her 1-g stall there. Every other aeroplane slows 1 to 6 kt
     past her warning; she slowed 8.6.
   - **Fix**: her own stall warning, 12 kt over her stall
     (assets/figures/f22.xml, `stall_warning_kts`). **Tuned, not sourced**:
     no F-22 stall or angle-of-attack warning was found. Searched
     2026-11-04 for her approach angle of attack, her stall warning, her
     ICAWS angle-of-attack cautions and her approach speed: only flight
     simulator forums and the high-alpha test releases.
   - Measured (linux-release): 8 kt over, she slowed to 120.2, under the
     test's floor of 121.5; 10 kt over, to 122.5; **12 kt over, to 125.1,
     losing 287 ft**. 12 is written.
   - `only_the_mosquitos_figures_give_her_own_stall_warning_ten_knots_over_her_stall`
     is renamed `only_the_mosquito_and_the_f22a_give_their_own_stall_warnings`
     and expects her 12 kt, labelled tuned. Its CI cost lines are renamed
     with it.
   - **What else it moves**: her plan's slowest keeps 157 kt (her warning
     plus the 10 kt gust allowance is 142.8, under her approach speed).
2. **The wet walking pace** (`every_landplane_held_at_every_pace_to_walking_pace_at_idle_on_a_wet_runway_...`).
   - **Before**: at 0.5 kt wet she rolled 29.93 s of 30.
   - **Cause**: the test's push, not her. Her wet brakes come on in the
     hand-over's first third of a second. The push, a proportional-integral
     hand at half her weight per foot a second short, slowed her from 0.5 kt
     to 0.244 before it caught them: eight steps under the quarter knot that
     counts as rolling. She brakes harder than most at walking pace (pushed
     at most 0.30 of her weight, against 0.19 to 0.27 for every other
     landplane but the J-3 Cub's 0.34).
   - **Fix**: the hand is all her weight per foot a second short
     (tests/unit/test_lesson.cpp, `PushedData`). Nothing about her changes.
   - **Every landplane passes it dry, wet and in 15 kt across**: rolling
     all 30 s in 135 of 135 runs each, within 0.4 kt of her pace.

**Why the wet crosswind roll-out is not fixed.** Traced, handed over at
133.6 kt:
- her engines are at take-off power at the hand-over (51,700 lb of thrust,
  from the take-off autopilot's run). They take about three seconds to
  spool down, and she gains 4 kt;
- her nose wheel is off the runway for those three seconds: her nose is 4
  degrees up from the take-off run, and her roll-out's stick lowers it;
- with only her main wheels on a wet runway (side grip 0.30), she slides
  downwind at up to 22.8 ft/s. Most of her 6.13 m comes before her nose
  wheel is down. The lander's rudder law asks almost nothing there: its
  heading and track terms cancel.

A fix in her model or in the lander's generic law did not fit this round's
time box. Nothing about the roll-out was changed.

**Seen red.** Each name, taken off before its fix, failed:
- the stall watch at the rule's 5%: "f22: slowed under her stall for her
  weight", 118.19 kt under 121.32;
- the wet walking pace at the old hand: "f22 at 0.5 kt rolled only 29.93 s
  of 30" (the test's `wrong` list).

**Other aircraft unchanged.** No other flight model or figures file is
touched: assets/figures/f22.xml is the only asset in the diff. The hand's
new gain is in a test; every other landplane's walking-pace runs pass as
before.

**Verification**:
- linux-release (FMA), no display: 301 tests matching her stall, walking
  pace, plan speeds and floor, f22, recording, selftest, committed-file,
  landing and roll tests (`ctest -LE nightly -j4 -R ...`). All pass; the
  two live recording tests are skipped.
- **x86-64-v2 build** (`-march=x86-64-v2`, no FMA, as CI): the stall watch,
  the three walking-pace tests, the stall-warning test and the crosswind
  roll-out all pass. Her figures there: noticed at 132.8 kt, slowest 125.2,
  287 ft lost; at 0.5 kt wet she rolls all 30 s; the roll-out 6.13 m wet.
- **Windows** (`tools/windows_build.sh windows-debug glideslope_tests`,
  this commit): the same six tests all pass.
- Seen red on linux-release: each fix undone with the name off, as above.
- Her model is untouched; `tools/make_f22.py` is not changed.
- Searched 2026-11-04 for a published F-22 stall warning or approach angle
  of attack: none found.

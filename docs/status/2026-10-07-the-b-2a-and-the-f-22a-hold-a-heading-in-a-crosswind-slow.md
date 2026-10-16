### The B-2A and the F-22A hold a heading in a crosswind slow: their own yaw damper and rudder integral; their plans fly from 159 and 140 kt, 2026-10-07 — tail not done

**What was wrong.** On the autopilot in a 20 kt crosswind the B-2A swung her
sideslip 7.3 degrees either way at 159 kt and held it only from 189; the
F-22A 7.8 at 220 and held only from 250. Their plans were kept above 194 and
255 kt for that alone. The sideslip's integral, at 0.05 of rudder a second
per degree, lags the sideslip and feeds the swing; a slower one for every
aircraft (a third, within two degrees) had spun the S.23 in her stall lesson
(2026-10-02), so it was not in.

**What changed.**
- **The gains are each aircraft's, as data**: the catalogue's new
  `yaw-damper PER_DEGPS INTEGRAL_RATE` (`sim/catalogue.hpp`), read into
  `Aircraft::yaw_damper_per_degps()` and `rudder_integral_rate()`, 0.05 and
  0.05 where it says none - every other aircraft flies as before. The B-2A
  says `0.05 0.01`, the F-22A `0.15 0.005`. Tried at 159 kt (B-2A) and 170
  to 220 (F-22A): the B-2A at a third of the integral held to 0.66, a
  fifth 0.35, a tenth 0.22; a larger damper made her worse (0.15: 8.7
  degrees, 0.3: 13.9). The F-22A at a fifth of the integral alone still
  swung 6.7 degrees at 170, at twice the damper alone 11.8; with 0.1 or
  more of damper and a fifth or less of the integral she held from 170.
- **Their plans' floors measured again** (`glideslope_cli plan-speeds b2`,
  `plan-speeds f22 120`): the yaw had spoiled their orbits too. The B-2A
  now holds everything from 154 (below it she comes down 500 ft before
  her circle): slowest **159**, from 194. The F-22A from 135, the sweep
  begun at 120 as she has no approach speed: slowest **140**, from 255.
  Fastest unchanged, 300 and 360.
- **Their glides**: the B-2A's slowest glide measured again
  (`glide-speeds b2`), 159 kt, round unstalled at 13.6 and 13.7 degrees
  where her lift peaks at 13.5 and 13.7; her glide tests are re-banded,
  159-179, 184-204 and 209-230 each way. The F-22A still glides nowhere
  (140 to 275 kt stalls before half way round, to 265 departing past 95
  degrees of alpha; 280 to 360 is less than half way round at 1,000 ft):
  her file keeps 255, her old slowest under power, as her slowest glide,
  so a copilot asks no glide it did not ask before; the open tail "A glide
  may still be asked of a jet at its approach speed" stands.
- The F-22A's recorded Claude plan of the CBD orbit was recorded again with
  the owner's key (`tests/data/copilot/cbd-orbit-f22-anthropic.jsonl`; the
  brief carries her speeds, and the old recording was refused as for
  another request; scanned, no key): the model asked a 3,500 m orbit at
  220 kt, was refused (at least 7,003 m) and took 7,003; flown off 16R.
- **What is not done: the item's verification, from each approach speed
  up.** Measured on these gains (2026-10-07, linux-release), every aircraft
  with an approach speed in the 20 kt crosswind from it up in 5 kt steps
  (the 747-400 and F-22A have none): eleven hold at every speed; five do
  not, all below their plan floors -
  - **737-300** (vref 137): 137, 142 and 157 kt swing 11 to 20 degrees of
    sideslip, heading 84 to 100 off; at 147 and 152 her alpha leaves her
    tables and the flight is stopped;
  - **A380** (vref 136): every speed from 136 to 186 kt, sideslip -12 to
    -16 one way and up to 13 the other, heading 68 to 85 off;
  - **B-2A** (vref 124): at 124, 129 and 139 kt she leaves her tables; at
    134, 77 degrees of sideslip; at 144, 15; at 149, 1.2 (heading 2.3);
  - **Learjet 35A** (vref 125): at 125 kt, 8.5 degrees, heading 12.7 off;
  - **F-35B** (vref 159): 3.1, 2.4 and 1.7 degrees at 159, 164 and 169 kt.
  Why each - the yaw loop, or a wing that will not hold the height clean
  at a flaps-down speed - is not found. The tail stays open with them.

**Verification** (linux-release, locally):
- `the_<id>_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it`
  (new): every aircraft from its plan floor to its start speed in 5 kt
  steps, 262 of 262 cases; the most sideslip anywhere 0.35 degree (B-2A at
  159), the F-22A 0.12 at 140. **Seen to fail** with the two catalogue lines
  removed from the build's data: the B-2A 7.3 degrees at 159, the F-22A
  departing at 140.
- The catalogue's parse test now reads a `yaw-damper` line, the defaults
  without one, and refuses one number and an integral past 1.
- The B-2A's and F-22A's one-step-past tests (149 and 130 kt hold nothing),
  their tightest orbits at their new slowest and fastest, the six new B-2A
  glide bands and the F-22A's, and every test matching b2, f22, crosswind,
  heading, lesson, catalogue, land, stall, orbit, glide, engine,
  instructor, hand, take, plan, copilot, route or speed (464): green. Before
  the floors came down, the one-step-past tests were the two red (each held
  10 kt under its old floor).
- **The selftest hash does not move** (`d36123c1eecc3e23`, linux-release):
  it flies the Cessna on the default gains.
- **Each aircraft's sweep is a test of its own**
  (`the_<id>_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it`,
  sixteen, and `every_aircraft_has_its_own_test_of_...` asserting one each):
  all sixteen in one test, 34 s here in linux-release, ran past CI's 900 s
  in linux-debug. CI's cost tables (`tests/ci_costs/`) carry them at
  estimates - each one's linux-release time here, times 30 for linux-debug,
  25 windows-debug, 10 macos-debug, 1.2 the releases - and the B-2A's glide
  bands at the old bands', until `tools/ci_test_costs.py` measures them.
- **Found on CI (run 37609045925), fixed: windows-debug failed
  `the_f22a_holds_nothing_a_plan_asks_one_step_past_its_slowest`** on a
  JSBSim debug assertion (`FGTable.cpp` line 618, `Factor >= 0.0 && Factor
  <= 1.0`). One step past her new slowest is 130 kt, where in calm air she
  departs: her alpha passes the 90 degrees her aerodynamics' tables hold.
  A key past a table's ends does not trip that assertion - `GetValue` holds
  the end value - so what it caught is a key that is not a number: flown on
  past its tables, the departure carried MSVC's state to a NaN (GCC's
  stayed finite, and Linux debug passed). **Every trial now stops a flight
  whose alpha or sideslip leaves its tables, and judges it not held**
  (`Aircraft::outside_its_tables`, in `fly_heading_in_crosswind`,
  `fly_tightest_orbit`, `glide_tightest_orbit` and `full_throttle_level_kts`;
  `left_tables` in their results). The ranges are read from each model when
  it loads: the widest breakpoints of every table in its `<aerodynamics>` -
  or the file that names - keyed on alpha or sideslip. Most hold -90 to 90
  degrees of both; the A320's and the Cessnas' alpha -5.2 to 20.6, the
  Cherokee's -10 to 20, the Mosquito's -10 to 50, the S.23's -16 to 24; eight
  have no table keyed on sideslip and so no bound on it (named in the test).
  - `every_aircraft_knows_the_alpha_and_sideslip_its_tables_hold` (new): all
    sixteen, each holding 0 to 10 degrees of alpha and either side of no
    sideslip, the eight without a sideslip table named.
  - `a_trial_stops_a_flight_whose_alpha_or_sideslip_leaves_its_tables_and_judges_it_not_held`
    (new): the F-22A at 130 kt in calm air is stopped with her sideslip at
    10.4 degrees, not held. **Seen to fail** with `outside_its_tables`
    always false: flown on, her sideslip reached 89.7.
  - On Windows debug (`tools/windows_build.sh windows-debug`, commit
    7abcbc8): every aircraft's one-step-past test, the F-22A's at 130 kt
    among them, and both new tests - 18 of 18 green, no assertion.

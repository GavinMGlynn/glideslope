### A plan's slowest for what she weighs, and 10 kt over her stall warning, 2026-10-10 — item still open

**What is still missing, first**: the J-3 Cub's climb. At 1,220 lb (her
climb figure's full load) her warning is 39.9 kt, and at her manual's
best-climb speed for that load, 47.8 kt, moderate turbulence takes her
to 40.1 (39.7 in CI): 0.2 kt over it, short of the 3 kt the test now asks
of every phase. In calm air the step from cruise into the climb already
undershoots by 5.2 kt (42.6); the gusts take 2.5 more. The book speed is
not the plan's to change, and the notice is not loosened; the test names
her climb, and her name turns it red once she has the 3 kt. Not decided
here: a climb flown faster in turbulence, or the autopilot's undershoot
when it slows into a climb (4.9 to 6.3 kt for every light aeroplane, calm).

**What changed** (`plan-floor-for-weight`, stacked on #160):
- **A plan's slowest is for a weight**: every figures file's
  `<plan_speeds>` now says `weight_lbs`, her model's own weight - what
  `glideslope_cli plan-speeds` flies (it loads nothing) and what a server
  flies her at (`frontend::brief_for` already briefs at it).
  `sim::for_weight(PlanSpeeds, lbs)` raises the slowest by the square
  root of the weights where she is heavier, as a stall speed goes, so the
  margin it keeps over her warning grows with her stall; it never lowers
  it (a slower speed than measured is one no trial flew), and keeps the
  fastest. `sim::plan_weight_lbs` gives the model's weight.
- **The search for the slowest begins at least 10 kt over the stall
  warning** (`sim::least_plan_slowest_kts`, `sim::plan_gust_allowance_kts`):
  the most of her approach speed as published (rounded, as the search
  always began), her approach speed for the weight a plan flies her at,
  and her stall warning there (`sim::stall_warning_kts`, now shared with
  the AI's notice: 5 kt or 5% over the stall, 14 CFR 25.207(c)) plus 10
  kt, each rounded up. **Why 10**: a plan's slowest is flown level and
  round orbits in whatever air there is, so it keeps at least what a
  pilot adds on an approach in turbulent air - "the normal approach speed
  plus one-half of the wind gust factor" (FAA, Airplane Flying Handbook,
  FAA-H-8083-3C, chapter 9, "Turbulent Air Approach and Landing": 70 kt in
  15 kt gusts is flown at 77) - and the AI's held speed was measured to
  dip up to 8 kt in moderate turbulence on a light aeroplane (the table
  in the entry below); 10 leaves 2 over that.
- **Re-measured where it moved** (`glideslope_cli plan-speeds`,
  linux-release, 2026-10-10): the **B-2A**'s slowest 159 -> **169 kt**
  (her approach speed at her model's 327,000 lb; 159 was under it), held
  all four ways and in the crosswind, fastest 300 kept; the **Mosquito
  FB.VI**'s 123 -> **129 kt** (her approach speed at 19,800 lb against
  her book's 18,000), fastest 264 kept. The J-3 Cub's was flown again and
  stays 43/62 (at her model's 752 lb, 10 kt over her 32.4 kt warning is
  42.4). Every other aircraft's slowest already met the rule; none moved.
  The copilot's and planner's briefs do not move: they already routed no
  slower than the approach speed for the model's weight (169 and 129), so
  **no recording changed and none was re-recorded**.
- `tests/data/plans/b2-on-final.plan` starts at 169 kt, not 168, or it is
  refused; its server test passes.
- **A tail found, to Later** (not a regression: the briefs already took
  the higher): the B-2A's and Mosquito's `<glide_speeds>`, 159 and 128 kt,
  are under their plan floors now; they want measuring again from them.
- **The one-step-past tests' bound** is now where the search begins
  (`least_plan_slowest_kts`) rather than the published approach speed: the
  B-2A's slowest is at it (169), so nothing below it is asked of her, as
  for every aircraft whose slowest is at its approach speed.

**Measured** (linux-release), `the_ai_pilot_notices_no_stall_cruising_or_climbing_in_moderate_turbulence`,
asked / slowest seen, against the warning for her weight, the plan's
slowest now for that weight:

| Aeroplane | weight | cruise | climb | plan's slowest | warning |
|---|---|---|---|---|---|
| J-3 Cub | 1,220 | 60.0 / 52.6 | 47.8 / **40.1** (named) | 54.8 / 45.6 | 39.9 |
| C172P | 2,400 | 100.0 / 90.9 | 75.4 / 67.8 | 67.8 / 60.8 | 51.0 |
| C182 | 3,100 | 120.0 / 110.3 | 82.0 / 75.3 | 71.2 / 64.4 | 54.5 |
| PA-28 | 2,400 | 110.0 / 99.2 | 73.9 / 68.2 | 74.1 / 67.7 | 54.5 |
| Short S.23 | 40,500 | 130.0 / 114.0 | 100.0 / 79.2 | 92.9 / 77.1 | 71.3 |
| Mosquito | 21,020 | 220.0 / 171.8 | 148.0 / 140.3 | 126.7 / 122.1 | 107.2 |
| the jets | | all at least 26 kt over | | | |

Before, at the unscaled slowest: the Cub 37.7 (under her warning), the
C172P 52.2 (1.2 over), the S.23 73.9 (2.6 over). Now the least margin
outside the Cub's climb is the S.23's plan floor, 5.8 kt. None is
noticed.

**Tests**:
- `the_ai_pilot_notices_no_stall_cruising_or_climbing_in_moderate_turbulence`
  flies the plan's slowest for her weight and asks every phase of every
  aeroplane for 3 kt over her warning, counting them: 41 of 42 (fourteen aeroplanes, three phases), the Cub's
  climb named. **Seen red**: the slowest left unscaled, it failed on the
  C172P (52.2 under 51.0 + 3), the Cub (37.7) and the S.23 (73.9);
  reverted.
- New: `every_plan_floor_keeps_its_gust_allowance_over_the_stall_warning_at_the_weight_a_plan_flies_her_at`
  - every aircraft's file weight is her model's (within a pound) and her
  slowest at least `least_plan_slowest_kts`; the 747-400 and F-22A, with
  no stall, counted and named. **Seen red** on the B-2A (159 under 169)
  and the Mosquito (123 under 129) before their re-measured figures.
- `the_speeds_a_plan_may_fly_an_aircraft_at_are_read_and_refused_where_they_are_wrong`
  reads the weight and refuses a file without it or with it at 0.
- Run, all passing (linux-release): the four AI-notice tests (stall
  coming, heavy B-2A, second stall, turbulence), the new test, the plan
  speeds' reading, every-aircraft plan speeds, fastest-in-hand, the plan
  file refusals (the Mosquito's message now says 129), the CLI's refusal,
  a plan or route outside the speeds, a plan from words, the tightest
  orbits at the slowest and fastest and one step past for the B-2A,
  Mosquito, F-15C and J-3 Cub, every aircraft having its own orbit tests,
  the Mosquito's and B-2A's crosswind at every plan speed, the Mosquito's
  glide and the B-2A's first glide bands, the B-2A's orbit entries, the
  server's B-2A on final, the Learjet and Mosquito's plan to the orbit,
  and every test whose name says a recording is played back.
- **The selftest hash does not move**, `182dd6c996e0ee4c`: the C172P's
  flight reads no plan speed, and the notice's warning is the same sum.

### The gust rule made whole: a plan's and a route's speeds and the light climb floor, half the gust factor faster; the Cub and the S.23 keep their margins, 2026-10-10 — item done

**Nothing is missing.** The item's verification holds: every aeroplane in
all three phases of the moderate-turbulence notice test - cruise, climb
and plan floor - stays at least 3 kt over its warning and is never
noticed, none named, and the AI-notice tests (stall coming, heavy B-2A,
second stall) pass.

**The rule, one rule** (`sim::in_gusts`, FAA-H-8083-3C ch. 9's "plus
one-half of the wind gust factor"): in gusty or turbulent air,
- the take-off's climb and initial climb (the entry below);
- **every speed the AI holds from a plan or a route** - each waypoint's,
  each orbit's, the copilot's routes - raised by half the air's gust
  factor in `Controller::fly`, up to her plan's fastest (its
  `limit_speed`), never lowered; the legs to final are the approach's and
  are left at its speed, and a glide flies its own;
- **the light aeroplane's climb floor** (`Autopilot::fly`), the same, up
  to the fastest she may hold.
**The air's gust factor reaches the simulation in its conditions**:
`sim::Conditions::gust_factor_kt`, set by `world::conditions_at` from the
report (`world::gust_factor_kt`) and mixed across a weather's blend, kept
by the aircraft each step (`Aircraft::gust_factor_kt`). Every machine has
the same report, so the client's prediction flies the same speeds. **Calm
air is unchanged** (a factor of 0), and the briefs are the book's
speeds, unchanged - no recording moves. The Later item it replaces is
removed.

**Measured** (linux-release), moderate turbulence (gust factor 15 kt),
asked / slowest seen against her warning:

| Aeroplane | cruise | climb | plan floor | warning |
|---|---|---|---|---|
| J-3 Cub | 60.0 / 53.7 | 55.3 / 50.3 | 62.0 (capped at her fastest) / 51.7 | 39.9 |
| C172P | 100.0 / 90.9 | 82.9 / 77.4 | 75.3 / 68.5 | 51.0 |
| C182 | 120.0 / 110.3 | 89.5 / 84.3 | 78.7 / 74.6 | 54.5 |
| PA-28 | 110.0 / 100.9 | 81.4 / 74.6 | 81.6 / 76.5 | 54.5 |
| Short S.23 | 130.0 / 114.0 | 107.5 / 88.5 | 100.4 / **80.0** | 71.3 |
| Mosquito | 220.0 / 171.8 | 155.5 / 143.7 | 140.4 / 135.5 | 107.2 |
| the jets | | all at least 75 kt over | | |

The S.23's plan floor was 73.1 (1.8 over) held as in calm air; now 8.7
over, the least margin of any phase; every phase has its 3.

**Tests**:
- The notice test holds the plan's slowest as `Controller::fly` does
  (`in_gusts(slowest, gust factor, fastest)`) and asks every phase of all
  fourteen for 3 kt, 42 of 42, none named.
- `the_ai_climbs_half_the_gust_factor_faster_in_gusts_or_turbulence_and_as_ever_in_calm_air`
  now also holds the speed rule: raised, capped at the fastest, never
  lowered, calm unchanged. **Seen red**: both `in_gusts` made to add
  nothing, it failed ("15 gusting 27: gust factor 12, climb 47.8"), and so
  did the notice test (the Cub's climb 40.9, under 39.9 + 3); restored.
- Run, all passing (linux-release, 434 tests, the live ones left out):
  every test whose name speaks of a climb, a take-off, a first leg, a
  departure, a plan, an orbit, turbulence, a notice, gusts, the weather, a
  METAR or a recording, and the capture test - every copilot recording
  played back among them.
- **The selftest hash does not move**, `182dd6c996e0ee4c`: no weather, no
  gust factor.

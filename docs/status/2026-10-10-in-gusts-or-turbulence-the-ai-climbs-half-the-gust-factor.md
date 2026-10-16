### In gusts or turbulence the AI climbs half the gust factor faster; the Cub has her margin, the S.23's plan floor does not, 2026-10-10 — item still open

**What is still missing, first**: the Short S.23 at her plan's slowest in
the test's moderate turbulence. Level at 92.9 kt (her slowest for 40,500
lb) she fell to **73.1 kt against her 71.3 warning**, 1.8 over, short of
the test's 3. It was 77.1 before the climb before it was flown faster:
the same turbulence met at another place and time. She has no speed floor
(only light aeroplanes do), and dips 15 to 20 kt there, more than any
light aeroplane; she is named in the test, and the item stays open on her.

**The rule** (`sim::in_gusts`, `world::gust_factor_kt`): in gusty or
turbulent air the AI's climb speed, and its initial climb speed, are her
best-climb speed plus half the air's gust factor - "the normal approach
speed plus one-half of the wind gust factor" (FAA, Airplane Flying
Handbook, FAA-H-8083-3C, chapter 9, "Turbulent Air Approach and
Landing"), the rule this item's plan floor already cites, applied to the
climb, where the best-rate speed leaves the least over the stall of any
speed she climbs at. **The gust factor is the weather's own figure**: its
METAR's gust spread over its mean wind; or, where the report gives its
turbulence a severity, the spread `world::severity_from_gust_spread`
reads that severity from (`world::gust_spread_of_severity`: 5, 10, 15
moderate, 20, and 30 from severe), whichever is more. Moderate turbulence,
as the test flies, is 15 kt, so 7.5 kt faster. **The same for every
aircraft**: the rule is the air's, not the aeroplane's, so no figures file
carries it. **Calm air is unchanged**: a gust factor of 0 adds nothing, so
every calm climb, take-off and departure test flies as before.

**Where it applies**: the server's AI take-off and climb-out
(`Fleet::fly_plan`, from the session's weather when it has one), and the
turbulence notice test's climb. **Not yet** (a Later item): a plan's own
waypoint speeds, the copilot's routes and the light aeroplane's climb
floor take no allowance, and the briefs are unchanged - so no recording
moves.

**Measured** (linux-release), the notice test's climb at 500 ft/min,
asked / slowest seen against her warning:

| Aeroplane | before (Vy) | after (Vy + 7.5) | warning |
|---|---|---|---|
| J-3 Cub | 47.8 / 40.9 | 55.3 / **44.5** | 39.9 |
| C172P | 75.4 / 69.1 | 82.9 / 72.9 | 51.0 |
| C182 | 82.0 / 76.3 | 89.5 / 81.0 | 54.5 |
| PA-28 | 73.9 / 67.7 | 81.4 / 71.4 | 54.5 |
| Short S.23 | 100.0 / 79.2 | 107.5 / 88.5 | 71.3 |
| Mosquito | 148.0 / 140.3 | 155.5 / 143.7 | 107.2 |

The jets' climbs, all at least 75 kt over. None is noticed in any phase.
Plan floors in the same run: the Cub 45.9, the C172P 60.3, the S.23
**73.1** (named), the rest at least 9 kt over.

**Tests**:
- The notice test climbs at `in_gusts(departure_speeds, gust_factor_kt)`;
  41 of 42 phases have their 3 kt, the S.23's plan floor named; the Cub is
  no longer named. **Seen red**: the gust factor zeroed, the Cub's climb
  came to 40.9, under 39.9 + 3; restored.
- New: `the_ai_climbs_half_the_gust_factor_faster_in_gusts_or_turbulence_and_as_ever_in_calm_air`,
  13 cases counted: calm, a steady wind, gusts alone, every severity 0 to 7
  alone, and gusts with a severity above and below them.
- Run, all passing (linux-release, 299 tests, the live ones left out):
  every test whose name speaks of a climb, a take-off, a first leg, a
  departure, a plan, turbulence, a notice, gusts, the weather or a METAR,
  and the capture test - the server's weather and plan tests among them.
- **The selftest hash does not move**, `182dd6c996e0ee4c`.

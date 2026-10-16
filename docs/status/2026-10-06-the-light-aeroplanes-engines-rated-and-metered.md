### The light aeroplanes' engines, rated and metered, 2026-10-06 — the leaner's restart and the Cub's carburettor done; the 172P, the 182S's ceiling and the Cherokee's curve still open

**What is still missing.** The Cessna 172P's engine still makes 222 hp at
2,700 rpm against its 160 (no test pins its rating: the fix is kept out, see
below); the Cessna 182S, rated right now, still climbs only to 13,612 ft
against 18,100; the FAA's mixture curve is on the 182S and the Cub but not
the 172P or the Cherokee; and the ratio the leaner settles at on the new
curve is not measured.

**Done:**
- **The FAA's mixture curve** (`tools/piston_mixture.py`, shared by the make
  scripts): a MIXTURE table giving the power the shape FAA-H-8083-32 (vol. 1,
  page 2-4) describes - best power at 13.1 to 1, essentially flat from 13.8
  to 12.5, falling gradually and then fast to nothing at 8 to 1, and to
  nothing at 20 to 1 lean - scaled so that full rich at sea level, the rated
  condition, gives the efficiency JSBSim's own table does there. On the
  182S's and the Cub's engines.
- **The 182S's engine rated at 2,400 rpm** (`make_c182.py`, maxrpm 2575 ->
  2400, the handbook's and 3A13's "230 rated BHP at 2400 RPM").
- **A float carburettor** for the Cub (`with_float_carburettor`): a channel
  setting the mixture JSBSim reads to the lever times delta over the square
  root of sigma, so that it richens as a venturi does, not as the pressure.
- **A `rated_power` figure flight** (src/sim/figures.cpp): full throttle and
  full rich, the propeller held at the rated rpm (by its lever, or for a fixed
  propeller by the airspeed), flown again from a corrected height until the
  measurement is within 100 ft of sea level.
- **The leaner's restart rule, changed and tested**: an engine stopped while
  being leaned is richened while it has no fuel to report a ratio, then given
  the ratio last found - not full rich, which above about 9,500 ft is past
  the rich limit. **The catalogue's `mixture-lever FULL_RICH_BELOW_FT`**: the
  leaner richens to full rich below it - 3,000 ft for the two Cessnas (the
  172P handbook's figure 5-6), 0 for the Cherokee (a tail: held full rich
  below its handbook's 5,000 ft, its stall recovered at 4,950 ft lost 328 ft
  against 300).

**Measured** (glideslope_cli figures, linux-release), before -> after:

| | rated power | ceiling on the autopilot |
|---|---|---|
| 182S | 222.5 hp at 2,400 -> 233.6 (230) | 13,639 -> 13,612 ft (18,100) |
| J-3 Cub | 64.5 hp at 2,300 -> 64.4 (65) | 8,016 -> 13,637 ft solo (14,000) |
| Cherokee | 183.0 at 2,700 (180), unchanged | 13,560 ft (13,000), unchanged |
| 172P | 222.5 hp at 2,700 (160), unchanged | 17,226 ft (13,000), unchanged |

Every other figure of the four stays in range.

**The 172P's engine, tried and kept out.** Without its `<bsfc>`, propeller
cp_factor 0.88 and ct_factor 0.96, with the curve and a carburettor: 160.4 hp
at 2,700 and every figure but the ceiling in range (climb 764 ft/min, cruise
119.7 KTAS), the ceiling 15,383 ft against 14,300 at most - the AI holds the
sea-level climb speed all the way up, where the handbook's falls from 76 to
70 KIAS. The learnt landing's 27 starts all still land within the limits;
`the_learnt_policy_touches_down_in_the_simulation_where_it_did_in_training`
goes red until its fixture is recorded again. The fixture's flights are the
exported policy flown in JSBSim's Python bindings, and recording them again
from the committed policy on the old engine gives the committed file byte
for byte, so on a new engine it can be recorded again without retraining.
But `the_autopilot_captures_a_new_heading_altitude_airspeed_and_climb`
then settles its climb in turbulence in 38.95 s against 30, on an engine
with 160 hp where it was tuned on 209. Kept out; its tail says so.

**The Cherokee on the curve, tried and kept out**: its stall recovered at
the warning at 4,950 ft lost 805 ft against its lesson's 300, and its
circuit, its first leg after take-off and its fastest tight orbit went
outside their limits.

**Verified**, each seen red:
- `the_cessna_182ss_engine_makes_230_hp_at_2400_rpm`: 233.6 hp; at maxrpm
  2575, 222.5 - red.
- `the_cherokee_180s_engine_makes_180_hp_at_2700_rpm`: 182.6; its engine's
  maxhp put at 200 in the build's data, 201.7 - red.
- `the_cubs_engine_makes_65_hp_at_2300_rpm`: 64.4; maxrpm put back at 2800,
  55.9 - red.
- `the_ai_climbs_a_cub_flown_solo_to_its_published_service_ceiling`: 13,637
  ft (14,000 +-10%); on JSBSim's metering and curve, 8,016 - red.
- `an_engine_the_leaner_was_leaning_that_stops_is_richened_and_runs_again`
  (172P, leaned 90 s, mixture cut off 5 s, given back 30 s): at 7,000 ft the
  lever goes 0.880 -> 1.000 and the engine runs at 192.9 hp of 193.5; at
  12,000 ft, too. Richened to full rich, as before, at 12,000 ft it never ran
  again - red; with no richening, the lever stayed at 0.880 - red.
- `every_light_aeroplane_is_climbed_to_its_published_ceiling_or_named_with_its_reason`:
  the Cub climbed, without a mixture lever, named as such; 172P and 182S
  named. `every_published_figure_has_a_flight_and_every_flight_a_figure`:
  119 figures.
- Targeted suites green on linux-release: the leaner, the learnt landing,
  the light aeroplanes' figures, ceilings and stalls, lessons, plans and
  orbits, handing over, the committed models.

**From the review** (2026-10-06):
- **A ratio found high up is not given back low down.** Below the full-rich
  height the leaner now takes the ratio it has there as the one to hold, and
  an engine stopped there is richened to full rich; before, an engine leaned
  at 8,000 ft that stopped below 3,000 was walked back towards the 8,000 ft
  ratio. `an_engine_leaned_high_up_that_stops_below_the_full_rich_height_is_given_full_rich`
  (each Cessna leaned at 8,000 ft, brought down to 2,500, cut off, given
  back): the lever stays at 1.000 and the engine runs; with the old rule
  the 182S's lever went to 0.998 and its engine did not run again - red.
- `the_leaner_holds_full_rich_below_the_catalogues_height_and_leans_above_it`
  (each Cessna at full throttle): at 2,000 ft handed 0.8, the lever at 1.000
  for two minutes; at 6,000 ft handed full rich, leaned to 0.915 (172P) and
  0.695 (182S) at most. With the height's rule taken out, the 172P rested
  at 0.867 at 2,000 ft - red.
- The restart test now flies the 182S too, at both heights; its lever is
  cut off 1.5 s, not 5, because its model charges a stopped engine its
  friction (make_c182.py) and the propeller stops within five seconds.
  172P: lever 0.880 -> 1.000 -> 0.880 at 7,000 ft, 0.725 -> 1.000 -> 0.728
  at 12,000; 182S: 0.669 -> 0.818 -> 0.662 and 0.552 -> 0.701 -> 0.556, each
  running again at its leaned power.

**The selftest hash is unchanged**: d36123c1eecc3e23 (linux-release). It
replays a pilot's inputs on the 172P, whose model did not move, and flies no
autopilot or leaner (src/sim/selftest.cpp).

### The 172P on the FAA's mixture curve, and climbed to its ceiling at its handbook's speeds, 2026-10-08 — four tails narrowed, none closed

**What is still missing, first.** **The 172P's engine still makes 222 hp
at 2,700 rpm against its 160**: corrected, every figure is in range, but the
learnt landing, trained on the old engine, lands 22 of its gate's 160
corners at 301-311 ft/min against 300, and two short; the policy needs
training again, which was out of this item's scope. **The 182S still
climbs only to about 12,900 ft against 18,100.** **The Cherokee is still on
JSBSim's mixture curve and leaned at every height**: on the FAA's, with a
float carburettor and full rich below 5,000 ft, its stall at the warning at
4,950 ft is never recovered (1,578 ft lost against 300).

**Done:**
- **The 172P on the FAA's mixture curve and a float carburettor**
  (`make_c172p.py`: `with_best_power_mixture` on `eng_io320.xml`,
  `with_float_carburettor` on the airframe; the O-320-D2J is carburetted,
  handbook section 1). The engine's `<bsfc>` and its propeller are as they
  were.
- **A best-climb speed that falls with height** in the
  `ceiling_on_the_autopilot` flight (`speed_falls_kt_per_1000_ft`): the
  autopilot is asked every step for the handbook's speed at the height it
  is at. The 172P's figure 5-6 gives 76 KIAS at sea level to 70 at 12,000
  ft (75.4 to 70 KCAS by figure 5-1): 0.45 kt a thousand feet.
- **The 172P's service ceiling is a figure** (`assets/figures/c172p.xml`,
  section 1's 13,000 ft, 10%), flown from 6,000 ft.
- **The learnt landing's parity fixture recorded again** from the
  committed policy (`tools/rl/export.py`'s `parity()` on
  `assets/rl/c172p-landing.txt`, JSBSim 1.3.1 in the RL environment): no
  training, the same policy, flown on the new mixture curve.
- `the_autopilot_captures_a_new_heading_altitude_airspeed_and_climb`'s
  turbulent climb is allowed 45 s to settle, not 30: at 100 kt and 4,000 ft
  the throttle is at its stop for most of it, and the 172P leaned makes 190
  hp there on the FAA's curve where JSBSim's gave it 205. Measured: settled
  at 38.2 s; on JSBSim's curve, 20.9 s; with the carburettor alone 21.3,
  with the curve alone 38.3 - the curve is what moves it.

**Measured** (glideslope_cli figures, linux-release), before -> after:

| | rated power | ceiling on the autopilot |
|---|---|---|
| 172P | 222.5 hp at 2,700 (160), unchanged | 17,226 ft at 75.4 KCAS -> 13,126 at figure 5-6's speeds (13,000) |
| 182S | 233.6 hp at 2,400 (230), unchanged | 13,612 ft at 82 KCAS; 12,886 at figure 5-7's speeds (18,100), not a figure |
| Cherokee | 183.0 at 2,700 (180), unchanged | 13,560 ft (13,000), unchanged |

The 172P's other nine figures: static 2,339 rpm, take-off 922 ft, climb
745 ft/min, cruise 119.4 KTAS, glide 9.38, stalls 51.3/47.7/45.9, turn
99%: all in range. Leaned at 8,000 ft at full throttle the 172P sits at
12.87-13.21 to 1 and the 182S at 12.83-13.24 (JSBSim's curve: 9.79-10.03).

**Tried and kept out:**
- **The 172P's engine at 160 hp** (no `<bsfc>`, propeller cp_factor 0.88,
  ct_factor 0.96, with the curve and carburettor): 160.4 hp, static 2,351
  rpm, take-off 906 ft, climb 763 ft/min, cruise 119.6 KTAS, ceiling 12,786
  ft at figure 5-6's speeds - every figure in range. But
  `the_learnt_policy_lands_within_its_limits_from_every_corner_of_its_gate_in_every_wind_it_admits`
  goes red: 22 of 160 short, 301-311 ft/min against 300 (all but two in the
  crosswind from the right), and two touching 12 and 20 m before the
  threshold. Retuned propellers do not save it and leave the figures: cp
  0.84 (climb 830 ft/min, out) 19 short; ct 1.00 (climb 822, out) 7 short.
  The policy learnt its flare on the 209-222 hp engine.
- **The 182S at figure 5-7's speeds** (82 KCAS at sea level falling 0.51 kt
  a thousand feet to 74.8 at 14,000): 12,886 ft, lower than at a fixed 82
  KCAS. Figure 5-7 climbs 925 ft/min at sea level and 285 at 14,000, 46
  ft/min a thousand feet; figure 5-9 has 2,400 rpm and 16 in. at 14,000 ft
  as 51% (117 hp) at peak EGT. With the power the model makes there (136
  hp), 285 ft/min at 92 KTAS needs a propeller efficiency well above its
  sea-level climb's: the handbook's numbers say the propeller gains
  efficiency with height (lighter loading, higher advance ratio) where the
  model's loses it. Not tuned here.
- **The Cherokee on the FAA's curve with a float carburettor**
  (`make_pa28.py`), full rich below 5,000 ft: its nine figures stay in range
  (ceiling 12,279 ft, climb 718 ft/min), its engine idles and opens up at
  4,950 ft (159 hp full rich), but its stall at the warning at 4,950 ft is
  never recovered - 1,578 ft lost - and left thirty seconds in it loses
  1,202 ft against 515. Kept out with its two tails.

**Verified**, each seen red:
- `the_ai_climbs_a_cessna_172p_at_its_handbooks_speeds_to_its_published_service_ceiling`:
  13,126 ft. On JSBSim's mixture curve, 14,332 - red; at a fixed 75.4 KCAS,
  15,995 - red.
- `every_engine_the_leaner_leans_sits_between_12_and_13_8_parts_of_air_to_one_of_fuel`:
  the 172P and the 182S (the Cherokee named as left out). The 172P on
  JSBSim's curve, 9.79 to 10.03 - red.
- `every_light_aeroplane_is_climbed_to_its_published_ceiling_or_named_with_its_reason`:
  three climbed, the 182S named.
- `the_cross_platform_check_accepts_platforms_that_agree_and_refuses_one_that_does_not`:
  `cross_platform_flights.cmake` counts ten 172P figures now, the ceiling
  among them, each held to 1% across the platforms; counting nine it was
  red on eight CI jobs (run 37732259070) and here.
- `a_client_predicting_its_aircraft_stops_its_engine_when_the_server_says_and_is_put_right_no_more`
  was red on windows-release and one windows-clang shard in that run (median
  after the stop 495 and 1,529 mm against 59 before); here, 11 mm against
  40. It is the open Windows tail of 2026-10-07 (429 mm then); whether the
  carburettor's channel widens it on Windows is not yet known.

**Verified** in linux-release: 370 tests - every learnt-landing, figures,
engine, ceiling, climb, leaner, stall, landing, lesson, circuit, orbit,
turn, take-off, autopilot, plan-speed, glide, committed-asset and selftest
test - none failed; the language-model ones skipped, with no key.

**The selftest hash moves**, deliberately: `d36123c1eecc3e23` ->
`be036519d2c19ea0` in linux-release. The selftest flies the 172P, whose
engine's mixture curve and carburettor changed.

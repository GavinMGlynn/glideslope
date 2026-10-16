### From the review of #160: a wet runway takes grip from the brakes alone, is wet only near its station, and its sources pinned, 2026-10-10 — items stay done; one found

**What is still not done, first.** A wet runway leaves a free-rolling wheel's
side force, steering and the airframe's scraping as dry: a cornering source
for wet tyres was not found, so none is reduced (a Later item). The J-3 Cub
ground-loops in a 15 kt crosswind, dry or wet, and is named in the new
crosswind test (Later). A take-off handed to the AI at lift-off is given the
plain autopilot, rightly, but it holds the height she had: 7 of the 13
landplanes touch the runway again within 30 s (a new Phase 10b item). A
server given `--metar` without `--station` tells its planners, asked before
its clock starts, nothing of a wet runway, as the station's place is not
known yet; its copilots are told from then on. **The selftest hash
(`182dd6c996e0ee4c`, unmoved) shows only that dry is unchanged**: the
selftest flies no weather, so nothing wet is in it.

**Braking only** (`Aircraft::apply_weather`, `set_controls`). JSBSim's
`ground/static-friction-factor` scales a wheel's braking, but also its side
force (Pacejka's peak), the nose wheel's steering and the airframe's
scraping friction (FGLGear.cpp, ComputeSideForceCoefficient and the contact
limits). It is left at 1. Instead the brake pedals are given only the share
of them a runway of that code takes: JSBSim brakes a wheel with r + b (s - r)
(rolling r, static s, pedal b), so the share is (mu - r) / (s - r), at most
1, from the braked wheel that grips most - an anti-skid letting through only
the pressure the runway takes. The pedals asked for are kept (`left_brake_`,
`right_brake_`) and the share applied every step; dry the share is 1.
Braking so, a wet braked wheel gives exactly mu, where scaling the static
friction alone gave a little more: the AI's wet roll-out now brings the
pedals fully on at a hand's pace (half a second) rather than finding them by
the deceleration, without which an A320 ran 3 m off the wet 1,725 m runway.

**Locality** (`world::with_air_motion`, `frontend::wet_runways`). A METAR's
present weather is the aerodrome's: weather in its vicinity is reported as
VC, about 8 to 16 km off (ICAO Annex 3, appendix 3; FMH-1's 5 to 10 statute
miles). So `world::metar_radius_m` is 8 km: an aircraft more than 8 km from
the station is given a dry runway, and the copilot and planner are told
`world::WetRunways` - the station and the radius - and judge each landing
runway by whether it lies within it (`copilot::landing_refusal` now takes the
dry and wet needs and where the runways are wet).

**Sources pinned** in `docs/ASSETS.md`, "The runway's condition": AC 25-32
(URL, SHA-256, 12/22/15), 14 CFR 25.109(c) and 121.195(d) on eCFR, and the
METAR's reach.

**Verification.**
- `every_aircraft_takes_off_from_a_wet_runway_as_from_a_dry_one`: all 15
  landplanes (the flying boat named) taken off by the take-off autopilot dry
  and wet leave the ground within 1 per cent and 2 m of each other and wander
  no more than 0.5 m farther from the centreline wet. **Failed on the code
  before** (the ground's friction factor): "mosquito-fb6 wandered 5.24 m from
  the centreline wet, against 4.13 dry"; now 3.09 against 4.13.
- `every_light_aircraft_landed_on_a_wet_runway_in_a_15_kt_crosswind_stays_on_it_as_on_a_dry_one`:
  the C172P, C182S and Cherokee, landed by the AI dry and wet with 15 kt
  across, stop on the wet runway the right way up, no farther from the
  centreline wet than the half-width nor than 1 m beyond dry (C172P 1.72 m
  wet, 3.39 dry; C182S 4.47 and 4.47; Cherokee 5.06 and 6.64); the Cub named.
  **It passed on the code before too**: the side force's cut did not show in
  a light aeroplane's crosswind roll-out, so this test is not evidence of the
  fix, only that it broke nothing.
- `every_landplane_landed_by_hand_on_a_wet_short_runway_is_stopped_on_it_by_the_ai`,
  braking only: 12 of 13 stop, the A320 at 1,720 m of 1,725 (5 m to spare),
  F-15C 2,200 of 2,459, A380 1,768 of 2,225. **Seen to fail** with the dry
  braking: "a320 ... 1780.7 m past the threshold"; reverted.
- `a_metars_rain_wets_the_runways_within_8_km_of_its_station_and_none_beyond`:
  code 5 at the station and 7.9 km off, 6 at 8.1 km.
- `a_runway_turning_wet_or_dry_in_a_blend_turns_halfway_through_it`: dry to
  wet and wet to dry over 300 s, the old code at 149 s and the new at 151.
- `a_session_whose_metar_reports_rain_refuses_a_runway_long_enough_dry_but_short_wet`
  now also: the raining station 7.9 km from the runway refuses it, 8.1 km
  takes it.
- `a_take_off_handed_over_at_lift_off_is_given_the_plain_autopilot_not_a_landing`:
  13 landplanes taken off at full power by the take-off autopilot's controls
  and handed over on the step their wheels leave the runway - inside the
  skip's second and three feet - are given the plain autopilot, none a
  landing.
- `each_runway_condition_codes_wheel_braking_coefficient_is_ac_25_32s`:
  **seen to fail** with the 0.80 anti-skid efficiency taken out ("code 5 at a
  standstill"); reverted.
- Run again (linux-release, 406 tests by name - the roll, take-backs, landings
  by hand, short runways, the lander, landings, runways, stops, skips,
  lift-offs, take-offs, crosswinds, the weather, METARs, the copilot and the
  planner - 30 that ask a live model skipped): 403 passed, 3 failed under
  that load and passed when run again, twice each:
  `an_aeroplane_landing_on_a_runway_another_is_on_goes_around_and_lands_once_it_has_left`,
  `a_players_copilot_glides_when_the_engine_stops_while_it_is_thinking_as_recorded`
  (failed once more on its first re-run, "refused, reason 6", then passed
  twice) and
  `an_aircraft_left_by_a_take_over_is_planned_in_the_air_by_the_servers_chatgpt_as_recorded`.
  Whether they fail so on the base too was not measured; CI's runs will say.

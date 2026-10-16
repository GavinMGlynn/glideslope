### A wet runway: rain in the METAR wets it, its wheels grip as 25.109(c) says, the AI brakes for it and a short one is refused, 2026-10-10 — item still open (closed by the entry above)

**What is still missing, first.** **Only dry and wet are ever flown.** The
simulation brakes on every FAA runway condition code from 6 (dry) to 1
(ice), but the only report it has, the METAR, says that rain or snow is
falling, not how deep anything lies, so nothing sets codes 4 to 1, nor a
runway still wet after the rain has stopped (to Later). **The F-35B is
named** in the wet test: she publishes no landing distance, so nothing sizes
her wet runway, and touching at 155 kt, where a wet runway grips at 0.135,
she stops 2,319 m along. **One tyre for all**: the wet grip is 25.109(c)'s
100 psi curve with fully modulating anti-skid, for every aircraft (to Later).
**The server and clients do not yet brief the copilot wet**: the refusal and
the briefs take the runway condition, but every caller still passes dry. And
**a landing handed over in a skip is not landed** (found here, a Phase 10b
item): handed over while a pilot's landing has its wheels an inch clear, she
is given the plain autopilot - a 787 on the wet runway, wow 0 for ten steps
at the take-back, ran 22.7 km.

**Sources** (searched 2026-10-10):
- FAA AC 25-32, *Landing Performance Data for Time-of-Arrival Landing
  Performance Assessments*, 12/22/15 (SHA-256
  e9588da75cd51d8f9d3c005eb4d289166b380da3412de366ec8cd553484e82f4, from
  faa.gov): table 2, the runway condition codes and each one's wheel braking
  coefficient for a fully modulating anti-skid - 6 dry, the certified value;
  5 wet (3 mm or less) or frost, per 25.109(c); 4, 0.203; 3, 0.163; 2, half
  25.109(c)'s but at most 0.163 below 85 per cent of the hydroplaning speed
  9 sqrt(P), and 0.053 above it; 1 ice, 0.083; 0 nil, no operations.
  Section 8.3: the distance assumes the full braking configuration after the
  transition.
- 14 CFR 25.109(c)(1)-(2) (eCFR, current): the smooth wet runway's maximum
  tyre-to-ground braking coefficient, for 100 psi
  -0.0437 (V/100)^3 + 0.320 (V/100)^2 - 0.805 (V/100) + 0.804, V groundspeed
  in knots, times 0.80 for a fully modulating anti-skid system (0.50 quasi,
  0.30 on-off): 0.643 standing, 0.381 at 50 kt, 0.220 at 100 kt.
- 14 CFR 121.195(d): a runway that may be wet or slippery must be at least
  115 per cent of the dry runway (b) requires.
- Open-Meteo has hourly precipitation, but the METAR is used: it is
  observed at the station, and already reaches every client whole, so the
  server's and each client's runway are the same with no protocol change.

**What changed.**
- `sim/runway_condition.hpp`: the codes, `wet_wheel_braking_coefficient`,
  `wheel_braking_coefficient(code, kts)` (throws for 6 and anything outside
  1 to 6) and `wet_landing_factor` (1.15).
- `sim::Conditions::runway_condition` (6 by default). `Aircraft` finds her
  braked wheels' dry static friction at load (the most among wheels with a
  brake group, or among all wheels where none has) and, before every step,
  sets JSBSim's `ground/static-friction-factor` to the code's coefficient
  at her groundspeed over it, at most 1 - 1 when dry, as before. JSBSim's
  brake is rolling friction plus the pedal's share of static, both times
  that factor, and never locks: an ideal anti-skid. The side force is scaled
  by it too. `Aircraft::runway_condition()` says what she last rolled on.
- `world::runway_condition_of(metar)`: code 5 while DZ, RA, SN, SG, PL, GR,
  GS or UP falls at the station (any intensity, showers, thunderstorm,
  freezing; not in the vicinity), else 6; `surface_conditions` carries it,
  and a blend switches it halfway.
- `Lander`: on a runway that is not dry, a nosewheel aeroplane brakes from
  the touch at MAX's 14 ft/s^2 - AC 25-32's full braking - and the anti-skid
  takes what the runway gives. Not a tailwheel aeroplane: braked so, the
  Mosquito stood on her nose; braked as on a dry runway she stops on a wet
  one.
- `copilot::landing_refusal(approach, need, wet, plan, fields)`, and
  `Brief`/`PlanRequest::runway_wet`: `frontend::brief_for(data, id,
  runway_condition)` gives a wet runway 1.15 times the dry need, and the
  refusal says "on a wet runway". (The copilot sees no simulation header, so
  the factor is applied in the frontend.)

**Verification.**
- `every_landplane_landed_by_hand_on_a_wet_short_runway_is_stopped_on_it_by_the_ai`:
  every landplane taught the approach, landed by hand and handed over half
  a second after the touch, on a runway of code 5 as long as 1.15 times the
  longer of 1,500 m (the dry short runway) and her published need - 1,725 m
  for most, 1,893 the 787-8, 2,225 the A380, 2,459 the F-15C. 12 of 13 stop
  on it (the flying boat and the F-35B named, the count asserted): 737-300
  1,459 m, 787-8 1,769, A320 1,676, A380 1,737, B-2A 1,212, C172P 534,
  C182S 548, F-15C 2,044, Cub 465, Learjet 1,217, Mosquito 1,448, Cherokee
  531. **Seen to fail** with the AI braking as on a dry runway: "a320 ...
  stopped off the runway, 1732.8 m past the threshold"; reverted.
- `a_metar_reporting_precipitation_at_the_station_wets_the_runway_and_none_leaves_it_dry`:
  57 of 57 groups walked (8 precipitations in their forms, 15 that are not,
  and none), and light rain reaches the conditions flown.
- `each_runway_condition_codes_wheel_braking_coefficient_is_ac_25_32s`:
  each code's figures, every code from 0 to 200 kt never gaining grip, and
  6, 0, 7 and -1 refused. First failed on the test's own error (code 2 at
  76 kt is half code 5's, 0.143, not 0.163); the test was corrected.
- `every_aircraft_is_refused_a_runway_shorter_than_it_needs_to_land_on_and_given_a_longer_one`
  now also: briefed wet, each of the ten needs 1.15 times as much, the
  planner too; the runway a metre long dry is refused wet, saying so, and
  one a metre over the wet need is given; the six without are refused
  nothing wet either. **Seen to fail** with the brief's wet need left dry:
  "737-300: briefed wet, it needs 1.15 times as much, for the planner too";
  reverted.
- Run again, all passing (linux-release, 136 tests, the 4 that ask a live
  model skipped, none failed): every test whose name speaks of the roll, a
  take-back, a landing by hand, a short runway, the lander, a landing, a
  runway, a stop, the weather or a METAR - the roll-out, take-over, lander,
  landing-length and runway tests among them.
- **The selftest hash does not move**, `182dd6c996e0ee4c`: dry, the friction
  factor is 1, as JSBSim's default.

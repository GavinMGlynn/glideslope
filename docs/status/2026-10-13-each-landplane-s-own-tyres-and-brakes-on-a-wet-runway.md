### Each landplane's own tyres and brakes on a wet runway, 2026-10-13 — item done; one found

**What is still not done, first.** **Three landplanes run past the wet
short runway** now that each brakes on her own tyres, and are named in
`every_landplane_landed_by_hand_on_a_wet_short_runway_is_stopped_on_it_by_the_ai`:
the 787-8 at 1,911 m of 1,893 (228 psi), the A320 at 1,809 of 1,725 (209
psi) and the F-15C at 2,478 of 2,459 (305 psi). It was green before only
because every tyre was 100 psi. A new Phase 10b item. Reversers alone are
not shown to be the answer: the 787-8's and F-15C's ~20 m on ~2 km is the
size of the hand landing's fast touch (146.5 and 157.8 kt) against the 1.15
margin, a candidate cause as much as the missing reverse thrust. Each
named overrun is bounded (25, 90 and 25 m), so a growing one fails.
**#185 (short runways)** sizes wet runways for the Cub and Mosquito, which
may overrun on these brakes: whichever of the two lands second re-runs the
wet short-runway test and extends the named list or fixes the sizing.
**Four pressures are estimates** (F-22A, F-35B, B-2A,
Mosquito) and the anti-skid class of the 737-300 and the four military
aircraft is this project's reading: no document names it.

**Which side was wrong** (looked at before changing either):
- **The AI brakes fully.** The A320's roll logged once a second: pedals
  1.0 from about a second after the take-back, throttle idle, spoilers
  out, decelerating 5.1 to 5.5 ft/s^2 from 140 to 110 kt on a wet
  coefficient of 0.14 to 0.18 - the brakes at the runway's limit. What a
  crew would add on a wet runway is reverse thrust (AC 25-32 section 10
  credits it), and no flight model here has a reverser. The hand landing
  also touched fast: the A320 at 144 kt, 342 m in.
- **The runway is the regulation's.** The test sizes it at 1.15 times the
  longer of 1,500 m and `landing_need_m`, which for the airliners is a
  landing field length - the chart's own "actual distance / 0.6", 14 CFR
  121.195(b) - and for the F-15C her flight manual's dry distance over 50
  ft times 1.67, the same margin. The A320's is longer than the rule needs
  (1,725 m against 1.15 times her 1,341). Neither the 1.15 nor the grip
  curves were touched.
- So the three are flown and named: each must still stop, unwrecked and on
  the centreline, and each must overrun - named so that the day one stops,
  the list is shortened. **Seen to fail** with the C172P named among them:
  "c172p ... is named as overrunning the wet runway, and stopped on it,
  567.6 m"; reverted.

**What changed.**
- `sim/runway_condition.hpp`: `WheelBrakes` (tyre psi, `AntiSkid`), and
  the wet curve now 14 CFR 25.109(c)(1)'s four, for 50, 100, 200 and 300
  psi, linearly interpolated as (c)(1) allows and held at their ends (no
  extrapolation: a 15 psi Cub tyre is given 50 psi's curve); the speed held
  to 200 kt (the 50 psi cubic turns up from about 240). (c)(2)'s 0.80,
  0.50 and 0.30, and **no anti-skid 0.55**: a wheel braked by hand locks
  and skids, and NASA TM X-72650 (Horne, 1975) figure 8 puts mu skid at
  about 0.6 of mu max at low speed and 0.5 from 60 kt (NASA TP-1080 agrees).
  AC 25-32 table 2's other codes take each anti-skid's share over 0.80
  (its note 3: 0.625, 0.375), and code 2's hydroplaning speed is 9 sqrt(P)
  of her own P (note 4).
- Every landplane's catalogue file has `brakes PSI ANTI_SKID`, each with
  its source in a comment and in `docs/ASSETS.md`; a landplane's file
  without one is refused. 737-300 201 psi, 747-400 200, 787-8 228, A320
  209, A380 218, Learjet 35A 156, F-15C 305 (all anti-skid); F-22A 286,
  F-35B 300, B-2A 224 (estimates, anti-skid); C172P 28, C182S 42, PA-28 24,
  J-3 Cub 15, Mosquito 50 (estimate) - none of the five with anti-skid.
- `Aircraft::wheel_brakes()` (from her file), `set_wheel_brakes`,
  `brake_share()` and `runway_braking_coefficient()`. A model loaded
  without a catalogue and put on a wet runway throws, naming what is
  missing.
- **The brake share is a ceiling, not a scale**: the pedals are given
  min(pedal, share), not pedal times share. Scaled, a light steering brake
  lost grip the runway would have given it, and the Mosquito, braking to
  steer on her own softer, anti-skid-less share (0.47), wandered 6.31 m
  from the centreline wet against 4.13 dry; capped, 4.13 both.

**Verification.**
- `a_wet_runways_grip_is_read_for_the_tyres_pressure_and_the_anti_skid`:
  the four curves at a standstill and 100 kt, 150 psi halfway, held at 29
  and 305; a softer tyre never grips less at every pound from 20 to 320 and
  every knot to 170 (51,471 points, asserted; 171 to 200 kt left out and
  named: the rule's 200 and 300 psi fits cross at 174 kt); every kind, code
  and knot (4,020) its share over 0.80; a 29 psi tyre's hydroplaning at 41.2
  kt. **Seen to fail** with the interpolation's weight set to 0: "100 psi's
  curve: 0.883 standing"; reverted.
- `each_aircraft_stops_on_a_wet_runway_on_her_own_tyres_and_brakes`: all 15
  landplanes (the flying boat named), braked from 20 kt (a tailwheel
  aeroplane's stick back and pedals half on - fully on the Mosquito stands
  on her nose), are given at every braked step her own coefficient; stop
  later wet than dry; and later than on the 100 psi modulating tyre where
  hers grips less (all 15 do: 737-300 16.7 m against 15.9, A320 17.9 / 16.9,
  F-15C 25.9 / 20.1, C172P 38.6 / 30.6, Cub 63.8 / 51.3). **Seen to fail**
  with the aircraft braking on the 100 psi tyre again: "737-300's wheels
  were given what the runway gives her tyres and brakes"; reverted.
- `an_aeroplane_without_anti_skid_stops_later_on_a_wet_runway_than_with_it`:
  all five without (the count asserted) stop later wet than the same
  aeroplane fully modulating - C172P 38.6 m against 28.3, C182S 47.2 / 33.8,
  Cub 63.8 / 47.3, Mosquito 63.6 / 43.7, PA-28 41.6 / 29.9 - and dry exactly
  as far. **Seen to fail** with the same seeded bug: "c172p without
  anti-skid stopped in 30.63 m wet, with it 30.63"; reverted.
- `a_light_pedal_on_a_slippery_runway_is_given_as_pressed_up_to_the_runways_share`:
  every landplane (the flying boat named) braked from 20 kt at a fifth of
  her pedals is given exactly min(pedal, share) at every braked step - the
  pedal on a wet runway (share 0.46 to 0.96), the share on ice (0.05 to
  0.13); on ice a light aeroplane's idle propeller out-pulls a fifth of her
  brakes, so only wet is asked to stop. **Seen to fail** with the old
  pedal times share: "737-300's brakes were given the less of the pedal and
  the share"; reverted.
- The named wet overruns are each bounded; **seen to fail** with the
  A320's ceiling at 50 m: "a320 ... overran the wet runway by 83.7 m, more
  than the 50 named"; reverted.
- `an_aircraft_is_added_by_its_file_alone_and_refused_where_it_is_wrong`
  now also reads all four anti-skids and refuses a landplane without
  brakes, an unknown anti-skid, a missing one and a 0 psi tyre.
- Run again (linux-release, 148 tests by name - wet, brakes, landings on a
  runway, roll, figures, stops, short runways, published, the catalogue,
  METARs, the runway condition; 3 that ask a live model skipped), and the
  light-pedal test: all 149
  passed, the three wet overruns named. The published figures tests all
  pass: dry is untouched (share 1).
- **The selftest hash does not move**, `182dd6c996e0ee4c`: it flies dry.

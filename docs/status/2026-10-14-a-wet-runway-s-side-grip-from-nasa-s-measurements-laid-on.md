### A wet runway's side grip from NASA's measurements, laid on every wheel, 2026-10-14 — item done

**What is not claimed, first.** **The wet side grip makes a wet roll-out
slide faster across her heading, not end farther from the centreline.**
Braking held equal and only the side grip changed, every jet slides at least
1.2 times as fast with the wet grip (the 787-8 10.8 against 5.9), but the AI's
roll-out steers her back, and most jets end nearer the centreline wet (the
787-8 13.5 m against 19.1). The light aeroplanes show little: their 30 to 35
psi tyres keep three quarters or more of their dry grip at their slow
roll-outs. The Mosquito, a tailwheel aeroplane, slides less wet. The first
difference test tried, the PA-28 dry against wet (7.0 m and 15.1 m from the
centreline), was the braking's: it stayed green with the side grip taken
out, and was removed. And **the roll-out bound is the runway's half-width,
30 m**, because the AI's roll-out drifts 10 to 21 m even dry when 15 kt of
crosswind arrives at once (the 737-300 12.5 m, the A320 21.0, the F-35B
21.2); a tighter bound waits on the roll-out's centreline law, not on the
tyres.

**What now works.** A rolling tyre on a runway that is not dry grips less
sideways, by how much NASA measured:

- **The source** is NASA TN D-7743 (Dreher and Tanner, 1974), table II: five
  30 x 11.5-14.5 type VIII tyres at 265 psi braked and yawed on the same damp
  concrete. The most unbraked cornering coefficient over 4, 8 and 12 degrees
  of yaw, over the unyawed tyre's maximum braking coefficient, averages
  **0.571 at 5 kt, 0.603 at 50 and 0.769 at 100** (per tyre: 0.57, 0.52,
  0.72, 0.56, 0.48; 0.57, 0.61, 0.52, 0.65, 0.67; 0.47, 1.05, 0.77, 0.86,
  0.70). A wet tyre's cornering coefficient is that share times the runway's
  maximum braking coefficient - 25.109(c)(1)'s curve at her tyre's pressure,
  the braking coefficient before any anti-skid's share
  (`runway_maximum_coefficient`).
- **Held to a second report it was not made from**: NASA TN D-7351 (Tanner
  and Dreher, 1973), figure 7, a 40 x 14-16 type VII tyre at 155 psi and 10
  degrees of yaw. Its damp fairing reads 0.44 at 5 kt, 0.26 at 50 and 0.20 at
  100; the mapping predicts 0.404, 0.269 and 0.202. Its dry fairing, 0.48 at
  5 kt to 0.43 at 100, is the dry tyre's cornering.
- **The share of her dry side grip she keeps** is the wet cornering over the
  dry, at most 1 (`side_grip_share`): for the 100 psi tyre about 0.91 at a
  walk, 0.63 at 50 kt and 0.49 at 100. Codes 1 to 4 carry the wet share to
  their own maximum coefficients - an extrapolation, since no source here
  measured cornering on snow or ice.
- **Laid on the flight model**: JSBSim caps a wheel's side force at its
  static friction (the peak of its cornering curve), so every wheel's static
  friction is scaled by the share each step on a runway that is not dry; a
  wheel is a bogey whose rolling friction is below its static (the A320's
  tail and wingtip bogeys and the flying boat's keels are not). JSBSim brakes
  with the same coefficient, so the pedal sent is scaled by (s - r) /
  (share s - r) and the brakes grip exactly what the runway code gives them,
  as before (`Aircraft::brake_scale`). Dry, nothing is touched.

**Verification**, each test seen red:

- `a_wet_runways_side_grip_is_nasas_measurement_of_a_wet_tyres_cornering`:
  the share at 5, 50 and 100 kt is the mean of table II's fifteen rows,
  transcribed in the test; read linearly between and held beyond; TN D-7351's
  damp figure predicted within 0.05; the share in (0, 1] on every code at
  every knot 0 to 160 for nine tyre pressures, and falling from 5 to 100 kt
  wet. Red with the 5 kt share set to 0.60.
- `every_landplane_rolls_out_on_a_wet_runway_in_a_15_kt_crosswind_and_stays_on_it`:
  twelve landplanes run up in calm air to nine tenths of the lesser of their
  rotation and approach speeds and handed to the AI's roll-out, told the
  runway, as 15 kt arrives from the left; each stops on the wet runway,
  unwrecked, within 30 m of the centreline (the most, the Mosquito, 26.8 m;
  dry she nosed over), her side grip at least once below 0.8 of dry. Left
  out, named: the S.23 (no wheels), the 747-400 and F-22A (no stall speed
  published, so the AI lands neither), the J-3 Cub (her roll-out never
  brings her below half a knot). Red with the side grip held at 1.
- `on_a_wet_runway_with_its_braking_held_the_wet_side_grip_alone_makes_every_jet_slide_faster_across_her_heading`:
  every landplane of the roll-out test rolled out wet twice, the side grip
  as the runway gives it and held at dry (`Aircraft::hold_side_grip`, for
  tests), the brakes braking as the wet code says both times. The eight jets
  slide across their heading at least 1.2 times as fast at worst with the
  wet grip - a ratio, so that a platform's floating point moves both runs
  alike; the least measured, the A320's, 1.29 - and this is the item's
  grip-dependent check, which the roll-out test's 30 m bound is not: 737-300 10.7 against 6.9 ft/s, 787-8 10.8/5.9, A320 14.2/11.0, A380
  7.7/4.2, B-2A 4.1/2.3, F-15C 5.8/3.8, F-35B 9.3/5.8, Learjet 7.5/4.8.
  Named and printed, not asserted: the C172P (27.1/27.1), C182 (11.7/11.3),
  PA-28 (14.4/12.7) and Mosquito (25.8/28.8, a tailwheel aeroplane sliding
  less wet, not explained, not investigated). No assertion exercises a light
  aeroplane's wet side grip. Red with the share held at 1.
- **The pedal's rescale has a real floor** (review of #192): the share is
  held to at least 2r/s, where (s - r)/(share s - r) stays bounded; no code
  reaches it for any catalogue aeroplane (least share about 0.09, largest
  2r/s 0.08). It relies on JSBSim's SetLBrake and SetRBrake not clamping
  the pedal, which they do not.
- The stopping harness now checks the pedal sent is the less of pedal and
  share times the brake scale. With #188's reversers and the wet side grip
  together, every landplane landed by hand on her wet short runway stops on
  it: the 787-8 at 1,563 m of 1,893 and the A320 at 1,494 of 1,725 (1,552 and
  1,478 without the side grip), the F-15C at 2,441 m on the runway sized
  from her manual's wet chart; none is named as overrunning.

**The selftest hash does not move**: 6e988ba216e5ee07 in linux-debug and
182dd6c996e0ee4c in linux-release - it is
flown dry, where nothing is touched.

Sources, with SHA-256, in `docs/ASSETS.md`.

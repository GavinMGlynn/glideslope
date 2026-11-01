### The AI lands the J-3 Cub three-point, 2026-11-01 - one item done

Phase 10b's "The AI wheel-lands the J-3 Cub" is ticked.

**What is still not true first.**
- **She touches two degrees short of her standing attitude, not on it**:
  10.2 degrees where she stands at 12.1, her main wheels a moment before her
  tail wheel. The FAA's Airplane Flying Handbook (FAA-H-8083-3C, ch. 14,
  "Normal Landing") has a three-point landing touch on all three together
  at about the stall. Her model's lift (assets/jsbsim/aircraft/j3cub,
  `Lift_alpha`) is 1.47 at 7.5 degrees of incidence, 1.50 at 10 and 1.62 at
  12.5, its peak: the last four degrees of nose come in the last knot, about
  a second, and her wing gives out first. The margin stated is 2.5 degrees.
  Whether the Cub's real lift curve is that flat was not looked into; the
  model is FlightGear's, unchanged in lift by tools/make_j3cub.py.
- **In gusts she is still flown on to her main wheels**, as every aeroplane
  is in gusts (the AFH, ch. 9, "Turbulent Air Approach and Landing", and
  ch. 14 prefers a wheel landing in gusty wind). The gust tests are not
  changed by this item.
- The Mosquito is still wheel-landed: her figures do not say `lands`, and
  her Pilot's Notes were not read for it.

**What changed.**
- Her figures (assets/figures/j3cub.xml) say `lands="three_point"`; a figures
  file saying anything but `three_point` or `wheels` is refused
  (src/sim/figures.cpp), and the lander refuses three-point figures for an
  aeroplane that does not stand on a tail wheel.
- The flare (src/sim/lander.cpp), for a three-point aeroplane not in gusts,
  while her nose is more than a degree under her standing attitude: her
  wheels are flown down at the flare's own sink to half a foot over the
  runway and held there (a little climb asked for below it); the nose is
  raised four times as hard for the sink it is asked to arrest, its rise not
  damped up to 4.5 degrees a second, and the elevator's flare gain is 6, not
  3. From the touch her stick is held fully back (the AFH, ch. 14).
- Tried and not kept, each in calm air: the flare's own law held to a foot,
  5.5 degrees at the touch; held a foot or two with more nose, 8.8 to 9.5,
  dropping on at 100 to 150 ft/min; at half a foot without the undamped rise
  and the doubled gain, 9.3 to 9.4.
- `glideslope_cli land` prints her nose at the touch.

**Figures, before and after** (linux-release, calm air, her model's own
loading, 922 lb): touched 301 m in at 2.8 degrees and 88 ft/min, stopped 498
m past the threshold; now touches 358 m in at 10.2 degrees and 66 ft/min, and
stops 441 m in - her tail is down at once, so her brakes no longer wait. Her
measured landing distance, re-measured with `tools/measured_landing.py`, is
1,500 ft (was 1,700), and the copilot test's pin with it (the shortest
runway she is allowed: the "runway too short" item). In every crosswind to
12 kt, both sides, dry and wet (48 landings) she touches at 10.15 to 10.23
degrees.

**Verification.**
- New: `the_j3_cub_is_landed_by_the_ai_three_point_in_calm_air_dry_and_wet`;
  the four Cub crosswind tests now also hold every touch within 2.5 degrees
  of her standing attitude. **Seen red** with the three-point flare switched
  off in the lander: 2.84 degrees in calm air dry and wet, and every
  crosswind landing from the left, dry; the measured landing distance test
  red too (498 m against 1,500 ft). Reverted, green.
- New: `how_a_tail_wheel_aeroplane_lands_is_read_and_anything_but_three_point_or_wheels_is_refused`
  and `a_three_point_landing_is_refused_an_aeroplane_without_a_tail_wheel`;
  **seen red** with each refusal taken out, reverted, green.
- `the_j3cub_lands_within_her_measured_landing_distance` at 1,500 ft (441 m,
  411 to 457 allowed).
- Every other aircraft's landing is untouched: the change is behind her
  figures' `lands`, and not in gusts.

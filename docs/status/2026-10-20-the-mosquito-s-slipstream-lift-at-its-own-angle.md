# The Mosquito's slipstream lift defined at any angle her creep gives

**What now works.** Her `lift-slipstream` - her wing's lift coefficient on the
propellers' slipstreams' extra dynamic pressure over 90 sq ft a side - reads
the coefficient, for each propeller, at the angle the air in its slipstream
meets the wing, atan2(w, u + 1.5 vi), not at her angle of attack, atan2(w, u).
Standing on her brakes that angle is near nothing at any power, and the term
is 55 to 671 lb from closed throttle to full, where it was -2,645 to +3,790 lb
flipping as she crept. `tools/make_mosquito_fb6.py` makes it
(`with_slipstream_lift_at_its_own_angle`, its docstring's entry of
2026-10-20): two new functions per side, the angle and the wing's own table
read at it, the table copied from the wing's so they cannot part.

**Why that angle.** The slipstream's speed along her x axis is what the term
already gave it, u plus 1.5 times JSBSim's induced velocity; across it the air
moves at w, as outside it. In flight at low power the two angles are the same;
at high power and low speed the slipstream's is the smaller, as it is on a
real wing behind a propeller. Where a propeller gives nothing its angle may
again be anything, but that side's term is multiplied by its own extra
pressure, then nothing.

**Verification.**
- New test,
  `the_mosquito_standing_on_her_brakes_at_any_power_calm_or_in_15_kt_across_has_her_slipstream_lift_at_her_wings_coefficient_for_the_air_along_her`:
  seven loadings (her own and the figures' six) x eleven throttles (closed to
  full, -9 to +18 boost) x calm, 15 kt from her left, 15 kt from her right;
  stood on full brakes with the stick back, run up, then the wind; over the
  last five seconds of each, every step's coefficient (lift over the
  slipstreams' extra pressure on 180 sq ft) within 0.02 of 0.17 (a quarter of
  a degree of her lift curve). Measured 0.166 to 0.176, never moving 0.01 kt.
  Coverage asserted: 231 runs, 138,600 steps judged. **Seen red on main's
  model**: all 231 runs wrong, out by 0.63 to 0.86, the coefficient -0.69 to
  0.99.
- **The nose-over test's level moved, 0.60 -> 0.65 at five loadings** (her
  own, HX809's, the three Pilot's Notes'; HJ679's and the B.IV maximum's were
  0.65 already). Traced at her own loading, brake 0.60: on main's model the
  term was up to 3,936 lb at her 12 degrees as the braking began, lightening
  her main wheels, and -341 lb as her nose came down through -9.5 degrees of
  angle of attack, pressing it down - she tipped to -3.1 degrees. Now 1,178 lb
  at most and 78 lb as she stops: at 0.60 she dips to 0.9 to 6.9 degrees at
  every loading and stays on her wheels; from 0.65 she tips at all seven (56
  of 140 runs). Pin and comment updated; the owner's 2026-10-17 decision (her
  tipping her real behaviour) is unchanged.
- **Every figure in range** (linux-release, all Mosquito tests, the alpha-term
  checks, the standing and braked-stop tests, selftest: 49, all pass). Moved:
  take-off distance 2,521.9 -> 2,492.3 ft (range 2,146.5 to 2,623.5), unstuck
  715 -> 708 m in the Sydney flight; take-off swing 2.619 -> 2.555 lb/sq in of
  port lead (range 0.2 to 3.0) - measured on her brakes at +9 boost, where the
  old term flipped; the rest in the third decimal or a foot (climbs, safety
  speeds, single-engine ceiling, clean stall 105.582 -> 105.580 kt). Selftest
  hash unmoved (C172P).
- **x86-64-v2 build** (no fused multiply-adds, as CI), because the
  nose-over level is edge-sensitive (at 0.60 her nose comes within 0.9
  degrees of the horizontal): the same 45 tests pass, the 0.60 dips the same
  to 0.1 degree, take-off distance 2,492.333 against 2,492.331 ft, the swing
  2.555 the same.
- `the_committed_mosquito_airframe_and_engine_are_what_their_script_writes`
  passes.

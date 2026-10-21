### The J-3 Cub's wing reversed: her lift and pitch tail to the wind, 2026-10-21 — item done

**What is still not true first.**
- **Her pitching moment past 90 degrees is derived, not measured on her**
  (owner's decision, 2026-10-20): the reversed section's force normal to its
  chord at its centre of pressure. **The centre moves** from half the chord
  at 90 degrees to three quarters at 180, where the decision said "about
  three quarters": tail to the wind (164 to 180 degrees) that is 0.71 to
  0.75 of the chord, and 0.28 of moment against 0.30 at three quarters
  throughout. The moving centre is held to a measured moment: NACA TN 3361
  (Critzos, Heyson and Boswinkle, 1955,
  https://ntrs.nasa.gov/citations/19930084501), figure 1, the NACA 0012's
  quarter-chord moment through 180 degrees at Re 1,800,000, read off the
  graph - about -0.5 at 90 to 120 degrees and -0.4 to -0.45 at 150 to 175.
  The moving centre meets it within about 0.05 at every angle; three
  quarters throughout gives -0.85 at 110, off by 0.35. Kept on the
  coordinator's word (2026-10-21).
- **Her section is a NACA 0012's**, the nearest measured through 180 degrees
  to her cambered USA-35B (owner's decision); that hers is alike reversed is
  an inference.
- **Her tailplane in reversed flow and her elevator's term** are not
  modelled apart: the derived moment is her whole moment past 90, and
  Pitch_elevator is unchanged.
- Between 90 and 95 degrees her Cm-alpha fades into the reversed term; no
  source gives that stretch.

**The change** (tools/make_j3cub.py, run; assets/jsbsim/aircraft/j3cub/j3cub.xml
remade):
- Lift_alpha's table runs on past plus and minus 90 degrees to 180 by
  Sheldahl and Klimas, SAND80-2114, Table 2, NACA 0012 at Re 1,000,000
  (page 22; 700,000 identical), odd in the angle, scaled by 1.41/1.80 as
  her drag is: -0.66 at 160, -0.85 at 170, 0 at 180 unscaled.
- Pitch_alpha is multiplied by a table of the angle of attack, exactly 1
  within 90 degrees and 0 from 95: in flight bit-for-bit unchanged.
- A new term, Pitch_reversed_flow (on aero/qbarUW-psf, like her other
  angle-of-attack terms): 0 within 90 degrees, and from 95 to 180 either way
  -(x_cp - 0.25) times the scaled normal force, odd in the angle, 0 at 180.

**Measured** (linux-release), stood on her brakes tail to 15 kt:

| | Before | After |
|---|---|---|
| Angle of attack | -164 deg | -166.8 to -167.4 deg |
| Lift (her wing) | 0 lb | 85 to 88 lb, pressing her down |
| Air's pitching moment about her quarter chord | +4,150 ft-lb (Cm 5.8) | +198 to +204 ft-lb (Cm 0.28) |
| Drag along the wind | 29 lb | 23 to 24 lb |

Her stance moved because the 4,150 ft-lb had been squashing her tail
wheel; the drag test still holds within 0.0001 of its figure.

**The new test**,
`the_j3_cub_standing_tail_to_a_15_kt_wind_has_a_reversed_wings_cited_lift_and_the_pitching_moment_it_makes`
(tests/unit/test_departure.cpp): four loadings (her model's own and her
figures' three), 15 kt from behind after five seconds' calm, the last five
of fifteen seconds judged at every step: not moved, tail wheel down, the air
from behind, her lift terms' sum within 0.01 of the cited section's
coefficient as her model scales it (times her ground effect, 1.1), and her
pitch terms' sum within 0.01 of the derived moment. Lift off by 0.0001 at
most, moment by 0.0011. 600 steps each asserted.

**Verification.** Seen red on her model before the change, every loading:
lift coefficient 0.000 against 0.570, Cm 5.80 against 0.25. Seen red with
the centre of pressure at three quarters throughout in the script: Cm 0.30
against 0.28, off by 0.022 at every loading; reverted, the script re-run,
`make_j3cub.py --check` clean. In linux-release the 84 tests named cub, j3,
committed, selftest, figures, tail_wheel, every_landplane or walking pass;
`glideslope_cli selftest` hash `521cb12d3a1c5f0c`, unchanged (her flight is
unchanged to the bit). In a `-march=x86-64-v2` release build the 36 tests
named cub, j3, tail_wheel, every_landplane_braked or walking pass.
CI's first run failed one test this set missed,
`every_flight_models_angle_of_attack_pitch_terms_leave_out_the_crosswind_or_are_named_with_their_reason`:
the new term reads the angle of attack and was not listed. It is now in
tests/tools/alpha_pitch_terms.py's moved terms for the Cub (20 of 20 moved);
locally the 28 tests named every_flight_model, terms, committed or
every_model pass.

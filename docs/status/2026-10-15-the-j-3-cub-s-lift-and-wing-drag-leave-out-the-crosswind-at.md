### The J-3 Cub's lift and wing drag leave out the crosswind; at every walking pace in 15 kt across she stays on her wheels, 2026-10-15 — item done

**What is still not true first.**
- **She leans 4.2 to 4.4 degrees standing in 15 kt straight across**, her
  upwind main wheel still carrying weight (0.08 ft of its spring at her
  model's own loading, against 0.51 on the other). What leans her is her
  sideways drag, `Drag_beta`, the pinned model's 1.23 on her wing's area at
  90 degrees (some 158 lb at 15 kt), and her dihedral term, both about her
  soft main gear (1,000 lb/ft). Whether a real Cub leans so is not
  measured: a new Phase 9 item, "The J-3 Cub leans 4.4 degrees standing in
  15 kt across".
- **Her roll terms keyed to the angle of attack are left on the whole
  dynamic pressure**: `Roll_beta` (Du's Cl-beta, which at her low
  Reynolds numbers and any angle of attack past 10 degrees is its table's
  corner, -1.01 a radian, times her ground effect's 0.048), `Roll_yaw` and
  `Roll_differential_propwash`. Issue #89 offers no guidance for the rolling
  moment, and standing they did not cause it (Roll_beta 250 to 330 ft-lb as
  she went over, not growing). `Lift_pitch_rate`, a damping term, stays too.
- **The other fifteen models' lift and wing drag are not moved**, nor
  examined; none of them rolls at walking pace in 15 kt across (the most is
  the F-15C's 1.7 degrees, flown at 4.5 kt in the build only).
- **The walking-pace tests still fly only 3 kt** of walking pace's 2-5:
  handed over at 2 kt the Cub rolls at 2-5 kt for 0.03 s against the 0.4 s
  they require, so flying them at the range needs them reshaped - not done;
  the walking-pace item says so. The Cub is flown at the range in her own
  test (below).
- **A real Cub can have a wing picked up by a wind across.** Her Owner's
  Manual (Taxiing, page 40, as copied at stpeteair.org): "When ground winds
  are in excess of 15 M.P.H., turn into wind using ailerons in direction of
  turn; apply ailerons away from the turn when turning downwind. This
  procedure helps to prevent the wind 'picking up' a wing during windy,
  gusty conditions." And the NTSB's NYC96LA043 (Westerly, Rhode Island,
  1995): a J3F taxiing in 320 at 15 gusting 20 kt, "the wind caught the
  right wing, lifting the wing into the air", and a later gust put her on
  her back. That is gusts and ailerons left wrong; a steady 15 kt across
  with her wings held level is not, and the test flies the latter. The
  rollout holds her wings level with the ailerons, not into the wind: the
  test judges the model, and with the model fixed she stays on her wheels
  without it.

**Why she went over, on the real output.** A per-step trace (linux-debug,
not committed) of the 4.5 kt crosswind walking-pace run, every aerodynamic
roll term, the force-at-lever part of the aerodynamic rolling moment, the
gear's moment, the wind-axis forces and each main wheel's compression.
Handed over at 4.3 kt in 15 kt from the left, her autobrake stopped her in
1.8 s and she stood at taxi power (0.37). As she slowed her angle of attack,
atan2(w, u), went from 21 degrees to 45 and her sideslip to -80; her
upwind (left) wheel's compression fell from 0.014 ft to nothing 2 s after
the hand-over, and over the next 2.4 s she rolled from 5 degrees to 21 and
struck her airframe. Through it, by source:
- `Drag_basic` - her wing section's drag against the angle of attack, a
  flat plate's 1.41 at 90 degrees - on the whole dynamic pressure grew from
  33 lb to 155, as each degree she leaned away from the wind gave the beam
  wind a w and took her angle of attack on towards 90 (81 degrees as she
  struck). Her drag in all, the force along the wind and so nearly all of
  it sideways, grew 171 lb to 274, at her wing six feet above her wheels: a
  lean that fed itself.
- `Drag_beta` held at 111 to 146 lb; `Roll_beta` at 250 to 330 ft-lb, not
  growing; `Roll_differential_propwash` 13 to 40; the ailerons, the
  rollout's wings-level law, -80 to -470 ft-lb against her; her lift fell
  from 187 lb to nothing as her angle of attack passed the stall's.
So what rolled her was her wing's drag keyed to an angle of attack that a
beam wind does not define, on a dynamic pressure that counted the beam wind:
the alpha-rate and Cm-alpha faults (#89; the entries below) a third time.
**The unexplained 21.1 degrees** (her lift moved alone, the entry below):
without her lift's ~100 lb she stood a degree more nose-up at 3 kt (13.4
against 12.0), her angle of attack 47 degrees against 42, `Drag_basic` 89 lb
against 79, and its feedback took her over - at every pace from 2 to 5 kt.
**Her propwash pitch term** likewise (#203's finding) moved her pitch, and so
where she stood on the same edge.

**Variants, flown in the build's copy** (the Cub alone, the crosswind
walking-pace run, at 2, 2.5, 3, 3.5, 4, 4.5 and 5 kt): as on main, 5.3 to
5.4 degrees at 2-3 kt and struck at 3.5-5 kt; `Drag_basic` alone moved, 4.1
to 4.3 degrees at every pace; her lift alone moved, struck at every pace;
both, 4.1 to 4.2 at every pace; both with `Lift_pitch_rate` too, the same.

**The change.** tools/make_j3cub.py names `Lift_alpha` and `Lift_propwash`
(LIFT) and `Drag_basic` (DRAG) to `alpha_rate.with_alpha_terms_on_qbar_uw`,
beside her two pitch terms: each on `aero/qbarUW-psf`. Its docstring says
why each, and why not her roll terms or `Lift_pitch_rate`. The lift is
issue #89's and the FlightGear wiki's guidance ("Using QBarUW is more
appropriate for CL than the generic QBar"); the wing's drag at its angle of
attack is the same wing met by the same air - the sideways part runs along
its span - and the guidance does not name it. tools/alpha_rate.py is
unchanged. FlightGear's own J3Cub (FGMEMBERS/J3Cub, J3Cub.xml) has the same
aerodynamics on `qbar-psf` throughout.

**The tests.**
`the_j3_cub_held_at_every_walking_pace_in_a_15_kt_crosswind_stays_on_her_wheels`
(tests/unit/test_lesson.cpp), new: at her model's own loading and her
figures' three, with 15 kt straight across from the left and from the right,
rolled up to every walking pace from 2 to 5 kt by half a knot and handed to
the rollout, held there at taxi power for half a minute as the walking-pace
tests hold her, then let go: nothing but her wheels touches the runway, her
wings within 15 degrees, not gone over, and stopped. Coverage asserted: 56
runs of 4 x 2 x 7, all rolled up and judged. Her most roll in any run 4.4
degrees. **Seen red** with main's Cub in the build's copy: 22 of 56 wrong -
her model's own from the left struck at 3.5 to 5 kt and from the right at 3
to 5 kt, solo from the left at 3.5 and 4 kt, each at 20.1 to 21.2 degrees of
roll; the rest leaned 4.5 to 8.0 degrees. Restored, green.
`the_j3_cub_flown_stick_central_in_a_15_kt_crosswind_keeps_her_tail_wheel_on_the_runway_wherever_she_does_in_calm_air`
(test_departure.cpp): the three runs named for her lift are judged now - 26
crosswind runs held to her tail down, 34 to calm air's outcome, asserted.
All three keep her tail down. **Seen red** with main's Cub: exactly those
three failed (gross from the right at 0.5 her tail up at 7.9 kt; solo from
either side at 0.5 gone over, where in calm air her tail only lifts).
Restored, green.
`every_landplane_rolling_at_walking_pace_in_a_15_kt_crosswind_...`: the Cub
is no longer named there for the rolling ceiling. On main her autobrake
reaches 0.160 at most rolling in 15 kt across (named as 0.21, an older
hand's); now it reaches the ceiling, 0.300 (her lean 4.2 degrees, her turn
0.1, where on main she leans 5.4 and turns 1.0), and the ceiling is judged
on her. **Seen red** with main's Cub: the test fails on her alone, "reached
only 0.159588 rolling". Restored, green. Dry and wet she is still named
(0.291 rolling, dry).

**Her figures**: every Cub figure test (16 in the test binary: static rpm,
rated power, climb, cruise, glide, stall, the ceiling, the orbits, the
heading holds in 20 kt across, the height holds, the hand-overs after
lift-off) printed the same output byte for byte with main's model and the
new one; the measured landing stops 399 m past the threshold with both.

**Verification.** linux-debug: the 61 tests of the walking pace, the tail
wheels, the crosswind stops and brake sweep, the Cub's figures and both Cub
crosswind tests, the light aeroplanes' heading holds and crosswind landings,
the steady-wind rollouts, the committed models and the alpha-rate check
pass. The same 61 pass in a `-march=x86-64-v2` debug build (no fused
multiply-adds, as CI builds) and in linux-release, the new test's most roll
4.4 degrees in each (4.2 to 4.4 by run). The selftest hash does not move:
it flies the C172P.

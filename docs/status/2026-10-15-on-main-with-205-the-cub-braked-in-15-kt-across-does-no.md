### On main with #205: the Cub braked in 15 kt across does no worse than calm air, and the crosswind pitch item is done, 2026-10-15 — item done

**What is still not true first.**
- **"Every other model's" term is read as each model's pitching moment
  keyed to the angle of attack itself** (her Cm-alpha, as tables or
  factors): all nineteen are on `qbarUW`. Five terms that read the angle
  only to pick a coefficient are left on `qbar` and named with their reasons
  (the entry below): the C182's `CmDe`, the F-22A's `CmDh` increment,
  `CmDtv` and `Cmq`, and the S.23's `Cmq`.
- **"Her tail stays down" is held where calm air's does**: braked stick
  central from 10 kt at half brake and more she goes over in calm air too
  (her brakes' grip near her tip ratio), and in 15 kt across she does no
  worse. Whether a real Cub would is the open item "The J-3 Cub braked stick
  central from 10 kt noses over in calm air".
- Everything the entry below names as not true still is, but its first
  point: the Cub's lift is now on `qbarUW` (#205).

**Rebased onto main after #205** (the Cub's `Lift_alpha`, `Lift_propwash`
and `Drag_basic` on `qbarUW`). Nothing clashes: every make script run again
on the combined tree writes the committed models unchanged, the Cub's among
them, with this branch's `alpha_rate.py`; this branch's check reads only
the PITCH axis. **On the combined tree, linux-debug**: 242 tests - all 148
of test_figures.cpp (each prints the same figure as on this branch before
the rebase, every one in range), every crosswind heading hold, landing,
take-off, wet-runway, walking-pace, tail-wheel, braked-stop and standing
test, every learnt landing and every Cub test - pass, and the 30 committed-
model, term-check and selftest tests (hash `ba8e97281095464f`). **The Cub's
departure crosswind test**,
`the_j3_cub_flown_stick_central_in_a_15_kt_crosswind_keeps_her_tail_wheel_on_the_runway_wherever_she_does_in_calm_air`,
passes with none of her runs named: standing and run up her tail stays
down in every wind, and braking in 15 kt across no run does worse than calm
air's at the same loading and level. In the `-march=x86-64-v2` debug build
the 17 Cub, walking-pace, tail-wheel and braked-stop tests pass too.

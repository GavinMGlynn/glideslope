### Lift not keyed to the angle of attack leaves out the crosswind, 2026-10-23 - item done

**What is still not true first.**
- **Her pitching moments not keyed to the angle of attack still count the
  crosswind standing**: the elevators', flaps' and pitch damping's moments
  (CmDe, Cmdf, Cmq and their kin) are on the whole dynamic pressure, as
  their lift was. Unmeasured. A new Phase 9 item. The Mosquito's elevator
  shares its pressure with her elevator's lift; only the lift is moved (below).
- **Swept surfaces are moved on an argument, not a measurement**: simple sweep
  theory and thin aerofoil theory of a hinged flap (tools/alpha_rate.py,
  `with_unkeyed_terms_on_qbar_uw`), not a swept wing's flap measured standing
  in a beam wind.
- **The J-3 Cub at idle on half brake with her stick full forward goes over on
  her nose and her flight goes to NaN** (a JSBSim table asserting on a NaN
  height) - seen while writing the test below, which now stands every
  aircraft with her engines stopped. Her going over is the owner's decision
  of 2026-10-20 (Phase 9, "braked stick central ... noses over"); her flight
  going NaN once over is not new and not this item's.
- **The learnt landing's policy flies a C172P it was not trained on, again**:
  her flaps', elevator's and pitch-rate lift moved under it (below). A new
  Phase 9 item.

**The change.** Every make script names its lift terms not keyed to the angle
of attack to a new `alpha_rate.with_unkeyed_terms_on_qbar_uw` (axis LIFT),
which puts each on `aero/qbarUW-psf` and fails if one is missing, reads the
angle of attack or its rate, or has no `aero/qbar-psf`. Each docstring says
which, and what standing in 15 kt across did before and after. 38 terms:

| model | moved |
|---|---|
| 737-300 | dCLflap, CLde |
| 747-400 | dCLflap, dCLsb, CLde |
| 787-8 | Lift_flap, Lift_speedbrake, Lift_elevator |
| A320 | CLDe |
| A380 | CLq, CLde |
| B-2A | CLde |
| C172P | CLq, CLDf, CLDe |
| C182 | CLq, CLo, CLDf, CLDe |
| F-15C | CLq, CLDe |
| F-22A | dCLflap, dCLsb, CLde |
| F-35B | CLde, CLdroop |
| J-3 Cub | Lift_elevator |
| Learjet 35A | CLq, CLde, CLstab, CLspoilers |
| Mosquito | lift-elevator, lift-pitch-rate |
| PA-28 | CLDf, CLDe |
| S.23 | CLdF3L, CLdF3R, CLDe |

**And lift with pitch rate keyed to the angle**, named in the 2026-10-16
item ("the angle of attack only picks the coefficient of her pitch rate"),
now moved by `with_alpha_terms_on_qbar_uw`: the F-22A's, PA-28's and S.23's
CLq and the Cub's Lift_pitch_rate. Named they left her lift with pitch rate
counting the crosswind: with every other term moved, standing in 15 kt
across an A380 rocking at a tenth of a degree a second was still lifted
26 lb by her CLq, an F-22A 25, a PA-28 3.2 (0.0018 of her weight, over the
bound below: seen red). **Nothing is named now** in either lift check.

**The Mosquito's elevator lift** was on `aero/function/qbar-tail-psf`, 0.6 of
the whole pressure and 0.4 of her slipstreams' (u alone). That pressure is
her elevator's pitching moment's too, which is not this item's, so her
lift-elevator alone moves onto a copy, `aero/function/qbarUW-tail-psf`, its
0.6 on `aero/qbarUW-psf` (make_mosquito_fb6.py,
`with_elevator_lift_on_qbar_uw`).

**The sources.** JSBSim issue #89 (Coconnier) and the FlightGear wiki's
"JSBSim Aerodynamics", "for unswept wings, using QBarUW is more appropriate
for CL than the generic QBar": the whole of CL, not its part at the angle of
attack - and the FlightGear C172P, which put only CLadot on it. **Swept
surfaces**, where issue #89 did not look: by simple sweep theory (NACA Report
863) a beam wind meets each half at V sin L normal to its edge, one half
leading edge first, the other trailing edge first. On the first, a
deflection's lift is tan^2 L / 2 of its figure on the whole pressure; on the
second a hinged trailing-edge surface is a drooped nose, which thin aerofoil
theory of a hinged flap (Glauert, ARC R&M 1095, its integral taken over the
nose) gives 0.077 against 0.661 radian of zero-lift angle a radian for a 30%
chord surface, and of the opposite sign; an all-moving tail met backwards is
the opposite incidence. So standing such a term is under half its whole-
pressure figure at every sweep here (45 degrees or less) - nothing, on
`aero/qbarUW-psf`, is the nearer. A spoiler takes away lift the wing makes,
and standing the wing makes none. Lift with pitch rate is the tail's lift at
q l / V in the same air, of order V_uw q, near nothing standing; in flight
the truth is cos b of the whole and `aero/qbarUW-psf` cos^2 b, under 0.4%
apart at 5 degrees. Every other term, in flight, differs by cos^2 b: under
1% at 5 degrees.

**Standing in 15 kt straight across** (linux-debug; the new test below, on
main's models and on these): on her brakes, engines stopped, a lifting
control set one at a time, the most lift either way over twenty seconds:

| model | main's, lb (setting) | now, lb |
|---|---|---|
| 737-300 | -911 (flaps down) | -0.01 |
| 747-400 | -6,451 (flaps down) | -0.06 |
| 787-8 | -2,240 (flaps down) | 0.00 |
| A320 | -122 (stick forward) | 0.00 |
| A380 | +1,243 (stick back) | +0.03 |
| B-2A | +1,043 (stick back) | +0.01 |
| C172P | -61 (flaps down) | -0.01 |
| C182 | -110 (flaps down; -42 to -46 neutral) | -0.02 |
| F-15C | +63 (stick back) | -0.06 |
| F-22A | -551 (speedbrake out; -260 to -282 neutral) | +0.01 |
| F-35B | +37 (stick back) | 0.00 |
| J-3 Cub | +28 (stick back, as she stands) | -0.01 |
| Learjet 35A | +29 (speedbrake out) | 0.00 |
| Mosquito | +25 (stick back, as she stands) | 0.00 |
| PA-28 | -78 (flaps down) | 0.00 |

What remains is her terms keyed to the angle of attack on the creep of u and
w. **The S.23** is not stood: afloat she has no brakes and turns into the
wind, so it is not across her; her terms are moved as every other's.

**The tests.**
- `every_landplane_standing_on_her_brakes_in_15_kt_across_with_any_lifting_control_set_is_lifted_by_no_more_than_a_ten_thousandth_of_her_weight`
  (tests/unit/test_departure.cpp): every landplane, seven settings (neutral,
  flaps down, stick back, stick forward, speedbrake out, trim nose up, trim
  nose down), settled in calm air until every surface has stopped, then 10 s
  from her left and 10 from her right, judged at every step. Bound a
  ten-thousandth of her weight; the worst now is 0.000008 (the C182 and the
  Cub). Coverage asserted: fifteen landplanes, seven settings, 105 runs of
  2,400 steps; the flying boat named. **Seen red**: on main's models 86 of the
  105 runs fail, every aircraft among them; with lift with pitch rate still
  named, the PA-28 fails (3.2 lb against 0.18).
- `every_flight_models_lift_terms_not_keyed_to_the_angle_of_attack_leave_out_the_crosswind_or_are_named_with_their_reason`
  (tests/tools/unkeyed_lift_terms.py): in every model's LIFT axis, every term
  naming a dynamic pressure that does not read the angle of attack or
  multiply its rate is moved or named; sixteen aircraft, 38 moved, 0 named,
  asserted. **Seen red**: on main's models (0 of 32 then); a named term
  moved (the C182's CLq, while named); a term renamed (the F-22A's dCLsb:
  unlisted, and the listed one missing); the Mosquito's qbarUW-tail-psf put
  back on the whole pressure.
- `every_flight_models_angle_of_attack_lift_terms_..._named_with_their_reason`
  now lists 22 moved, 0 named.

**The selftest hash moves**, deliberately: it flies a C172P, whose CLDf, CLDe
and CLq are now on `aero/qbarUW-psf`. `831a4e7954233b0a` ->
`bae579e2555aabce` in linux-debug; its printed flight (roll -0.78, pitch
0.92, heading 347.03, 105.76 KCAS, 21.5 ft/min, 2444 RPM) is the same to
every printed digit.

**The learnt landing's parity fixture, flown again** with
`tools/rl/export.py --parity-only` (its header's `model` line now the sha256
of this c172p.xml, b24cfbb5...d1e5). She flies her approach slipped, so her
flaps', elevator's and pitch-rate lift are cos^2 of it less. Against the
fixture's old flights, from the 27 verification starts (new less old):
**calm air**, 7 to 15 ft/min less sink (now 70 to 74), 0.84 to 0.89 m more
across (now -1.10 to -1.20), 66 to 69 m shorter (now 104 to 115 m along);
**10 kt from one side**, -20 to +30 ft/min (now 63 to 121), -0.46 to +0.19
m across (now 0.13 to 1.09), 39 to 48 m shorter (now 129 to 150); **10 kt
from the other**, 7 to 9 ft/min less sink (now 75 to 85), 0.53 to 0.56 m
more across (now -0.92 to -0.95), 54 to 57 m shorter (now 147 to 155). All
within her limits (5 m, 300 ft/min). **The policy is not trained again**:
it was trained on her as she was on 2026-10-20 (parts 8 to 10), and now
flies a C172P a little different - a new Phase 9 item.

**Verification.** Locally (linux-debug): the new standing test, red on
main's models and green on these; the four term checks (lift keyed to the
angle and not, drag, pitch, alpha rate) green; every make script writes the
committed models unchanged; the selftest above. The 170 figures in
test_figures.cpp and the crosswind, standing, braked, walking-pace and
learnt-landing tests: see below, and CI.

### Pitching moments not keyed to the angle of attack leave out the crosswind, 2026-10-28 - item done

**What is still not true first.**
- **Her side force standing in a beam wind is turned into her plane of
  symmetry**, a new Phase 9 item, found while measuring this one. Her side
  force (CYb) is on the whole dynamic pressure, as it should be, but JSBSim
  makes it along its wind axes' Y, and at a sideslip of 90 degrees that axis
  lies in her plane of symmetry at the angle of attack - which standing is
  undefined. Standing in 15 kt straight across a 737-300 on her brakes has
  her 1,400 lb of side force along her body's x axis, fore or aft as the
  angle falls, and its moment about her centre of gravity (her reference
  point is 5.3 ft above it) is 6,800 ft-lb; a PA-28's swings between -63
  and +44 ft-lb as her creep turns it. Unmeasured across the fleet. Not this
  item's: the test below judges the moment of her PITCH axis, not that.
  **It puts the J-3 Cub over**: braked stick central from 10 kt at half
  brake, solo, in 15 kt across from either side, she goes over where in
  calm air she lifts her tail and stops on her wheels. Her pitch damping on
  the whole dynamic pressure had held her (with it put back, she passes);
  with her side force zeroed and her damping moved, she lifts her tail only,
  as in calm air. The two runs are named in
  `the_j3_cub_flown_stick_central_in_a_15_kt_crosswind_keeps_her_tail_wheel_on_the_runway_wherever_she_does_in_calm_air`,
  each required still to be worse than calm air, so that the fix is
  noticed; seen red both ways (her damping put back, her side force
  zeroed).
- **Swept surfaces are moved on an argument, not a measurement**, as their
  lift was: simple sweep theory and thin aerofoil theory of a hinged flap
  (tools/alpha_rate.py, `with_unkeyed_terms_on_qbar_uw`), not a swept wing's
  flap measured standing in a beam wind.
- **The learnt landing's policy flies a C172P it was not trained on**, a
  little more (below): a fidelity item in Phase 10b already (owner,
  2026-10-24).

**The change.** Every make script names its pitching-moment terms not keyed
to the angle of attack to `alpha_rate.with_unkeyed_terms_on_qbar_uw` (axis
PITCH), which puts each on `aero/qbarUW-psf`; its docstring now says why for
pitching moments. The five pitching terms keyed to the angle that the
2026-10-15 check named - because the angle only picks their coefficient -
move by `with_alpha_terms_on_qbar_uw`: the C182's CmDe, the F-22A's CmDh,
CmDtv and Cmq, the S.23's Cmq. **Nothing is named now** in either pitch
check. 47 terms moved and 1 already off the crosswind:

| model | not keyed to the angle | keyed, named until now |
|---|---|---|
| 737-300 | Cmde, Cmq | |
| 747-400 | Cmde, Cmq | |
| 787-8 | Pitch_elevator, Pitch_damp | |
| A320 | Cmo, CmDe, Cmq | |
| A380 | Cm0, Cmq, Cmde, Cmflaps | |
| B-2A | Cmq, Cmde | |
| C172P | Cmo, Cmq, Cmdf, Cmde (her tailplane's pressure) | |
| C182 | Cmo, Cmq, CmDf | CmDe |
| F-15C | CmM, Cmq, Cmde | |
| F-22A | (none) | CmDh, CmDtv, Cmq |
| F-35B | Cmq, Cmde, Cmdroop | |
| J-3 Cub | Pitch_damp; Pitch_elevator already off it | |
| Learjet 35A | Cm0, Cmq, Cmde, Cmstab, Cmflaps | |
| Mosquito | pitch-zero, -damping, -flaps, -radiators, -gear, -elevator (her tailplane's pressure) | |
| PA-28 | Cmo, Cmq, CmDf, Cmde | |
| S.23 | CmDe, CmDf3L, CmDf3R | Cmq |

**Her tailplane's pressure.** The C172P's elevator moment was on
`aero/function/qbar-tail-psf` (2026-10-16): the whole pressure plus 0.39 of
her slipstream's excess over it. It is made now, as
`aero/function/qbarUW-tail-psf`, from `aero/qbarUW-psf` in both places
(make_c172p.py). The Mosquito's was 0.6 of the whole pressure and 0.4 of her
slipstreams'; her lift-elevator was moved onto a copy of it on 2026-10-23,
and her pitch-elevator, the only other term on it, now joins it, so the
copy is made in place of the first (make_mosquito_fb6.py,
`with_elevator_on_qbar_uw`). **The J-3 Cub's Pitch_elevator** is on
`aero/function/qbar-induced-psf`, her slipstream's pressure of u and her
propeller's induced velocity alone: already off the crosswind, and checked
so. Her fin's pressures, the C172P's `qbar-fin-psf` among them, are yaw's,
and the crosswind is theirs.

**The F-22A's CmDtv**, her thrust vectoring's moment, is a coefficient on the
dynamic pressure times her thrust setting, not her engines' thrust: standing
in calm air it is nothing on either pressure, and on `aero/qbarUW-psf` it is
nothing standing in a crosswind too.

**The sources.** JSBSim issue #89 (Coconnier) moved only Cmadot, and says
swept wings "might need a different treatment that we have not investigated
further"; the FlightGear wiki's "JSBSim Aerodynamics", written from it
(dany93, 2019): "Using QBarUW is more appropriate for Cm than the generic
QBar", at least for unswept wings. A later revision of that page calls the
same pressure QBarUV and swaps the two names; JSBSim's `aero/qbarUW-psf` is
of u and w, the plane of lift, whatever the wiki calls it. A pitching term not
keyed to the angle of attack is the moment of a lift moved on 2026-10-23 - an
elevator's, a flap's, a stabiliser's, a wing's at no angle, the tail's with
pitch rate - or of the Mosquito's radiators' and gear's drag: her radiators'
flow runs through ducts along her chord, and her gear legs' drag in a beam
wind is sideways, which rolls rather than pitches her. Swept surfaces are
moved on the argument their lift was (alpha_rate.py).

**Standing in 15 kt straight across** (linux-debug; the new test below, on
main's models and on these): on her brakes, engines stopped, each of seven
settings set one at a time, the most pitching moment of her PITCH axis
either way over twenty seconds:

| model | main's, ft-lb (setting) | neutral, main's | now, ft-lb |
|---|---|---|---|
| 737-300 | -4,045 (stick forward) | -130 to +83 | 0.51 |
| 747-400 | +53,063 (stick back) | -113 to +172 | 2.10 |
| 787-8 | -46,968 (stick forward) | -161 to +131 | 0.79 |
| A320 | -12,855 (stick forward) | +509 to +614 | 0.14 |
| A380 | +199,166 (stick back) | +2,521 to +9,329 | 20.6 |
| B-2A | -26,076 (stick forward) | -38 to +41 | 0.17 |
| C172P | +287 (stick back) | +59 to +69 | 0.05 |
| C182 | +334 (stick back) | -7 to +59 | 0.09 |
| F-15C | +1,383 (stick back) | -10 to +17 | 0.51 |
| F-22A | +13,785 (stick back) | -62 to +67 | 0.13 |
| F-35B | -753 (stick forward) | -1.6 to +2.5 | 0.00 |
| J-3 Cub | +5.0 (neutral) | -1.9 to +5.0 | 0.16 |
| Learjet 35A | +475 (stick back) | +85 to +92 | 0.01 |
| Mosquito | +782 (flaps down) | +566 to +587 | 0.02 |
| PA-28 | +517 (stick back) | +40 to +48 | 0.01 |

What remains is her terms on the creep of u and w, the most the J-3 Cub's
as the wind changes side and the A380's, 0.0044 of her bound. **The S.23** is
not stood: afloat she has no brakes and turns into the wind; her terms are
moved as every other's.

**The tests.**
- `every_landplane_standing_on_her_brakes_in_15_kt_across_with_any_control_set_is_pitched_by_her_pitching_moment_terms_by_no_more_than_a_ten_thousandth_of_her_weight_times_her_chord`
  (tests/unit/test_departure.cpp): the lift test's scenario, now shared
  (`stand_each_landplane_across`): every landplane, seven settings, settled
  in calm air, then 10 s from her left and 10 from her right, judged at every
  step. What is judged is the moment of her PITCH axis about her reference
  point: `moments/m-aero-lbsft` less the moment of her aerodynamic forces
  carried from that point to her centre of gravity (JSBSim's
  FGAerodynamics, M = r x F); with that left in, the side force above fails
  it on every airliner whatever her pitching terms do. Bound a ten-thousandth
  of her weight times her mean chord; the worst now 0.32 of it (the Cub); a
  hundred-thousandth, tried first, the Cub failed by three times. Coverage
  asserted: fifteen landplanes, seven settings, 105 runs of 2,400 steps; the
  flying boat named. **Seen red**: on main's models 86 of the 105 runs fail,
  every aircraft among them.
- The lift standing test is the same test on `forces/fwz-aero-lbs`, its
  scenario moved into the shared function unchanged; passes as before.
- `every_flight_models_pitch_terms_not_keyed_to_the_angle_of_attack_leave_out_the_crosswind_or_are_named_with_their_reason`
  (tests/tools/unkeyed_pitch_terms.py): in every model's PITCH axis, every
  term naming a dynamic pressure that does not read the angle of attack or
  multiply its rate is on a pressure without the crosswind or named;
  sixteen aircraft, 48 terms, 0 named, asserted. A pressure the model makes
  (the tailplanes', the Cub's slipstream's) is checked not to read the
  whole pressure or any velocity but u and w. **Seen red**: on main's models
  (1 of 48, the Cub's elevator); the C172P's Cmq put back on the whole
  pressure; the Learjet's Cmstab renamed; the Mosquito's qbarUW-tail-psf
  put back on the whole pressure; the Cub's induced velocity made from vt;
  the PA-28 removed.
- `every_flight_models_angle_of_attack_pitch_terms_..._named_with_their_reason`
  (tests/tools/alpha_pitch_terms.py) now lists 25 moved, 0 named; red on
  main's models (20 of 25).

**The selftest hash moves**, deliberately: it flies a C172P, whose Cmo, Cmq,
Cmdf and Cmde are now on the pressure without the crosswind.
`bae579e2555aabce` -> `7bb59e64053c4b4f` in linux-debug; its printed flight
(roll -0.78, pitch 0.92, heading 347.03, 105.76 KCAS, 21.5 ft/min, 2444 RPM)
is the same to every printed digit, its end's longitude 151.2304407 ->
151.2304406.

**The learnt landing's parity fixture, flown again** with
`tools/rl/export.py --parity-only` (its header's model line now the sha256
of this c172p.xml, 02cc3ddb...3559). Against its flights on main's model,
from the 27 verification starts (new less old): **calm air**, 2.3 to 3.7
ft/min more sink (now 74 to 76), 0.00 to 0.03 m across (now -1.10 to -1.17),
2.1 to 2.7 m longer (now 107 to 117 m along); **10 kt from one side**, -2.9 to
+8.4 ft/min (now 71 to 118), -0.28 to -0.08 m across (now 0.05 to 0.81),
0.2 to 3.9 m shorter (now 127 to 147); **from the other**, -1.1 to +1.7
ft/min (now 77 to 84), -0.04 to -0.01 m (now -0.93 to -0.98), 1.5 to 1.9 m
shorter (now 145 to 154). All within her limits (5 m, 300 ft/min). The
policy is not trained again (Phase 10b).

**Verification.** Locally (linux-debug, `ctest -j4`, no display): the new
standing test, red on main's models and green on these; 213 tests - the
figure tests in test_figures.cpp (161 registered), every crosswind, wind,
standing, braked, walking-pace and learnt-landing test (the server's and
the client's among them), the term checks (pitch and lift, keyed to the
angle and not, drag, alpha rate), every committed-model test and the
selftest's - all pass but two skipped (the Vulkan sky, the server's
window), the J-3 Cub's crosswind braking test once its two runs were named
(above). Every figure in range. Every make script's `--check` passes
(sixteen models).

**The figures, main's models against these** (linux-debug, the figure tests
of test_figures.cpp run on each, 2026-10-28; all pass both ways).
Of the 127 figures printed with their range, 11 move, none by more than 1.2%
of its range. The margin is the distance to the nearer end of the range, as a
share of its width:

| figure | main's | now | margin, main's -> now |
|---|---|---|---|
| S.23 water take-off run, 45,000 lb | 718.3 yd | 715.5 yd (676 to 914) | 17.8% -> 16.7% |
| S.23 water take-off time, standard | 25.30 s | 25.35 s (21 to 27) | 28.3% -> 27.5% |
| S.23 water take-off time, 45,000 lb | 31.23 s | 31.16 s (25.9 to 35.1) | 42.1% -> 42.8% |
| Learjet 35A take-off field length | 5,374.8 ft | 5,385.2 ft (4,770 to 5,830) | 42.9% -> 42.0% |
| 787-8 take-off field length | 10,902.1 ft | 10,907.2 ft (9,090 to 11,110) | 10.3% -> 10.0% |
| 747-400 take-off field length | 11,275.0 ft | 11,278.8 ft (9,450 to 11,550) | 13.1% -> 12.9% |
| Mosquito take-off over 50 ft | 2,492.38 ft | 2,492.21 ft (2,147 to 2,624) | 27.5% -> 27.5% |
| 787-8 climb, one engine out | 2.573% | 2.572% (2.4 to 10) | 2.3% -> 2.3% |

The other three (the A320's and 737-300's field lengths, the Mosquito's
single-engine ceiling) move in the third decimal or less. Why each moves
is not measured term by term: in flight the two pressures differ by the
cosine squared of her sideslip, and the S.23's run is the most moved.

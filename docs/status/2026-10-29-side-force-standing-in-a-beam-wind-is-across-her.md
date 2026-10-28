### Her side force standing in a beam wind is across her, 2026-10-29 - item done

**What is still not true first.**
- **The share left out of her side force is argued, not measured.** The
  share is sin^2 of her sideslip. Its shape is the crossflow's (below). No
  wind-tunnel side force between 10 and 80 degrees of sideslip is matched.
  Only the J-3 Cub's push standing in a beam wind is a cited figure.
- **What remains in her plane of symmetry standing is her drag.** Her drag
  acts along the relative wind, which her creep turns a few degrees off her
  beam. The most is the F-15C's, with her stick back: 18.4 lb, 0.55 of the
  new bound. Whether a real F-15C's drag in a beam wind is that is not
  measured.
- **The S.23 has no drag with sideslip.** Her side force now goes to nothing
  at 90 degrees of sideslip, as every model's does, and nothing else pushes
  her flank there. She is a flying boat, and afloat she turns into the wind.
  She is named in the check.
- **The J-3 Cub still noses over in calm air**, braked stick central from 10
  kt at half brake and more. That is Phase 9's other item and is unchanged.
  Braked in a crosswind she is now no worse than in calm air at every level.
  No other cause puts her over, so there is no tail.

**The cause.** Every model gives her forces as DRAG, SIDE and LIFT. JSBSim
takes these in its wind axes and turns them into her body's with Tw2b
(FGAerodynamics.cpp, FGAuxiliary.cpp). In her body, the wind axes' y lies
along (-cos a sin b, cos b, -sin a sin b). At b = 90 degrees, standing in a
beam wind, that direction is in her plane of symmetry, at an angle of attack
that is undefined there and turned by her creep. The FlightGear wiki's
"JSBSim Aerodynamics" says the lift/drag/side split breaks down with her x
axis across the relative wind. No JSBSim issue was found on it.

**What her side force is, there.** Each model's side force is a slope, CYb
times b, carried on to 90 degrees. Of the force on a body at incidence b,
the crossflow's part goes with sin^2 b and lies along the flow across it
(Allen and Perkins, NACA Report 1048, 1951). Standing in a beam wind, her
flank meets the air broadside, and the whole force is crossflow: along the
wind, across her, which is her drag with sideslip. Every landplane has one,
and the J-3 Cub's is a cited figure. Her side force there is that force
counted a second time, in the wrong direction.

**Two fixes tried first, and why each was dropped.** Both put her side
force along her body's y, which needs a term in DRAG as well.
- **All of it across her body.** In flight this adds a drag of F sin b along
  the wind: |CYb| b sin b of the dynamic pressure. For the 747-400 at 5
  degrees of sideslip that is 0.0076, nearly half her drag at no lift
  (0.017). Her take-off field length figure failed: with an engine failed,
  she was not 35 ft up after four minutes. With the drag term alone removed
  she flew it again (11,278.5 ft).
- **A sin^2 b share across her body.** Every figure passed, but the J-3 Cub
  standing in a beam wind was pushed at 0.53 of her dynamic pressure and
  wing area, against her side area's cited 0.32. That failed
  `the_j3_cub_standing_in_a_15_kt_beam_wind_is_pushed_within_0_03_of_her_side_areas_cited_drag`,
  because her flank force was counted twice: once as her drag with sideslip
  and once as her side force carried across her.

**The fix** (tools/side_force.py, `without_crossflow`, applied by every make
script as its last step). One term is added at the end of her SIDE axis,
`aero/force/side-crossflow`: -sin^2 b times F, where F is the sum of her
other side terms, read from their properties after they are worked that
step. The side terms themselves are left as they are. What is left of her
side force is cos^2 b of it:
- at 5 degrees of sideslip, 0.992 of it;
- at 85 degrees, her creep in a beam wind, 0.008, and that much of the old
  leak reaches her plane of symmetry;
- at 90 degrees, none. What pushes her across is her drag with sideslip,
  along the wind.

The fix is in the model files, not in ext/jsbsim. JSBSim cannot mix axis
systems in one model, and moving every model to body axes would move her
lift and drag too.

**Standing in 15 kt straight across** (linux-debug, the two new tests, on
main's models and on these). On her brakes, engines stopped, each of seven
settings, the most over twenty seconds. "Force" is the air's force in her
plane of symmetry. "Moment" is her whole aerodynamic pitching moment about
her centre of gravity.

| model | force, main's (lb) | now (lb) | moment, main's (ft-lb) | now (ft-lb) |
|---|---|---|---|---|
| 737-300 | 1,401 | 2.29 | -7,000 | 2.81 |
| 747-400 | 6,756 | 14.0 | -30,635 | -59.3 |
| 787-8 | 4,188 | 1.10 | 6,334 | -1.30 |
| A320 | 502 | 0.13 | -2,244 | 0.07 |
| A380 | 10,449 | 17.7 | -38,902 | 33.8 |
| B-2A | 625 | 1.40 | -1,060 | 2.28 |
| C172P | 18.1 | 0.21 | -32.6 | -0.11 |
| C182 | 83.9 | 0.21 | 140 | -0.04 |
| F-15C | 613 | 18.4 | -167 | 5.43 |
| F-22A | 1,796 | 0.72 | 4,913 | 1.61 |
| F-35B | 360 | 0.17 | 0.00 | -0.00 |
| J-3 Cub | 28.8 | 0.41 | -56.5 | 0.40 |
| Learjet 35A | 231 | 0.18 | -95.0 | 0.03 |
| Mosquito | 326 | 0.11 | -122 | -0.04 |
| PA-28 | 55.6 | 0.04 | 67.3 | -0.03 |

The S.23 is not stood: afloat she turns into the wind. Her model is changed
as every other's.

**The tests.**
- `every_landplane_standing_on_her_brakes_in_15_kt_across_with_any_control_set_has_her_side_force_across_her_and_no_more_than_a_thousandth_of_her_weight_fore_and_aft_or_up_and_down`
  (tests/unit/test_departure.cpp).
  - It uses the standing scenario of 2026-10-28
    (`stand_each_landplane_across`), and judges at every step the length of
    (forces/fbx-aero-lbs, forces/fbz-aero-lbs).
  - The bound, a thousandth of her weight, was stated after measuring. At
    the lift test's ten-thousandth, the F-15C and the J-3 Cub would fail by
    their drag.
  - Worst now: the F-15C at 0.55 of the bound.
  - **Seen red** on main's models: all fifteen fail. The B-2A fails least,
    at 1.9 times the bound.
- `every_landplane_standing_on_her_brakes_in_15_kt_across_with_any_control_set_is_pitched_by_her_air_about_her_centre_of_gravity_by_no_more_than_a_ten_thousandth_of_her_weight_times_her_chord`
  (tests/unit/test_departure.cpp).
  - It uses the same scenario, and judges moments/m-aero-lbsft whole. The
    2026-10-28 test had to leave out the moment of her forces carried from
    her reference point, because of this fault.
  - The bound is the pitching terms' own: a ten-thousandth of her weight
    times her mean chord.
  - Worst now: the J-3 Cub at 0.82 of the bound, as the wind changes side.
  - **Seen red** on main's models: 13 of the 15 fail. The B-2A (0.87) and
    the F-35B pass both ways; the F-35B's reference point is her centre of
    gravity.
- `every_flight_models_side_force_leaves_out_its_crossflow_and_every_landplane_has_a_drag_with_sideslip`
  (tests/tools/side_force_across.py).
  - Each model must be in wind axes, and must end her SIDE axis with the
    added term exactly as above, over exactly her listed side terms.
  - Each landplane must have her listed drag with sideslip. The S.23 is
    named.
  - Coverage, asserted: sixteen aircraft, 48 side terms, 15 drags.
  - **Seen red** on main's models: all 16 fail.
- `the_j3_cub_flown_stick_central_in_a_15_kt_crosswind_keeps_her_tail_wheel_on_the_runway_wherever_she_does_in_calm_air`:
  **its two named runs come off the list.** Solo at half brake, from the
  left she now keeps her tail down; from the right she lifts it and stops
  on her wheels, as in calm air. `named_runs` is empty, and the test says
  why.
- `the_j3_cub_standing_in_a_15_kt_beam_wind_is_pushed_within_0_03_of_her_side_areas_cited_drag`
  passes: 0.318 against the cited 0.319. It caught the second fix tried
  (above).

**The selftest hash moves**, deliberately: it flies a C172P, whose side
force now leaves out its crossflow. In linux-debug the hash goes from
`7bb59e64053c4b4f` to `27e94c795b47141c`. Its printed flight is the same to
every printed digit (roll -0.78, pitch 0.92, heading 347.03, 105.76 KCAS,
21.5 ft/min, 2444 RPM) except the longitude of its end, 151.2304406 ->
151.2304404.

**The learnt landing's parity fixture, flown again** with
`tools/rl/export.py --parity-only` (model line now d66f0f74...414b). Against
main's model, over the 27 verification starts (new less old):
- **Calm air:** +0.1 to +0.6 ft/min of sink, +0.25 to +0.29 m across, -1.5
  to -1.0 m along.
- **10 kt from one side:** -1.1 to +3.1 ft/min, +0.27 to +0.28 m across,
  -5.2 to -4.6 m along.
- **From the other side:** -0.2 to +0.1 ft/min, +0.20 to +0.22 m across,
  -1.6 to -1.2 m along.

Every flight is within her limits: sink 73 to 117 ft/min against 300, and
1.08 m across at most against 5. The policy is not trained again.

**The figures, main's models against these** (linux-debug, the 161 figure
tests of test_figures.cpp, run on each). All pass both ways. Of the 121
figures printed with their range, 12 move, none by more than 0.28% of its
range:

| figure | main's | now | range |
|---|---|---|---|
| Mosquito safety speed, +9 boost | 147.46 KCAS | 147.40 KCAS | 145 to 165 |
| S.23 water take-off run, 45,000 lb | 715.54 yd | 715.19 yd | 675.75 to 914.25 |
| S.23 water take-off time, standard | 25.358 s | 25.350 s | 21 to 27 |
| F-35B hover, height held | 0.022 ft | 0.011 ft | 0 to 10 |
| S.23 water take-off time, 45,000 lb | 31.158 s | 31.150 s | 25.925 to 35.075 |
| Mosquito safety speed, +18 boost | 174.87 KCAS | 174.86 KCAS | 160 to 180 |
| 747-400 take-off field length | 11,278.82 ft | 11,278.09 ft | 9,450 to 11,550 |
| Mosquito take-off over 50 ft | 2,492.21 ft | 2,492.33 ft | 2,146.5 to 2,623.5 |
| Mosquito single-engine ceiling | 12,479.76 ft | 12,479.87 ft | 10,000 to 14,000 |

The 787-8 and Learjet 35A field lengths and the F-22A's climb at 50,000 ft
move by under 0.002.

**Verification.** Locally (linux-debug, `ctest -j4`, no display):
- On these models, 201 tests pass:
  - the 161 figure tests;
  - the three standing-across tests;
  - the J-3 Cub's crosswind braking test;
  - every term check (alpha rate; angle of attack lift, drag and pitch;
    unkeyed lift and pitch; the side force);
  - every committed-model test;
  - the selftest's tests;
  - the models' own stall and climb tests.
- 95 related tests pass too:
  - every crosswind, wind, standing, braked and walking-pace test;
  - the J-3 Cub's beam-wind and tail-wind tests;
  - every heading-in-a-crosswind test;
  - every learnt-landing test that runs without a window or server;
  - the stall recoveries.
- Every make script's `--check` passes (sixteen models).

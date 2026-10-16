### Every nose-wheel aeroplane strikes its tail where its airframe would, 2026-10-13 — item done

**What is still not true first.** Five of the thirteen strike in the
over-rotation test with their wheels just lifted, not on them, and are held
more loosely there (below). No manufacturer's strike attitude was found to
check the meshes against: the figures quoted below come from forums and a
study card, not from documents. The Learjet's is still from her maintenance
manual's drawing (she has no mesh), and the 747-400 and F-22A still fly no
take-off of their own.

**What the accurate strikes showed, and how it was fixed (2026-10-13).**
With strike attitudes lower than the models had them, four tests went red
on two aeroplanes. All are green now:
- **The A380 at her light loading struck her tail taking off**
  (`every_landplane_takes_off_at_every_loading_within_ten_knots_of_its_speed_for_its_weight_and_unhurt`).
  Traced: the take-off autopilot's pull grew while her nose came up at
  under four degrees a second, and nothing eased it once it came faster, so
  she was rotating at 8.7 degrees a second as her wheels left at 10.3
  degrees. That carried her to 13.8 with her wheels five feet up, and her
  aft fuselage met the runway. Her old model had nothing behind her main
  wheels below 34 degrees, so the overshoot went unseen. `sim::Departure`
  now eases the pull as the nose nears her strike attitude less the
  two-degree margin, which is in every model. On her wheels, whenever the
  nose comes up more than a degree a second faster than a degree a second
  for each degree it is short of that attitude, the pull is taken off at
  half its travel a second (`rotation_rate_degps`, four, caps the rate the
  pull is built towards, as before). **What it measurably does:** she
  leaves the runway sooner and lower - the A380 at 9.3 degrees, not 10.3 -
  and her nose overshoots less after: the F-15C's peaks at 13.7 degrees,
  not 16.2. **It does not hold a rotation rate**: airliners still come up
  at 7 to 8 degrees a second on their wheels. A rate-holding rotation is
  its own item in Phase 10a.
  - **Tried and dropped:** at three degrees a second, with the slowing aimed
    at the 10-degree take-off attitude, the F-15C left 12 kt past her
    rotation speed and the F-35B 28; ten are allowed.
  - **Tried and dropped:** a bound off the runway as well - her pitch held
    below 35 ft to what her wheels' height lets her tail clear. The rate
    alone fixed the A380, and the bound flew a C172P hauled off early at
    45 kt flat in ground effect to her rotation speed before fifteen feet,
    so her lesson no longer named the early rotation
    (`rotating_early_is_caught_for_each_aeroplane_at_its_own_speed`). Easing
    the pull off the ground did the same, which is why it is eased only on
    the wheels.
  The A380 now takes off unhurt at her light loading, 1.3 kt past her
  rotation speed. At every loading, every aeroplane's lift-off is within
  half a knot of what it was; the Learjet's moved most, by 0.4. The
  747-400's and F-22A's measured take-off speeds come out as their files
  say.
- **The F-15C rose 0.61 ft after touching down**, 0.5 allowed
  (`the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief`,
  `every_aeroplane_lands_light_and_heavy_without_a_balloon_a_bounce_or_a_go_around`,
  `the_ai_flies_every_approach_at_the_speed_for_its_figures_loading_and_lands_within_its_limits`).
  Traced: her flare is now held at 13.2 degrees, not 17.2. The lander's
  `nose_out` ("the nose can come up no further, so fly the path on the
  power") read the attitude asked for, which runs ahead of the nose. With
  her nose still at eleven degrees and coming up, it raised her power from a
  third to two thirds. She met the runway with a third still on, sinking
  288 ft/min, and rose off her gear's rebound. On the old model she touched
  at 11.5 degrees and 409 ft/min with no power added, and rose 0.35.
  - **Tried and dropped:** cutting a jet's power in a step at the touch
    changed nothing, because her engines spool down anyway. Keeping power on
    in the last feet made it worse: 0.84 ft.
  - **The fix:** `nose_out` now reads her actual pitch. The F-15C's power
    comes off as before, and the F-35B, whose nose does reach her limit,
    still flies her path on the power.
- **The F-22A's take-off speeds were re-measured** (2026-10-13), as below.
- `tools/align_models.py` counts a tail strike among a model's shape
  contacts again (`each_visual_model_is_where_its_flight_model_says_the_aeroplane_is`
  counts them) but does not measure its distance; the shape counts in
  `alignment.txt` are one or two more for each model with a mesh. That
  test no longer holds a tail strike to the aligned mesh either: it is
  placed from the mesh's own main wheels, so it lies off the aligned mesh by
  as far as the two undercarriages are apart (the F-22A's 3.1 m, which
  turned the test red), and the ground-contact check holds it instead.
  `Aircraft::ContactPoint` now carries the contact's name so the test can
  tell which contact is which.

**What went red first, 2026-10-10, kept for the record.** The four tests
above, and the F-22A's measured speeds:
- **Fixed: the F-22A's take-off speeds, re-measured 2026-10-13.**
  `glideslope_cli takeoff-speeds f22`: rotated at 100 kt she now strikes her
  stabilators (her strike fell from 46.2 to 16.5 degrees), so the first
  rotation that leaves cleanly is 105, and 110 is written where it was 105
  (`assets/figures/f22.xml`). Her climb, flap and weight are unchanged.
  `the_measured_take_off_speeds_in_the_747_and_f22_figures_are_what_the_measurement_makes`
  passes.

Also: five of the thirteen strike with their
wheels just lifted, not on them - the 747-400, 787-8, F-22A and F-35B held
fully back, and the Learjet as before - so their strike in the test is
above their airframe's attitude (by up to 2.4 degrees, the F-22A), held only
to be no lower than it and within three. No manufacturer's strike attitude
was found for any of them to check the meshes against; the figures below
that are quoted are forum or study-card figures, not documents. The
Learjet's is still from her maintenance manual's drawing (no mesh), and
the 747-400 and F-22A still fly no take-off of their own.

**How it is measured.** `tools/ground.py` `tail_strike()` measures, on each
aeroplane's visual mesh, the least pitch at which anything of her airframe
behind her main wheels meets the ground, pivoting on the aft edge of the
mesh's own main tyres - not the flight model's, since a mesh's undercarriage
can sit well away from the model's (the 747-400's 49 in, the B-2A's 90, the
F-22A's 123). The undercarriage itself is anything lower than half the
typical belly height, `_profile()`'s rule. That point is placed on the
flight model at the same offset from its own main wheels, as a scraping
STRUCTURE contact - `TAIL_STRIKE`, or `LEFT_`/`RIGHT_TAIL_STRIKE` mirrored
where it is off the centreline - by `ground.with_tail_strike()`, called by
all twelve `tools/make_*.py` with a mesh. Five contacts that struck before
the drawn airframe are taken out. Four came with the source models and sat
below the drawn airframe: the C172P's tail skid (8 in below her drawn tail;
10.3 degrees against 11.7), the C182's (31 in; 11.3 against 15.8), the
PA-28's (5 in; 14.2 against 16.1), and the 787-8's own `TAIL_STRIKE` (145 in
below the drawn tail cone; 10.5 against 11.5). The fifth is the B-2A's keel
`TAIL`, placed by the alignment on wheels 90 in forward of the mesh's (14.3
against 15.3).

**Measured, and against what is quoted** (mesh; model; the test's strike):

| aeroplane | strikes on | as drawn | model | struck | quoted |
|---|---|---|---|---|---|
| 737-300 | aft fuselage, on the keel | 12.5 | 12.5 | 12.2 | — |
| 747-400 | aft fuselage, on the keel | 12.8 | 12.8 | 13.1, wheels off | — |
| 787-8 | aft fuselage, on the keel | 11.5 | 11.5 | 11.6, wheels off | — |
| A320 | aft fuselage, 28 in off the keel | 12.9 | 12.9 | 12.8 | 11.5-11.7 struts compressed, 13.5 extended (pilots' forums, brainly) |
| A380 | aft lower fuselage, 17 m behind her body gear | 12.3 | 12.3 | 11.9 | 11.5 compressed, 13.5 extended (an A380 limitations study card, Quizlet; not an Airbus document) |
| B-2A | wing trailing-edge tips | 15.3 | 15.2 (wing tip) | 15.1 | — |
| C172P | aft fuselage, on the keel | 11.7 | 11.7 | 11.7 | — |
| C182 | aft fuselage, on the keel | 15.8 | 15.8 | 15.6 | — |
| F-15C | stabilator tips | 15.2 | 15.2 | 14.9 | — |
| F-22A | stabilator tips | 16.5 | 16.5 | 18.9, wheels off | — |
| F-35B | tail surfaces, 38 in off the keel | 21.6 | 21.6 | 23.0, wheels off | — |
| PA-28 | aft fuselage, on the keel | 16.1 | 16.1 | 15.8 | — |
| Learjet 35A | ventral fin (drawing) | 14.0 | 14.0 | 14.3, wheels off | — |

Airbus's A380 Aircraft Characteristics gives no rotation clearance (the
make script's comment, 2-3-0), and the owner found no public source
(2026-10-09). The A380 strikes her aft fuselage at 12.3, not her outboard
engines at 34: the engines are ahead of her main wheels and were never what
she struck; her old model's only contacts behind them were her wing tips.
Of the commonly quoted figures, both Airbus ones bracket the mesh's.

**Tests.**
- `the_airframe_contacts_measured_from_the_meshes_agree_with_what_is_published`
  (`ground.py --check`) now also holds every one of the sixteen flight
  models: the twelve nose-wheel aeroplanes with a mesh must strike within a
  quarter of a degree of their mesh, and the four without (the Learjet, from
  her drawing; the Cub and the Mosquito on tail wheels; the Short S.23) are
  named; it fails unless that is twelve and four of sixteen. **Seen to
  fail** on the models as they were: all twelve, from the 737-300 0.3 degrees
  early to the F-22A's 46.2 against 16.5.
- `an_over_rotated_take_off_strikes_the_tail_and_its_debrief_says_so` now
  puts all thirteen nose-wheel aeroplanes to it, not nine: the F-15C (a tail
  now), the F-35B held fully back from brake release (held back from her
  rotation she flies off first), and the 747-400 and F-22A - which have no
  take-off to fly - run up on full power with the stick fully back from
  brake release, steered by nothing. Each must strike on a contact behind
  her main wheels, read from JSBSim's own touch flags, and her debrief name
  it; on her wheels within a degree of her contacts' attitude, with them
  lifted no lower than it and within three. Three named: the Cub and the
  S.23, with no tail to strike, and the Mosquito. **Seen to fail** on the
  old models: the 747-400, F-15C, F-22A and F-35B "held fully back and
  never struck".
- The approach test's `no_tail_to_strike` no longer names the F-15C.

**Found on the way, fixed.** The approach and circuit tests read each
aeroplane's stance from one never initialised, when JSBSim's centre of
gravity reads nought: the F-15C, whose structural frame runs negative, was
taken for a tail-wheel aeroplane (so she was named as having no tail and
passed), and the Mosquito for a nose-wheel one. They now initialise her
first (`initialized_for_her_stance`), and the Mosquito is named with the
Cub and the S.23. `tools/align_models.py` leaves a tail strike out of its
shape measure (it is placed from the mesh's wheels, not the fit); the
offsets are unchanged, and the shape distances fell where a skid left -
the 787-8's from 2.19 m to 0.58, the C172P's 0.30 to 0.06, the C182's 0.56
to 0.05 - more evidence the skids were off the airframe.

**From the review of #170, 2026-10-13.**
- **Open: the rotation's rate does not hold four degrees a second, and no
  test pins it.** The test asked for was written:
  `every_nose_wheel_aeroplane_rotates_no_faster_than_four_degrees_a_second_near_its_tail_strike_and_leaves_below_it`.
  - It flew the 11 nose-wheel landplanes with a take-off at all 38 of their
    loadings. On her wheels, within four degrees of her strike attitude less
    its margin, each nose was to come up no faster than 4 deg/s plus a
    tolerance, and her wheels to leave below her strike attitude.
  - **It showed that the slowing does not hold the rate.** The law eases
    the pull, but that is too slow to stop a nose already coming. Rotation
    reached 7.7 deg/s on the A380 at her light loading and 7.5 on the A320
    (8.6 on the A380 with the slowing removed). Every aeroplane still left
    the runway at least 3.2 degrees below her strike attitude. The A380 no
    longer strikes because she leaves sooner, at 9.3 degrees rather than
    10.3, not because she rotates at four.
  - **Tried and backed out: checking the rotation by its rate**, three
    tenths of the elevator per degree a second over the rate wanted.
    Checked against 4 deg/s everywhere, it brought the worst down to
    5.55 deg/s, and the test passed with 2 of tolerance. **Seen to fail**
    with the slowing removed: 25 loadings too fast, and the A380 light
    wrecked on her tail. But the full suite then had five tests red:
    - the F-22A's measured rotation moved from 110 kt to 135;
    - the Cub was held on her wheels as she was handed over at lift-off;
    - the F-15C, handed over ten seconds after lift-off, was not given the
      climb-out.
    Checked only where the slowing asks for less, the F-22A, the F-15C and
    the A320 (6.1 deg/s) were still red.
  - Both the check and the test are backed out. A rate-limited rotation that
    keeps the fighters, the Cub and the measured speeds is its own item in
    Phase 10a, "The take-off autopilot rotates at a held rate".
- **The F-15C's hand-over at 10.0 s, traced.** With the easing,
  `the_f15c_and_f35b_handed_over_at_any_half_second_of_the_ten_after_lift_off_climb_away_and_never_touch_the_runway`
  failed with "f15c handed over 10.0 s after lift-off (413.7 ft, 5,564
  ft/min) was not given the climb-out".
  - Logged with and without the easing line, every quarter second from
    11 s to 24 s of the run. The easing acts only on her wheels: her stick
    differs from 12.75 s, as her nose passes 8 degrees at 6.8 degrees a
    second. It sets no flag or timer that outlives lift-off; the
    `pull_` it eases is the one the climb law already eases. Her wheels
    leave at the same moment (13.28 s), and her stage goes to the climb in
    the same quarter second (by 14.25 s).
  - What differs is the flight. With the easing she peaks at 13.7 degrees,
    not 16.2, flies a shallower phugoid, and is at 411 ft ten seconds after
    lift-off, not 386.
  - The hand-over rule (`Controller::lifting_off`) gives the climb-out only
    below 400 ft (`lift_off_below_ft`), and above it the autopilot, by
    design. So she was given what the rule gives; the sweep expected the
    climb-out at every half second, which no jet climbing at 5,500 ft/min
    can be held to at 10 s.
  - The sweep now expects the climb-out below 400 ft and the autopilot
    above it, and fails either way round. Every other check on each
    hand-over is unchanged: never touching the runway again, passing
    500 ft, and the autopilot's within three minutes.
- **The `nose_out` change seen to fail.** With `nose_out` put back on the
  commanded pitch (`flare_pitch_`), all three landing tests went red with
  the F-15C "rose 0.612250 ft after its wheels met the runway, more than
  0.500000": the approach lesson, light and heavy, and the AI approach.
  Reverted.
- **Why the tick stands with five strikes judged loosely.** The 747-400,
  787-8, F-22A, F-35B and Learjet 35A strike in the over-rotation test with
  their wheels just lifted. For them the test asks for the strike within -1
  to +3 degrees of their contacts' attitude, not within one. The item asks
  that each strikes where her airframe would and is judged a strike. For
  these five that holds as it does for the other eight:
  - Each strikes on her tail-strike contact, behind her main wheels (the
    test checks which contact touched).
  - That contact is where her airframe is, held within a quarter of a
    degree of her mesh by `ground.py --check` (her drawing, for the
    Learjet).
  - Her debrief names the strike.
  - The wider band is only the pitch at which it happens. With her wheels
    lifted she is higher than her wheels let her pitch on the runway, so
    she must reach more than the on-the-wheels attitude to touch. Below
    that attitude would be wrong, and the test refuses it.

**Re-run, 2026-10-13** (linux-release, `ctest -R` over take_off, rotat,
lesson, flare, approach, gear_up, airframe, belly, contact, committed,
tail, strike, ground, landing, debrief, circuit, 16r, gust, turbulen,
crosswind, plan, visual_model, alignment and figures): 392 tests, all
passing, 9 of them skipped for want of a key. **The selftest hash does not move**,
`182dd6c996e0ee4c`: the C172P's new contact never touches in it.

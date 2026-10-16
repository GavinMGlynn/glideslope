### The J-3 Cub standing tail to the wind meets a reversed wing's cited drag, not a flat plate's, 2026-10-16 — item done

**What is still not true first.**
- **Her lift past 90 degrees of angle of attack is nought**: the pinned
  `Lift_alpha` table ends at plus and minus 90 with nothing, and a wing in
  reversed flow lifts - Sheldahl and Klimas (below), -0.85 at 170 degrees,
  -0.66 at 160. Stood tail to 15 kt (-164 degrees) her lift is 0.0 lb, where
  the section's figure scaled as her drag is would give her of order 70 lb
  pressing her down. The new Phase 9 item, "A J-3 Cub standing tail to the
  wind has no lift from her wing, and an unchecked pitching moment".
- **Her angle-of-attack pitching term grows on past 90 degrees**:
  `Pitch_alpha` is linear in the angle (Cm-alpha -2.03 at her lowest
  Reynolds number), so at -164 degrees it reads a Cm of +5.8 and the air
  presses her tail down with 4,096 to 4,121 ft-lb. It holds her tail down,
  the safe way, but its size is not measured against anything. The same new
  item.
- **Two judgements in her drag, not the report's** (the docstring says
  both):
  - **The whole curve scaled by 1.41/1.80 (0.78)**, to meet her finite
    wing's broadside 1.41 at 90 degrees where the report's section reads
    1.80. Near 180 degrees the drag is edge-on profile drag, which a finite
    wing does not reduce, so there the scaling is probably low: 0.0196 at
    180 against the section's 0.025, and 0.19 against 0.24 at -164 degrees,
    where she stands - about 7 lb at 15 kt.
  - **A symmetrical section's curve for her cambered USA-35B**: the report
    says the four sections it measured had similar drag past 20 degrees and
    held at every Reynolds number past the stall (page 11). That a cambered
    one would be alike too is an inference from that, not its own word.
- The air's force on her is held as her wing's drag plus her airframe's and
  gear's (0.024 at any angle, as in forward flight); the fuselage's own drag
  in reversed flow is not measured separately.
- **No tail-wheel aeroplane now goes through the gust test's 1 to 2 kt
  window** (below): the Cub joins the Mosquito, held by her brakes, and the
  brake cap there is exercised on nose-wheel aeroplanes alone.

**The finding.** Stood tail to the wind, JSBSim's angle of attack,
atan2(w, u), is near 180 degrees: on her brakes at her stance pitch in 15
kt from behind, -164 to -165 degrees. Her wing's drag table (`Drag_basic`,
the USA-35B's) ended at plus and minus 90 degrees with a flat plate's 1.41,
and a JSBSim table holds its last value past its end: the air pushed her
with 194.8 lb, a coefficient of 1.433 on her wing's area, at every loading.
Her brakes held her.

**The figure.** R. E. Sheldahl and P. C. Klimas, "Aerodynamic
Characteristics of Seven Symmetrical Airfoil Sections Through 180-Degree
Angle of Attack for Use in Aerodynamic Analysis of Vertical Axis Wind
Turbines", SAND80-2114, Sandia National Laboratories, printed March 1981
(the UNT Digital Library's scan,
https://digital.library.unt.edu/ark:/67531/metadc1191254/, SHA-256
af5cb8e9f564121155014b7c88d3b41ef75cd0655bcdc41690cd4f5b6b38bb97). Table 2,
the NACA 0012's lift and drag to 180 degrees at Reynolds numbers from
10,000 to 10,000,000: past the stall its rows are the same at every one
(hers at 15 kt is 0.7 to 1.0 million, those tables on pages 20 to 22) -
drag 1.80 at 90 degrees, 1.465 at 120, 0.575 at 150, 0.32 at 160, 0.23 at
165, 0.14 at 170, 0.055 at 175, 0.025 at 180. Page 11: past 20 degrees the
drag "similar for all" four sections tested, and 1.8 broadside "near
Hoerner's value of 1.98 for a two-dimensional flat plate"; and that the
post-stall measurements were "sufficiently independent of the Reynolds
number" to use at every one.

**The change.** tools/make_j3cub.py carries `Drag_basic`'s table on past
90 degrees to 180, either way (a symmetrical section's drag is even in its
angle), by those rows at every five degrees, 95 to 180, scaled by
1.41/1.80 to meet her own broadside figure at 90 - continuous there, and
the report's in shape past it (the two judgements above). The docstring
says it with the citation. Nothing else in the model moves.

**The test.**
`the_j3_cub_standing_tail_to_a_15_kt_wind_is_pushed_within_0_01_of_a_reversed_wings_cited_drag`
(tests/unit/test_departure.cpp), new: at her model's own loading and her
figures' three (solo, trainer, gross), started on the runway and stood on
full brakes at idle five seconds in calm air, then 15 kt from straight
behind for fifteen; over the last five, at every step, she has not moved
(under 1 kt over the ground), the air meets her from behind (her angle of
attack 150 degrees or more either way), and the air's force along the wind
(`forces/fwx-aero-lbs`) over her dynamic pressure and wing area is within
0.01 of what the figure makes of her: the report's section figure
interpolated at her angle, scaled by 1.41/1.80, plus her airframe's and
gear's 0.024. The bound, 0.01 (1.4 lb at 15 kt), is the small terms left
(induced and sideslip drag, near nothing standing), and narrow enough that
the section's rows unscaled are wrong. Her measured share beyond her wing is
printed. Coverage asserted: four loadings, 600 steps each.
- **Flown**, linux-debug, rebased on #207: at -164.2 to -164.4 degrees, 15.0
  kt of air, the air's force 28.8 to 29.3 lb, a coefficient of 0.2119 to
  0.2157 against the figure's 0.2118 to 0.2156 (off by +0.0001; the
  section's own, unscaled, 0.240 to 0.245); beyond her wing 0.0241; lift
  0.0 lb; pitching +4,096 to +4,121 ft-lb.
- **Seen red**, twice, in the build's copy of the model: with the section's
  rows unscaled, every loading wrong, "drag coefficient 0.268 at -164.2
  deg, the figure's 0.215: off by 0.053" (36.5 lb); and (the first draft's
  test, with the table as on main) every loading wrong at 1.433, 194.8 lb.
  Restored, green.
- **From the review of #211**: the first draft held her whole coefficient
  within 0.05 of the section's figure unscaled. At -164 degrees that was
  off by -0.029, reaching -0.05 near 159 degrees, so a correct model could
  fail had her stance moved a few degrees, and unscaled rows passed too. It
  now holds her to the figure as her model scales it, within 0.01.

**One test's coverage moved, found red on CI** (run 38789862490, every
platform): `a_gust_pushing_every_landplane_from_1_to_2_kt_with_her_throttle_closed_winds_her_brake_no_further_than_idle_needs`
asserted the Cub was pushed past 1.3 kt by a 20 kt gust from behind. She
was pushed to 2.99 kt only by the flat plate's drag; with the reversed
wing's (0.21 of her dynamic pressure on her wing's area, against 1.43) her
brakes hold her at 1.01 kt, autobrake at most 0.757, as they hold the jets
and the Mosquito. She is named with them, flown and judged as before, and
the pushed count asserted is the C172P, C182 and PA-28's three. Its comment
said 25 kt where the test blows 20; corrected. It now says that no
tail-wheel aeroplane goes through its 1 to 2 kt window.

**Verification**, rebased on main with #207, linux-debug: 142 tests chosen
by name pass - the new test, the gust test, every committed-model check,
the alpha-term checks, every Cub test, the walking-pace, crosswind, headwind
and tailwind lessons and roll-outs, the tail-wheel stance and standing
tests, and the selftest tests. No figure moved with the rebase: the new
test's and the gust test's are as before it.
`glideslope_cli selftest`: `07cc7d4a4d42a300`, main's, unmoved - it flies
the C172P. Before the rebase, in a `-march=x86-64-v2` debug build (no fused
multiply-adds, as on CI): the new test's figures the same to the printed
digit, and the 23 Cub, walking-pace, tail-wheel, standing, headwind and
tailwind, braked-stop and alpha tests and the Cub's committed-model check
passed.

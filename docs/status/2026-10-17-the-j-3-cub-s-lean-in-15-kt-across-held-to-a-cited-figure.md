### The J-3 Cub's lean in 15 kt across held to a cited figure, 2026-10-17 — item done

**What is still not true first.**
- **Her side area is the least her Owner's Manual's figures make of her**:
  her cabin is taken at the firewall's depth, though its windows stand
  above it to the wing (the manual gives no cabin height), and her wheels,
  gear legs, lift struts, engine cylinders and wing tip are not counted.
  Each would add to her 48.7 sq ft.
- **The flat plate is a judgement**: her fuselage is fabric over a truss
  with rounded decking, somewhere between Hoerner's flat plate broadside
  (1.17) and a circular cylinder's (0.6 to 0.7 in the same figure, at her
  Reynolds number of about 4 x 10^5). The flat plate is the bluffer.
- **Her main gear springs are the pinned model's**, 1,000 lb a foot a
  wheel, from no source. No citable figure was found for a J-3's gear
  stiffness or static deflection: the Owner's Manual says only "rubber
  cord shock absorbers"; Univair's shock-cord notes give no load against
  stretch; MIL-C-5651's tables were not found online, and would need
  Piper's gear geometry besides. A new Phase 9 item.
- **No measured lean of a Cub in a beam wind** was found to hold her to;
  she is held by the force on her, and her lean is printed, not judged.
- Her lean, 1.8 to 2.2 degrees standing in 15 kt across, is the side
  force's and her rolling moments' (her dihedral among them) on her soft
  gear; how much of it is each is not measured.

**The research.** Searched for a J-3's side area or side-force
coefficient, her gear's spring rate, and a measured lean in a beam wind:
the web, NASA's NTRS (only "Experiments in External Noise Reduction of
Light Airplanes" names a J-3), the Owner's Manual and "How to Fly a Piper
Cub" (OCR'd), Univair's shock-cord and gross-weight notes, and Du's
Cranfield thesis (2011, via the Wayback Machine). Du's side-force slope,
-0.134 per radian, is a small-angle figure for a quarter-scale model whose
fuselage he estimated from photographs (page 17, Appendix A.2) - already
the model's `Side_beta`, and no figure for 90 degrees. None of the three
asked for exists in a form that could be cited; the owner chose the chain
below.

**The figures.**
- **Her side area**, from Piper's J3C-65 Owner's Manual (undated; the
  Wag-Aero reproduction), pages 11 and 12: overall length 22 ft 4 1/2 in,
  depth of firewall 28 3/4 in, wing chord 5 ft 3 in (page 11); fin area 4.7
  sq ft, rudder area 6.55 sq ft, rudder height 4 ft 5 7/8 in, wing leading
  edge to axle 2 1/2 in, axle to tail post 15 ft 7 1/2 in (page 12). Her
  fuselage is the firewall's depth from her nose to the wing's trailing
  edge (63 in aft of the leading edge) and tapers to nothing at the tail
  post (190 in aft of it); her nose is where the overall length less the
  rudder's mean chord (6.55 sq ft over 53.875 in, 17.5 in) puts it, 61.0 in
  ahead of the leading edge. Fuselage 37.43 sq ft, fin and rudder 11.25:
  **48.68 sq ft**.
- **Its drag**: S. F. Hoerner, *Fluid-Dynamic Drag* (1965), Chapter III,
  page 3-16: figure 28, "Drag coefficients of rectangular plates and
  circular cylinders as a function of their height (or diameter) to span
  ratio", and its text, "in 3 dimensions C_D = 1.17"; page 3-15: "Up to b/h
  = 10, the drag coefficient does not increase appreciably" (her
  fuselage's is 8.7). The Internet Archive's scan,
  https://archive.org/details/FluidDynamicDragHoerner1965.
- **Her drag area broadside**: 1.17 x 48.68 = **56.96 sq ft, 0.3191 on her
  wing's 178.5**.

**The change.** tools/make_j3cub.py writes `Drag_beta`'s rows at plus and
minus 90 degrees of sideslip as that figure less her airframe's and gear's
0.024 (`Drag_airframe`, `Drag_gear`, which hold at any angle): 0.2951,
where the pinned model had 1.23. Its rows at 15 degrees, her flight's
sideslip, are unchanged, so in flight nothing moves short of a slip past
15 degrees. The docstring gives the citations and the judgements.

**The test.**
`the_j3_cub_standing_in_a_15_kt_beam_wind_is_pushed_within_0_03_of_her_side_areas_cited_drag`
(tests/unit/test_departure.cpp), new, built as the tail-wind test is: at
her model's own loading and her figures' three, with the wind from either
side, she is stood on full brakes at idle, stick central, five seconds in
calm air, then 15 kt from straight across for fifteen; over the last five,
at every step, she has not moved (under 1 kt), the air meets her at 85
degrees of sideslip or more, and the air's force along the wind
(`forces/fwx-aero-lbs`) over her dynamic pressure and wing area is within
0.03 of 0.3191, computed in the test from the manual's dimensions and
Hoerner's 1.17, not read from the script. The bound, 0.03 (4 lb at 15 kt):
the sideslip table's slope between 15 and 90 degrees over the five degrees
she may stand short of 90 (0.016), and the terms keyed to her angle of
attack. Coverage asserted: four loadings, two sides, 600 steps each.
- **Flown**, linux-debug: at 87.9 to 88.2 degrees of sideslip, drag 42.7
  to 42.8 lb, a coefficient of 0.3142 to 0.3148 against 0.3191 (off by
  -0.0043 to -0.0049); beyond her sideslip drag 0.0253 to 0.0258; her most
  lean 1.79 to 2.16 degrees.
- **Seen red on main's model**: every one of the eight runs wrong, the air's
  force 161.9 to 162.5 lb, a coefficient of 1.190 to 1.195, off by 0.871 to
  0.876, at 85.3 to 85.7 degrees of sideslip; her most lean 4.34 to 4.76
  degrees.
- **Seen red with a planted bug**: the fin and rudder left out of her side
  area in the script (0.2214 at 90 degrees), every run wrong, "drag
  coefficient 0.243 at beta -88.2 deg, the figure's 0.319: off by -0.076".
  Reverted, the script run, green.

**One test's hand moved, found red**:
`the_j3_cub_held_at_every_walking_pace_in_a_15_kt_crosswind_stays_on_her_wheels`
held her pace on the throttle alone, her brakes off. With the beam wind's
drag at her side area's, rolling at 2 to 5 kt with the wind at 70 to 80
degrees off her nose the drag's share along her track is of order 10 lb
where it was of order 35 (estimated, the drag along the wind times the
cosine of that angle), and at idle she rolled past every pace but the fastest: 52 of 56
runs held at their pace for 0.7 to 24.8 s of 40, against 25. That was the
old drag holding her back, not her taxiing. Her hold now has the
walking-pace tests' brake hand (`walking_pace_brake`), on both brakes
alike, as those tests hold a landplane whose idle rolls her past her pace:
every run 38.0 to 40.0 s at her pace, brake at most 0.05, throttle 0.04 to
0.30, her most roll 2.0 degrees (it was 4.4). Nothing it judges moved.

**Verification**, linux-debug: the 64 tests whose names have cub, j3cub,
every_landplane or committed pass, and the 128 more with crosswind, wind,
gust, walking, standing, tail_wheel, light_aircraft or selftest. In a
`-march=x86-64-v2` debug build (no fused multiply-adds, as on CI), the 42
tests with cub, j3cub or every_landplane in their names that its test
program and `glideslope_cli` run pass, the new test's figures and the
walking-pace test's the same to the printed digit.

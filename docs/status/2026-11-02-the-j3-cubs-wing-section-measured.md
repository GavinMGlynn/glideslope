### The J-3 Cub's wing section found measured, 2026-11-02 - her model not changed, the item still open

Phase 10b's "The AI lands the J-3 Cub three-point" stays open. Her wing
section's lift is measured after all, and was put into her model on a
scratch build. It did not bring her touch nearer three-point, 10.0 degrees
against 10.2 before, and it took her stall out of her manual's range. Her
model is not changed. What to do about the stall figure is the owner's
decision.

**What is still not true first.**
- She still touches at about 10 degrees on her main wheels, where she stands
  at 12.1. With the measured lift she touches at 10.0 degrees, at 127 ft/min
  (was 68).
- With the measured lift her stall, flown at the Trainer's 1,092 lb, is
  **37.6 kt**, against her manual's 38 mph (33.0 kt) and the 31 to 35 kt her
  figures allow. Her glide ratio falls to 9.05 (9 to 11 allowed). The other
  five figures stay in range: climb 445 ft/min, ceiling 13,517 ft, cruise
  64.9 kt, and her rpm and power unchanged.
- No margin is held to yet. One is derived below, about 0.4 degrees.

**The source found.** The U.S.A. 35-B is measured in NACA Report 233 (Munk
and Miller, 1926, "The aerodynamic characteristics of seven frequently used
wing sections at full Reynolds number",
https://ntrs.nasa.gov/citations/19930091301). It was tested in the
variable-density tunnel as a 5 by 30 in rectangular wing of aspect ratio 6,
and its lift is tabulated from -9 to 21 degrees:
- Table XIX, at a Reynolds number of 1,695,000 (hers at the touch is about
  1.7 million): 0.845 at 6 degrees, 1.050 at 9, 1.240 at 12, 1.370 at 15,
  1.327 at 18 and 1.167 at 21.
- Table XX, at 3,470,000: 0.823, 1.045, 1.235, 1.374, 1.304 and 1.181 at the
  same angles.

NACA Report 352 (Jacobs and Anderson, 1930,
https://ntrs.nasa.gov/citations/19930091421) reduces those same tests
(its reference 5) for tunnel walls and aspect ratio. On pages 422 and 423
it gives formulas, and its Figure 1 gives the span-loading factor tau. Its
summary table has the U.S.A. 35-B's maximum lift as 1.374. Report 233
gives no section data at infinite span; Report 352 shows them only as
curves.

Taken to her wing:
- The tunnel's effective aspect ratio is 6 / (1 - (30/60)^2 / 2) = 6.86. Hers
  is 35.25^2 / 178.5 = 6.96. The two corrections cancel to 0.011 degrees per
  unit of lift: the tunnel model's wing already flies almost as hers does.
- Her wing's incidence is 2 degrees at its root. That is from Piper drawing
  12322, "Rigging diagram" (the PA-11, which has the J-3's wing and
  fuselage), on the Internet Archive as
  `piperj3_Bin_61_12322_PA11_Rigging_diamgram`. Her model's metrics give the
  same 2.00. The drawing's washout, 0 deg 41 min at the outboard aileron rib,
  was left out.

Against her body's angle of attack, at 1.7 million:

| body angle | 4 deg | 7 | 10 | 13 | 16 |
|---|---|---|---|---|---|
| measured, taken to her wing | 0.85 | 1.05 | 1.24 | 1.37 | 1.33 |
| her model's (Du, 2011) | about 1.08 | 1.42 | 1.50 | 1.61 | 1.51 |

Her model's table has more lift at every angle, peaking at 1.62 against the
measured 1.37. It is steeper below 5 degrees (0.137 a degree against 0.075)
and flatter from 7.5 to 10.

**Why she still touches at 10 degrees.** With the measured lift, the trace of
her hold-off (`land j3cub`, linux-release, calm air, 922 lb) shows the
elevator at its full 34 degrees up for much of her last 4 ft. Her nose rises
at about 2 degrees a second while she sinks at 75 to 120 ft/min, and she
touches at 33 kt with her wing at 14 degrees, near its peak. So what she
lacks is not lift that grows: either nose-up authority from her elevator
(Pitch_elevator, Du's, unchanged), or a hold-off that keeps her off the
ground longer. The change of lift moved her touch by 0.2 degrees.

**A justified margin, derived.** Her model's wheels (assets/jsbsim, contact
points) put her mains 77.17 in below her reference and her tail wheel 33.86
in below it, 201.97 in apart. All three touch, unloaded, at
atan(43.31 / 201.97) = **12.10 degrees**, which is her standing attitude.
When her mains touch at a lower attitude, the tail wheel is 202 in x
tan(12.1 - touch) above the runway: 6.7 in at 10.2 degrees. The mains can
bring it down by their own compression, at most their standing deflection,
1.1 to 1.7 in. 1.5 in over 202 in is **0.43 degrees**. Within that margin the
tail wheel reaches the runway as the mains take her weight, "together" in the
FAA Airplane Flying Handbook's sense (FAA-H-8083-3C, ch. 14). This is
derived, not published; no source has a figure in degrees.

**What the owner is asked.** Her manual's 38 mph stall takes a lift
coefficient of 1.66 at the Trainer's weight. The measured section gives
1.37, plus a little from her propeller's slipstream. These are the
possibilities:
- The manual's figure is indicated, not calibrated. Her figures file takes
  it as calibrated because nothing else is published.
- Her real wing out-lifts the 1926 tunnel model, through Reynolds number,
  turbulence, or her fuselage's and struts' lift.
- Her model's lift is right for the wrong reason.

Taking the measured lift means re-founding her stall figure. Keeping her
model's lift leaves the three-point item on the elevator or the hold-off,
not on the lift. Either way, a 0.4 degree margin is far tighter than the
present 2.5.

**Verification.** Nothing landed: her assets, tests and figures are as on
main. The figures above are from a scratch linux-release build with the
make-script change applied, which is kept outside the tree. Searched and
read: NTRS for NACA Reports 233 and 352. Report 669 was not needed, because
Report 352 gives the method. The archive.org piperj3_Bin_* list was searched
for rigging, wing and fuselage drawings, and 12322 was read.

### Her elevator in the hold-off, checked, 2026-11-02 - not what stops her

The coordinator decided to keep her lift as it is (Du's table). Whether her
manual's 38 mph stall is indicated stays an open point.

The question was whether her elevator lacks the authority to raise her nose
to 12.1 degrees. On her model as on main, it does not. The figures are from
`land j3cub`, linux-release, calm air, 922 lb, traced every tenth of a second
on a scratch build:
- **At the touch her elevator is at 64% of its travel, not full.** Its moment
  is 1,463 ft-lb. Full travel gives about 2,290, and her Cm-alpha term
  resists about 100 ft-lb a degree there, so roughly 6 to 8 degrees of
  incidence are in hand.
- **The lander asks for 12.1 degrees.** Its flare attitude reaches her
  standing attitude 0.3 ft over the runway. Her nose stays at 10.1 to 10.2,
  with her incidence at 11.1 to 11.5, under the lander's 12-degree limit.
  Her sink grows from 39 to 59 ft/min in her last 0.3 ft.
- **Raising the three-point flare gain** from 6 to 9, 12 and 18 drives the
  elevator to full travel, and it chatters. She touches at 10.5, 10.5 and
  10.4 degrees.
- **Raising the lander's incidence limit** from 12 to 12.5, 13 and 14 changes
  nothing: 10.2 degrees, because she never reaches it.
- **With the measured lift** (above) and gains of 6, 9 and 12, she touches at
  10.0 to 10.1 degrees.

The other points checked:
- Her elevator's travel is A-691's, 34 degrees up and 29 down, already in
  the make script. Its power is Du's -1.2 a radian.
- Her elevator works on her slipstream's dynamic pressure, which at idle is
  1.11 times the free stream (3.43 against 3.09 psf at the touch).
- Her CG solo is 13.94 in, mid-range of A-691's 10.6 to 22.7 in.
- Her model leaves out ground effect on the tail's downwash. Less downwash
  near the ground raises the tail's angle of attack and adds nose-down
  moment, so modelling it would lower her nose further, not raise it.
- A nose-up stabiliser trim adds authority beyond the elevator's travel, but
  authority is not what she lacks. The lander's trim is an offset inside the
  same travel.

Her figures and make script are unchanged.

**The remaining shortfall, in figures.** She is 1.9 degrees short of her
12.1, against the 0.43-degree margin derived above. The last 4 to 5 degrees
of nose come in her last 0.5 ft and about 1.3 s, while she slows from 31 to
30 kt. Her wing gives 1.47 at 7.5 degrees of incidence and 1.62 at its peak
at 12.5. Her nose rises at 2 to 5 degrees a second, so the ground arrives
first. What would close it lies in the lander's hold-off law, which is not
a sourced aerodynamic figure: a hold-off higher than half a foot, or the
nose raised earlier as she slows. Tried before (2026-11-01) at one to two
feet without the faster nose, she dropped on at 100 to 150 ft/min.

### The AI lands the J-3 Cub three-point, 2026-11-02 - done

**What is still not true first.**
- **In gusts she is still flown on to her main wheels.** This is unchanged,
  as the FAA's Airplane Flying Handbook (FAA-H-8083-3C) prefers in gusty
  wind: ch. 9, "Turbulent Air Approach and Landing", and ch. 14.
- She touches at 114 to 125 ft/min, where she touched at 68. The AFH has a
  three-point touch from about a foot up. The sink limit is 300 ft/min, and
  the server's gear judgement passes every landing.
- The trim's rate in the hold-off (below) is chosen from a sweep, not
  sourced.
- Her stall question is still open: whether her manual's 38 mph is
  indicated.

**The source.** The AFH, ch. 14, "Three-Point Landing", page 14-4: "Round out
(level-off) should be made with the main wheels about one foot off the
surface. From that point forward, the technique is essentially the same
that is used in nose-wheels: a gentle increase in AOA to maintain flight
while slowing. In a tailwheel aircraft, however, the goal is to attain a
much steeper fuselage angle than that commonly used in nose-wheel models;
one that touches the tailwheels at the same time as the main wheels."

On the same page, under "Touchdown": "the airplane is held off the surface
of the runway until the attitude needed to remain aloft matches the
geometry of the landing gear". Held off a bit too long, "the tail makes
contact first. Provided this happens from no more than a foot off the
surface, the result is undramatic".

Under "Crosswinds", pages 14-5 to 14-6: "only one main wheel makes initial
contact, either in concert with the tailwheel in three-point landings".

**What changed** (src/sim/lander.cpp). Each change is gated on her figures'
`lands="three_point"` and not in gusts, so no other aeroplane's landing
changes:
- **Held off a foot**, the AFH's "about one foot", where she was held at
  half a foot.
- **Her incidence is not limited short of the stall** in a three-point
  flare. Her nose is still bounded by her standing attitude. The lander's
  limit is 12 degrees of incidence for a light aeroplane, and with it her
  nose stopped at 11.5 degrees with the limit at 12.5, 11.7 at 13, and 11.8
  from 13.5 up.
- **The back pressure goes on increasing until her nose is there.** The
  trim takes up the attitude's error at 3 a degree a second, not 0.02. The
  flare's law is proportional, and it settled 1.9 degrees short with the
  elevator at 64% of its travel. Swept at 0.1, 0.3, 1 and 3, the touch was
  at 10.6, 11.6, 11.7 and 11.8 degrees.
- The tests' margin (tests/unit/test_lander.cpp) is now the 0.43 degrees
  derived above, not 2.5.

**Figures** (linux-release):
- **In the tests**, from a mile out: in calm air and in every crosswind from
  1 to 12 kt, both sides, dry and wet (50 landings), she touches at 11.82 to
  11.90 degrees and 114 to 125 ft/min, 360 to 363 m in, and stops 434 to
  439 m in.
- **Her tail wheel**, traced by `land j3cub` from 2 nm out: it first carries
  weight 4 steps (0.03 s) after her mains in calm air. In a crosswind it
  touches between her upwind main and her downwind one: 6 to 7 steps after
  the upwind main in 6 and 12 kt.
- **`land j3cub`**: she touches 363 m in at 11.8 degrees and 116 ft/min, and
  stops 440 m in. Her measured landing distance, 1,500 ft, is unchanged, and
  so is its test.

**Verification.**
- The five Cub landing tests (calm dry and wet, and the four crosswind
  tests) hold the 0.43-degree margin. **Seen red** with the lander as it
  was: all five failed, the first "the Cub touched with her nose at
  10.225225 degrees, where she stands at 12.103087 in calm air, dry".
  Restored, green.
- **The hard-flare lesson's bound for a three-point aeroplane**
  (tests/unit/test_lesson.cpp,
  `a_flare_the_pilot_pulls_hard_and_hands_back_in_the_air_is_landed_without_a_stall`):
  the Cub's wing now goes to 13.8 degrees, past the 13 that test allows a
  light aeroplane, as the AFH's hold-off to about the stall asks. For an
  aeroplane landed three-point, the test now holds her nose to a degree
  over her standing attitude, the lander's own bound. Her nose reached
  11.8. **Seen red** with that bound set a degree under her attitude;
  reverted.
- **The full linux-release suite (-LE nightly)**: 1,424 tests, two red.
  - The hard-flare lesson, above, is fixed. Rerun after the fix, with every
    flare, landing, gust, lesson, selftest and Cub test, it is green.
  - `the_google_terrain_draws_with_its_attribution_or_says_why_not_on_vulkan`
    fails here, twice alone: "google via ion-token drew no terrain at all".
    That is the Cesium ion token's Google tiles; it touches no code this
    item changed, and on CI, with no key, it skips.
- Every other aircraft's landing is unchanged: each change is behind
  `three_point_`, and only the Cub's figures say it.

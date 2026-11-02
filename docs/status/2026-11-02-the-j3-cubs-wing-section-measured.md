### The J-3 Cub's wing section found measured, 2026-11-02 - her lift kept as it was

This is the record of one finding made while working Phase 10b's "The AI
lands the J-3 Cub three-point". That item is closed by a change to the
lander, not to her wing:
docs/status/2026-11-02-the-ai-lands-the-j3-cub-three-point.md.

**The finding.** Her wing section's lift is measured. Put into her model on
a scratch build, it moved her touch only from 10.2 to 10.0 degrees, and it
took her stall out of her manual's range. The coordinator decided to keep
her model's lift (Du, 2011). Whether her manual's 38 mph stall is
indicated stays an open point.

**What the measured lift did** (scratch build, not kept).
- With the measured lift she touched at 10.0 degrees, at 127 ft/min (was
  68), on the lander as it then was.
- With the measured lift her stall, flown at the Trainer's 1,092 lb, is
  **37.6 kt**, against her manual's 38 mph (33.0 kt) and the 31 to 35 kt her
  figures allow. Her glide ratio falls to 9.05 (9 to 11 allowed). The other
  five figures stay in range: climb 445 ft/min, ceiling 13,517 ft, cruise
  64.9 kt, and her rpm and power unchanged.
- The 0.43-degree margin derived below is the one the landing tests now
  hold.

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

Taking the measured lift would mean re-founding her stall figure. Her
model's lift is kept, and the three-point landing was closed in the
lander's hold-off instead.

**Verification.** Nothing in her model changed: her assets and figures are
as they were. The figures above are from a scratch linux-release build with the
make-script change applied, which is kept outside the tree. Searched and
read: NTRS for NACA Reports 233 and 352. Report 669 was not needed, because
Report 352 gives the method. The archive.org piperj3_Bin_* list was searched
for rigging, wing and fuselage drawings, and 12322 was read.

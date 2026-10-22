### The J-3 Cub's gear preloaded by her shock rings' pre-stretch, 2026-10-22 — item done

**What is still not true first.**
- **The figure is derived, not published.** No J-3 static deflection is in
  print; it is worked from her rings, her drawings and her tyre's data.
- **She now stands on her tyres, and they sink 1.1 to 1.7 in.** The rings'
  preload, 731 lb a wheel, is more than any loading puts on a main wheel
  standing (542 lb at her gross weight), so her struts stay closed; what
  she sinks is her 8.00-4 tyres at her manual's 15 psi.
- **The tyre's rate is taken as linear** through Goodyear's rated point and
  scaled to her pressure; NACA TN 4110's own curve (equation 23) would sink
  her about 4% deeper.
- **JSBSim's convention is kept.** It compresses a wheel along her body's
  vertical and turns its force normal to the ground, so standing at 13
  degrees her wheels drop about 5% less than a tyre squashed straight down
  would.
- **Left out, as in #223**: the strands' slant (about 1% on their length,
  some 3% on the preload) and the stop disc's end of the stroke. The table
  holds its last force past 2 ft of compression (3,130 lb).

**The figures and their sources.**
- **The pre-stretch.** Piper drawing 31423 (Internet Archive,
  piperj3_Bin_11_31423_J3_Shock_strut_assy_GW_increase, full size, the scan
  at 150 ppi). The spools are 6.77 in apart, as #223 measured, and 3/4 in
  across (112 px on the right spool's side view). Measured on the drawing,
  the cord's centre runs 0.72 to 0.73 in from each spool's axis, which
  matches 3/4 + 11/16 = 1.4375 in. A ring goes four times between the
  spools and makes four half turns round them, so fitted it is
  4 x 6.77 + 2 x pi x 1.4375 = 36.11 in. Free, it is 25.72 in, so it is
  stretched 40.4%.
- **The preload.** Eight strands at 365 lb for 100% stretch (MIL-C-5651D
  Table III, as #223) pull 1,180 lb along the strut. Over the lever of
  drawing 31513 (1.61), that is 731 lb at the tyre (733 with the lever
  rounded).
- **The tyre.** Goodyear Aviation Data Book 2022, Section 4C
  (https://www.goodyearaviation.com/resources/pdf/Data-Section-2022.pdf),
  the 8.00-4 4-ply 804C41-1: rated 1,100 lb at 24 psi, 18.0 in across
  (max) and a static loaded radius of 6.65 in, so 2.35 in at its rated
  load, 468 lb/in. NACA TN 4110 (Smiley and Horne, 1958,
  https://ntrs.nasa.gov/citations/19930084870), equation 22: at a given
  deflection a tyre's load goes as p + 0.08 p_rated. At 15 psi
  (J3C-65 manual, as assets/aircraft/j3cub.aircraft; the PA-18 manual has
  18 for the same tyre), that is 306 lb/in.
- **The model.** Each main wheel's spring is now a JSBSim `strut_force`.
  Its force follows the tyre alone up to the preload (2.39 in), then the
  tyre and the rings in series at 111 lb/in; the pinned dampers are kept
  (500 lb/ft/s, 850 on rebound). The script is tools/make_j3cub.py
  (`gear_strut_force_table`). JSBSim's own c172x model uses the same
  element.

**Verification.**
`the_j3_cub_standing_at_every_loading_sinks_on_her_main_wheels_by_her_tyres_deflection_alone_within_1_percent`
(tests/unit/test_departure.cpp) works the figure again from the sources'
numbers, independently of the script. It takes her wheels' load from her
weight and her centre of gravity between her wheels at the pitch she stands
at. At her model's own loading and her figures' three, she stands 20 s with
the engine stopped and the brakes on. Her main wheels sink as follows,
each within 0.2% of the figure:

| Loading | Sink | Load a wheel |
|---|---|---|
| Her model's own | 1.095 in | 343 lb |
| Solo | 1.309 in | |
| Trainer | 1.559 in | |
| Gross | 1.728 in | 542 lb |

The test asserts coverage: four loadings. It was seen red on the spring
before the change: 1.92 in against 1.10 (+75%).
`the_j3_cubs_main_wheels_are_sprung_within_1_percent_of_the_rate_her_shock_rings_rating_makes`
now reads the rings' rate out of the table: the slope past the preload,
with the tyre's taken out. It still reads 2,093 lb/ft (-0.4%).

Run here in linux-release,
`ctest -R "cub|j3|tail_wheel|standing|stands_on|every_landplane|every_aircraft|committed|figures"`:
116 of 116 passed. `glideslope_cli selftest` gave hash `521cb12d3a1c5f0c`,
unchanged (the C172P's flight).

**What moved.**
- **Stance.** She stands at 12.7 to 13.5 degrees by loading, against about
  12.1. The tail-wheel stance test has her between 11.98 and 12.65 degrees
  (was 11.89 to 12.41).
- **Lean.** Standing in 15 kt across she leans 0.43 to 0.50 degrees (was
  0.78 to 0.91). The beam-wind drag is unchanged at 43.2 lb.
- **Nose-over sweep (#228).** Its pinned levels and counts held: 30 of 60
  runs go over and 2 more lift her tail.
- **Not done:** an x86-64-v2 build. The new test is 0.2% inside a 1% bound.

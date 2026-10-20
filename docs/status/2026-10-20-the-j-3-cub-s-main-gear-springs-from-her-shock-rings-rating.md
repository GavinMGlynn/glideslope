### The J-3 Cub's main gear springs from her shock rings' rating, 2026-10-20 — item done

**What is still not true first.**
- **The rate is derived, not published.** No J-3 spring rate or static
  deflection is in print (the owner's decision of 2026-10-20: derive it).
- **Her rings' preload is left out.** On drawing 31423 the rings sit
  about 45% stretched, so a real Cub stands on that preload and hardly
  sags at all. JSBSim's spring has no preload, so she sinks on it instead:
  about 3.0 in a wheel, statically, at 527 lb a wheel (two 170-lb people,
  20 lb of baggage and full fuel, 1,112 lb, from the model's masses). On
  the old spring it was 6.3 in.
- **The cord's pull is taken as linear up to 100% elongation.**
  MIL-C-5651D Table III's 50, 75 and 100% figures stay within about 10% of
  a straight line.
- **The strands' slant is left out** (cosine 0.99). So is the stroke: the
  stop disc (81232-30, Univair SB-103) limits the strut's travel but does
  not change the rate.
- **Which ring.** Her own strut drawing draws the 11/16 in cord (P/N 1174
  class, 730 lb). The catalogue lists the 1280-HD (3/4 x 8 in, 950 lb) for
  J-3s from S/N 8278 up. That ring would make 2,550 lb/ft rather than 2,093.
- **The damping is still the pinned model's:** 500 lb/ft/s, and 850 on
  rebound.

**The figures and their sources.**
- **The ring.** P/N 1174 is 11/16 x 7 1/2 in, rated 730 lb test, four to
  an aeroplane: Aircraft Spruce catalogue p. 306,
  https://www.aircraftspruce.com/pdf/2025Individual/Cat25306.pdf
  (sha256 e6f366e8...aac0), and Piper Service Memo 12
  (https://resources.univair.com/docs/pdfexport/id/6256e099e69eb4e81d7b23c7).
- **What "test" means.** MIL-C-5651D (everyspec, scanned, read by OCR and
  by eye), Table III. A Type II ring's "load @ 100% elongation" is twice a
  Type I cord's of the same section: 800-1300 lb against 400-650 for the
  0.750 in cord, 650-850 for the 0.687 in ring. So the rating is the pull of
  two strands at double their length, and one strand pulls 365 lb.
- **The strut.** Piper drawing 31423, "Strut assembly, shock", marked
  SCALE FULL SIZE (Internet Archive, Piper J3 Club blueprints, Bin 11).
  Its 3600 x 2700 scan is a 24 x 18 in sheet at 150 ppi. Scale check: the
  AN310-4 castle nut (7/16 in across its flats) measures 0.45 in across
  its flats and 0.49 across its corners (0.505 expected), so the scale is
  within 3%. The 31322 cord sections measure 0.67 to 0.69 in, which is
  11/16 rather than 3/4 (0.75). The spools are 6.77 in apart, centre to
  centre (1015 px). Four cord passes lie side by side on each spool, two
  rings wound twice: four strands to a ring.
- **The strut's rate.** Growing the spool gap by d stretches a 25.72 in
  ring (pi x (7.5 + 11/16)) by 4d. Its four strands then pull
  4 x 365 x 4d / 25.72, which gives 227 lb/in a ring and 454 lb/in for the
  strut.
- **The lever.** Piper drawing 31513, "Gear installation - landing", GW
  1220 lb, front view at 3 in to 1 ft, rendered at 100 dpi (0.04 in of
  aeroplane to the pixel). Measured from it: the vee's hinge at the
  fuselage fitting, the strut's outer eye at the axle, its inner eye at the
  cabane vee, and the tyre's centre plane. The tyre rises 1.61 in for each
  inch the strut extends, near the forum's 1.5.
- **At the tyre.** 454 / 1.61^2 = 174 lb/in, or 2,093 lb/ft a wheel,
  against the pinned 1,000.

The coordinates and the arithmetic are in tools/make_j3cub.py
(`gear_spring_lbs_per_ft`), and its docstring cites each source.

**Verification.**
`the_j3_cubs_main_wheels_are_sprung_within_1_percent_of_the_rate_her_shock_rings_rating_makes`
(tests/tools/cub_gear_springs.py) derives the figure again from the
sources' numbers, independently of the script: 2,102 lb/ft, using the
lever rounded to 1.61. It holds both main wheels of the committed model to
within 1% of that figure; the model's 2,093 is -0.4%. Its space is the
model's five sprung contacts: both mains checked, and the tail wheel (a
leaf spring) and the two wing tips named as left out. It was seen red on
the old 1,000 lb/ft: -52.4%, both wheels failing.

Run here in linux-release, `ctest -R "cub|j3|tail_wheel|standing|stands_on|every_landplane|every_aircraft|committed"`:
102 of the 103 passed. Every Cub test and every committed-asset test was
among them, and every all-aircraft ground test. The one failure,
`a_client_hears_where_every_aircraft_is`, needs glideslope_server, which
was not built here; it is unrelated, and CI runs it.

**What moved.** Her lean standing in 15 kt across is now 0.78 to 0.91
degrees, down from 1.8 to 2.2. The beam-wind drag is unchanged at 43.1 lb
(coefficient 0.3167 to 0.3170, against the figure's 0.3191). Her stance is
12.10 degrees, and she stands between 11.89 and 12.41 on all three wheels.


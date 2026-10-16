### Two checklists that were never actually blocked, 2026-09-22

**What is still missing: eleven of the sixteen aircraft have no handbook
behind their checklists**, and for eleven of them that is now an answer rather
than an omission.

**The item said thirteen and explained ten.** The J-3 Cub's manual survives
only as scans; no flight manual is public for the B-2A, the F-22A or the
F-35B; the airliners' and the business jet's are their operators'. That is
ten aeroplanes. The Mosquito, the F-15C and the Short S.23 were in the list
with no reason given at all, and nobody had noticed because the number was
carried in prose rather than counted.

**The Mosquito's Pilot's Notes were already in the repository's own
documentation.** A.P. 2019E-P.N., January 1950, recorded in `ASSETS.md` a few
sections above the checklist entry - cited there for the aircraft's figures,
as `[PN50]`. It carries its own **"PILOT'S CHECK LIST", 128 numbered items on
pages 6 to 10**, with Part II paragraphs 37 to 44 giving the handling to go
with them. There was nothing to find; it only had to be read.

**Reading it corrected three things** in the file that had been written from
ordinary practice for the type: the radiator shutters were open for starting
where para. 37 shuts them and opens them at the magneto check; the fuel cocks
were on the main tanks where para. 37 starts on the outer ones; and the climb
was at 148 knots, the 1944 edition's 170 mph, where the 1950 edition's para.
41 gives 150.

**The F-15C's flight manual is on the Internet Archive.** T.O. 1F-15A-1,
section II, gives the drills and the numbers: engines to 80 per cent before
brake release, half aft stick at about 120 knots and ten degrees of pitch,
350 knots to 0.90 Mach on the climb, 300 knots at the break, gear and flaps
on downwind below 250, on-speed angle of attack on final, the nose to thirteen
degrees for aerobraking and not past fifteen, and the wheel brakes only after
aerobraking has done its work above 90 knots.

**What the F-15C's file cannot say, and says so.** The model has no flap
channel, so "Flaps - CHECK DOWN" before take-off and "Flaps - UP" after
landing are the pilot's to confirm rather than checks; and there is no
angle-of-attack channel, so the manual's on-speed approach is a confirmation
beside a speed band from this project's own figures.

**A slip worth recording.** The script that updated both living documents
failed its assertion on `COMPLETION_PLAN.md` and the commit ran anyway,
because the two were separate commands rather than one. `ASSETS.md` said five
and the plan said three, pushed. The next commit fixed it; chaining the write
to the commit would have stopped it.

### Three checklists written from the handbook, 2026-09-21

**The provenance blocker was mine, not the world's.** The checklists were
recorded as written from ordinary practice because no handbook had been read
- and `docs/ASSETS.md` had been holding URLs for several of those handbooks
all along, which nobody had opened. The project owner said to go and look.

**Three of the sixteen are now written from the aeroplane's own handbook**,
read rather than remembered:

| Aircraft | Handbook | What was taken |
| --- | --- | --- |
| Cessna 172P | POH and FAA Approved Airplane Flight Manual, 1981 Model 172P, section 4, pp. 4-6 to 4-10 | Its eleven checklists, in its order, with its figures |
| Cessna 182S | POH section 4, pp. 4-11 to 4-17 | The same |
| Piper PA-28-180 | Owner's Handbook, Cherokee "E", section III, pp. 17 to 23 | Its numbered before-landing check list, and its prose procedures |

**Two things the reading changed.** The Cessna 182's climb band was 82 knots
and the handbook says 85 to 95; the 172P's take-off gained the run-up at
1,700 RPM, with the magneto drop the handbook allows. Both were written from
memory before and were not wrong so much as not sourced.

**And one thing found on the way.** The 172P's handbook has no URL in
`ASSETS.md`, so it had to be searched for. The first copy found was a scan;
the second had a text layer but had scanned two climb speeds into nonsense -
"Climb Speed -- ~ ISJAS" - so a third copy was read to recover them, 70 to 80
knots on the take-off and 70 to 85 en route. Both copies are now recorded.

**The J-3 Cub's manual was looked for and is not readable.** Both copies
`ASSETS.md` records carry no text layer, and the text versions that exist are
a flying club's checklist and a flight-simulator vendor's manual, neither of
them Piper's. That is now a checked fact rather than an assumption, and its
checklist stays this project's own words.

**The frame test earned its keep.** Rewriting the 172P's take-off list from
the handbook took it from six items to seven, and
`the_checklist_on_screen_is_the_one_the_flight_is_working_through_on_vulkan`
failed because it pins that number - which is what it is for. A silent change
to what is taught is exactly what it refuses.

**The two Cesium defects were checked against `main`** rather than against
the pinned copy, and both are still there. The second is worse than the draft
said: `parseQuantizedMesh` and `decodeIndices` read unaligned the same way as
`readValue`, so a fix wants the file rather than the function. No matching
issue is open on their tracker. `docs/cesium-issues.md` says so now.

**What a lesson's reference speed can be** was looked into rather than
assumed. Six of the sixteen publish a stall speed. The airliners' and
business jet's manuals are their operators' and are not published; the A320
family's stall speed is not a tabulated figure at all, being VS1g behind a
low-speed protection the crew cannot override; and no flight manual is public
for the B-2A, F-22A or F-35A. `figures.cpp` can already measure a stall speed
from any model, which is a way through that invents nothing - but it changes
what a lesson's figures mean, so it is written in the plan for the project
owner rather than taken.

319 of 319 tests pass locally at `-j4`, in 991 s.

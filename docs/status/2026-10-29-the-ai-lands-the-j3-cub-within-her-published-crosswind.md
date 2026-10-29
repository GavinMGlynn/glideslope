### The AI lands the J-3 Cub within her published crosswind limit, 2026-10-29 - item done

**What is still not true first.**
- **The limit is not Piper's.** Piper published no crosswind figure for the
  J-3. She was certificated (A-691) long before a demonstrated crosswind was
  asked of an aeroplane: the placard is required of those certificated after
  3 May 1962, and 14 CFR 23.233's 0.2 Vso dates from 1964. The figure used is
  the EAA's, 12 kt, "about" in its own words. A Swiss operator's checklist
  gives 10 kt, which the 12 kt sweep covers.
- **No fix was needed, and none is made.** The ground loop the item named
  was gone before this item began (below). This item cites the limit, lands
  her through it, and puts her back into the 15 kt wet test.
- **Her roll-out still never brings her below half a knot in 400 s** in the
  roll-out test that hands her over as 15 kt arrives
  (`every_landplane_rolls_out_on_a_wet_runway_in_a_15_kt_crosswind_and_stays_on_it`).
  She stays left out of that test for that reason alone. The words saying she
  ground-loops are gone from it.
- **Within 5 m of the centreline is not asked after the touch.** She is held
  to the runway's half-width (30 m), as every light aeroplane is in the 15 kt
  test. The most she goes is 6.35 m (12 kt from the right, dry). The roll-out
  drift item (5 m for every landplane) is still open, and this does not close
  it.

**The limit, and where it was looked for.**
- **Used:** Steve Krog (EAA 173799), "Crosswind Takeoffs 2.0 - Understanding
  the Factors", EAA, 10 December 2019:
  <https://inspire.eaa.org/2019/12/10/crosswind-takeoffs-2-0-understanding-the-factors/>.
  He writes that the demonstrated crosswind component for the Piper J-3 Cub
  is about 12 knots at 90 degrees. Krog writes EAA Sport Aviation's "The
  Classic Instructor" column and has run Cub Air Flight, a tailwheel school,
  since 2006.
- **Also found:** "Normal Procedures Piper Cub J-3/L-4 - HB-OEN" (2021), a
  Swiss operator's checklist: "Max. Crosswind: 10 kt"
  (<https://sprinkle.com/aircraft/manuals/piper-12/piper-j-3-cub-121/checklist>,
  from flubag.ch). It is not an approved manual.
- **Disputed:** a j3-cub.com forum post (2010) says the J3 AFM's "Schedule of
  Performance, General" gives a demonstrated crosswind of 10 kt
  (<https://www.j3-cub.com/threads/crosswind-component.18941/>). Another
  thread says "no figure was ever published"
  (<https://www.j3-cub.com/threads/maximum-demonstrated-crosswind.28141/>).
  No copy of that page was found.
- **Searched, no figure:** the J3C-65 owner's manual (the Scribd copy is not
  readable; reprints are sold only); A-691 (it holds no performance figures);
  the A2A J-3 pilot's manual (text read: crosswind technique, no figure);
  Krepelka's J-3 flight notes; piperowner.org; and archive.org (the piperj3_*
  drawings, plus searches for a J-3 or L-4 handbook).

**What made the ground loop go.** Every model was flown with the lander and
code of this branch, and only her model file was swapped in the build's data
directory:

| her model | 15 kt from the left, dry | wet |
| --- | --- | --- |
| 40045241 (2026-10-14) | on her back, rolled 179.9 | on her back, never stopped |
| a793ee39 (2026-10-15, pitch terms off the crosswind) | banked 16.0 | banked 15.3 |
| 3fe583a8 (2026-10-15, lift and wing drag off the crosswind) | banked 3.9, 4.35 m off | banked 3.8, 3.77 m off |
| now | 6.45 m off | 5.95 m off |

So her lift and wing drag counting a crosswind as airspeed rolled her over
after the touch. That was fixed on 2026-10-15 (`tools/make_j3cub.py`), and the
plan item was not updated then. Within 12 kt she landed even on the model of
2026-10-06 (83c83b1f): 4.96 m off at the most. Now she lands in every whole
knot to 20, both sides, dry and wet, at most 6.53 m off (14 kt from the
right, dry).

**Verification.**
- Four tests, one for each side and runway:
  `the_j3_cub_is_landed_by_the_ai_on_a_{dry,wet}_runway_in_every_crosswind_from_the_{left,right}_within_her_published_12_kt`.
  Each lands her from 5 nm out in every whole knot from 1 to 12. For each
  landing it checks that she touches within 5 m of the centreline, stops on
  the runway, stays within its half-width, banks under 15 degrees, keeps her
  nose above -10, rises under 3 ft, and is unwrecked and settled as the
  server judges her gear. **Coverage:** 12 winds in each test, counted
  against the limit, so 48 landings in all. Locally (release) about 7 s
  each. Farthest after the touch at 12 kt: 6.25 m and 5.63 m from the left
  (dry, wet), 6.35 m and 5.78 m from the right.
  **Seen red** with the roll-out's rudder held central in `lander.cpp`: "the
  Cub went 34.368009 m from the centreline after the touch in 4 kt from the
  left, dry". Reverted.
- `every_light_aircraft_landed_on_a_wet_runway_in_a_15_kt_crosswind_stays_on_it_as_on_a_dry_one`
  now takes in the Cub, so all 4 of 4 are compared, and checks that the dry
  landing is upright too. **Seen red** with her models of 40045241 and
  a793ee39 swapped into the build: "j3cub stopped on the wet runway in the
  crosswind, 0.000000 m along" and "j3cub banked 15.303546 degrees after
  touching down on a wet runway". Green with 3fe583a8's and today's.
- **CI cost, estimated** in `tests/ci_costs/` from the 15 kt test's six
  landings (100 s on Linux debug): 200 s for each Cub test there and 134 s
  for the 15 kt test with her in it, about 830 s more in all, spread over
  the shards. Measure them again from a green run.
- `lander.cpp` is unchanged; its bug was only made for the red run.
- The selftest hash does not move: no simulation code or asset changes.

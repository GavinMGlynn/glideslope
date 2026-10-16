### "Survives only as scans" was not a reason, 2026-09-22

**What is still missing: ten of the sixteen aircraft have no handbook behind
their checklists**, and for all ten that is now a stated reason rather than an
omission.

**The J-3 Cub was recorded as unsourceable because its manual is a scan with
no text layer.** That stops a text search. It stops nothing else: the pages
were read as pages, the same way the Mosquito's Pilot's Notes were read an
hour earlier. Piper's Owner's Manual for the J3C-65 carries "Flying Hints" on
pages 37 to 42 - sections A to H, from before starting the engine through to
parking and mooring - and it is the manual `ASSETS.md` already recorded for
the Cub's figures.

**What it gave that the unsourced file did not have**: the throttle a tenth
open to start rather than "a quarter of an inch"; 700 r.p.m. to idle on and
550 to 600 once warm; the magnetos checked at 2,100 with no more than 75
r.p.m. dropped; 2,300 r.p.m. for take-off and no full throttle beyond three
minutes; airborne at 39 m.p.h.; best climb at 55; 2,150 r.p.m. for a 73 m.p.h.
cruise; the glide at 50 to 60; carburettor heat on *before* the throttle comes
back; the engine cleared every 200 to 250 feet of a long glide; and 122 m.p.h.
as the diving limit. The file is in knots with the manual's miles an hour
beside them, as the PA-28's is.

**Two near misses in one afternoon, both the same shape.** A script wrote one
living document and silently did nothing to the other, and the commit went
ahead because nothing checked. The first time `ASSETS.md` said five and
`COMPLETION_PLAN.md` said three. The second time this entry was not written at
all - the replacement was looking for an escaped newline that the file does
not contain - and the commit that was meant to carry it went out without it.
**A string replacement that matches nothing is not an error unless something
asserts it is.** Every replacement in these scripts now asserts.

**And a third, of a different shape**: the first attempt at the Cub's file had
a quoting error and wrote nothing, and the checklist tests were run anyway and
reported eight of eight passing - on the unchanged file. A green tick on a
file that was never written is not evidence of anything. The second attempt
printed `git diff --stat` beside the test result: 51 insertions, 33 deletions,
and then the eight passes.

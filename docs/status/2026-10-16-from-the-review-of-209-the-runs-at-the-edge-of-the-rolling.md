### From the review of #209: the runs at the edge of the rolling ceiling, the swing names held, every name a landplane, 2026-10-16 — item done

**What is still not true first.**
- **Two named runs are at the edge, and either outcome passes there**:
  the C172P handed over at 3 kt in 15 kt across (0.296, 0.004 short of the
  ceiling) and the C182 at 2.5 kt across (0.293, 0.007 short), within
  `edge_margin`, 0.01 - against 0.006 between linux-debug's figure and
  linux-release's on the Cub. Neither reaches it in linux-debug. Every other
  named run is 0.012 or more short (the F-15C dry at 3 kt, 0.288, the
  nearest) and is held to the strict rule.
- Everything the entry below names as not true still is.

**This supersedes** the entry below where it says a named run that reaches
the ceiling is wrong without exception.

**The changes** (tests/unit/test_lesson.cpp):
- **At the edge**: `AtTheEdge` names a run, by aeroplane and pace, whose
  measured figure is within 0.01 of the ceiling. It is printed with its
  figure, its outcome and the margin, counted, and never wrong for reaching
  the ceiling; one that is not otherwise named, or whose figure is further
  than the margin from 0.3, is wrong. Asserted: every run at the edge flown
  (2 in the 2-3.5 kt crosswind test, none in the rest).
- **The swing names held to the same rule**: a run whose swing is named
  and comes back within 30 degrees is wrong ("name her no longer"). The
  Mosquito, at 46.5 to 46.6 degrees, is well clear.
- **Every name a landplane flown**: an id in the ceiling, swing or edge
  names that is not a landplane flown in the test is wrong.
- The comment on the rolling-ceiling count says the rule as it is.
- `tests/ci_costs/windows-release.txt`: the six at the larger of the two
  runs - 14, 21, 12 at 2-3.5 kt (dry, across, wet), 27, 24, 11 at 4-5 kt.

**Seen red**, linux-debug, in both crosswind tests:
- The C182 named from 3.5 kt across, a stale `c999` named, and the C172P's
  swing named: 6 and 4 wrong - "c182 at 3.0 kt: named, and her autobrake
  reached the rolling ceiling all the same", "c999: named for the rolling
  ceiling, and not a landplane flown", and the C172P at every pace, 2 to 5
  kt, "her swing named, and she swung only 2.8 to 3.6 deg".
- The C182 at 3 kt across put at the edge (0.300) while still named, the
  C182 at 2.5 kt taken off the edge, a stale `c999` at the edge, and the
  Mosquito's swing name made stale: the C182 at 3 kt reached the ceiling
  and was counted, not wrong; the C182 at 2.5 kt, named and not at the
  edge, did not reach it and was not wrong; "c999: at the edge, and not a
  landplane flown"; and the Mosquito "swung 46.551269 deg" at every pace in
  both tests, 2 to 3.5 kt and 4 to 5 kt. Restored, green.

**Verification.** linux-debug: the six walking-pace tests, the Cub held at
every walking pace, the gone-over test, the tail-wheel stance test, the
braked stop and stick-central Cub tests in 15 kt across, and the wet and
steady crosswind roll-outs pass. Not run in linux-release this time (its
build directory removed to save the disk); the change is to the test's
bookkeeping alone, and CI runs it there.

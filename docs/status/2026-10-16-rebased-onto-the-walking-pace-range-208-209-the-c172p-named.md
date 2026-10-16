### Rebased onto the walking-pace range (#208, #209): the C172P named at 3 kt across with her figure, the tree green, 2026-10-16 — item done

**What is still not true first.**
- Everything the entry below names as not true still is (the wings' drag,
  lift not keyed to the angle of attack, the Mosquito's slipstream lift, the
  learnt landing's policy flying a C172P it was not trained on), but its
  first point: the walking-pace test it turned red is gone, reshaped by #208
  and #209, and the six that replace it pass.
- **The C172P at 3 kt in 15 kt across is now named, not at the edge.** Her
  autobrake reaches 0.286 there, 0.014 short of the rolling ceiling - not
  within the edge's 0.01, so no longer a run that may reach it on another
  platform. That is this change's: her lift no longer lifts her off her
  brakes in the beam wind (up to 240 lb on main), and she stops a little
  sooner.

**Rebased** onto main after #208 and #209. PROJECT_STATUS.md took both
sides, theirs below this one, mine below theirs; nothing else clashed. Every
make script writes the committed models unchanged.

**What the six walking-pace tests say on this tree** (linux-debug and
linux-release alike, every run of every landplane flown, all six passing):
- **Dry, 2 to 3.5 kt and 4 to 5 kt**: as on main. Named from 3.5 kt, with
  the figures main gives: the C172P 0.287 at 3 kt, the F-15C 0.288, the Cub
  0.278, the Mosquito 0.287. The PA-28 reaches the ceiling at 3 kt (0.300).
- **Wet, both ranges**: as on main. Named: the Cub 0.278 and the Mosquito
  0.287 at 3 kt; the C172P and PA-28 reach it from 2.5 kt.
- **15 kt across, both ranges**: the C172P at 3 kt reaches **0.286**, where
  main's model reached 0.296 and the test had her at the edge; she is named
  with 0.286 and taken off the edge list. The F-15C (0.272) and the Cub
  (0.27) named as on main, the Mosquito's swing named as on main, the C182 at
  2.5 kt at the edge as on main (0.293). **The PA-28 reaches the ceiling at 3
  kt (0.300)**: under the old single 3 kt test her 0.295 was a handing-over
  of another shape, and is not a run of these. No bound or rule is changed.

**Seen red**: the C172P taken out of the crosswind names, the 2 to 3.5 kt
test fails on her alone: "c172p at 3.0 kt: her autobrake reached only
0.286143 rolling at walking pace: the rolling ceiling is not tested".
Restored, green.

**Verification on the rebased tree** (linux-debug): the 148 figures print
exactly what they printed before the rebase, every one; the 156 crosswind,
ground, braked, learnt-landing, take-off and wet-runway tests pass, every one
printing the same but the two that need the DEM, which now has it and runs
them (the S.23 from the sea and a lake, the ground's water mask); the 30
committed-model, term-check, selftest and measured-landing tests pass. The
selftest hash is `07cc7d4a4d42a300`, as before. In linux-release the six
walking-pace tests and the Cub's held-at-every-pace test pass, the C172P at
3 kt across 0.286 and the PA-28 0.300 there too.

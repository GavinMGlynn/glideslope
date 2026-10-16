### From the review of #203: the Cub's braked runs judged by outcome, her calm-air nose-over named, the helper told what to move, 2026-10-15 — item still open; one found

**What is still not true first.**
- **She noses over in calm air, braked stick central from 10 kt at half
  brake.** Solo she lifts her tail at 0.5 and goes over from 0.6; trainer
  goes over from 0.5; gross from 0.6. Her tip ratio stick central is 0.45
  to 0.50, and her braking friction at 0.5 is 0.41 - near it, not past it.
  Whether a real Cub would is not measured: a new Phase 9 item, "The J-3 Cub
  braked stick central from 10 kt noses over in calm air". The entry below
  said only that her tail "lifts" there.
- **At those levels the crosswind runs are counted, not judged.** Held to
  calm air's outcome, and at every one so held (32 runs) calm air's outcome
  is going over, so the hold is empty. The test says so.
- **Three runs, not one, are named for her lift**: braked at 0.5 in 15 kt
  across, at her gross weight from the right her tail lifts for a moment
  where in calm air it stays down, and solo from either side she goes over
  where in calm air her tail only lifts. With her lift terms on `qbarUW`
  too (tried, the entry below), all three kept her tail down. The gross run
  is held to stopping on her wheels; the solo runs' outcomes are printed,
  not held.
- **Her roll at 3 kt walking pace in 15 kt across turns on her propwash
  pitch term**: left on the whole dynamic pressure, with only `Pitch_alpha`
  moved, she rolled 20.8 degrees and struck her airframe 8.48 s after the
  hand-over, standing (pitch 14.1, roll 20.6); moved, she rolls 5.4; on
  main, with neither moved, 3.0. So `Pitch_propwash` stays moved - and that
  her roll there turns on it is added to the open roll item, not answered.
- The other fifteen models are as the entry below says, not done.

**The changes.**
- **Outcomes, kept apart**: each run ends tail down, tail lifted and
  stopped on her wheels, or gone over (struck, past 45 degrees, or no
  number), worst last. Standing and in the run-up her tail stays down in
  every wind; braking in 15 kt across, she does no worse than calm air at
  the same loading and level. The calm outcomes are pinned level by level.
  The printout says, per loading, where she goes over in calm air.
- **The lift run is no longer pinned to lift**: one tenth of brake from the
  edge, that would flake between platforms. It is held to stopping on her
  wheels, and whether it lifted is printed.
- **The open item named by its real name**, "The J-3 Cub is rolled onto her
  wing by a crosswind at walking pace", in the test and its printout.
- **The helper moves only what it is told**:
  `alpha_rate.with_alpha_terms_on_qbar_uw(text, script, {axis: [names]})`
  (was `with_pitch_alpha_on_qbar_uw`, which moved every term in PITCH that
  read alpha). It fails if a named axis is missing, if a named function is
  not in it, does not read the angle of attack, or has no `qbar-psf` or
  `qbar-area` to replace. Each failure was seen: a missing name, a missing
  axis, `Pitch_damp` and `Pitch_alphadot` (no alpha), and a name in the
  wrong axis. Its docstring and the module's say that which terms move is
  the script's to choose and justify: a damping term (`Cmq`), an elevator
  term (`CmDe`) or a swept wing's (the F-22A, F-15C, B-2A, F-35B, where issue
  #89's guidance was not investigated) is not in because it reads alpha. The
  Cub names `Pitch_alpha` and `Pitch_propwash`; the model is unchanged.
- **make_j3cub.py's docstring**: `Pitch_propwash` did not cause the fault
  (with `Pitch_alpha` alone, the air's most nose-down moment standing in 15
  kt across is the same 37 to 239 ft-lb), and why it is moved all the same
  (above).
- **The plan**: the Cub item cut to its missing part; the roll item gains
  the propwash finding; the calm-air nose-over item added.
- **Commit b585cbe1's trailers**: the review read it as missing
  `Co-Authored-By`; `git log` shows both trailers on it, so it is not
  reworded.

**Verification.** linux-debug: the new test, the three walking-pace tests,
the gone-over test, the brake sweep and the Cub's committed-model test pass.
**Seen red, braking**: with her brakes at 1.5 times the level in the
crosswind runs only, 12 failed - solo and trainer at 0.3 and 0.4 from either
side and gross at 0.4, their tails up where calm air's stay down, and the
gross lift run gone over. Reverted, green. **Seen red, the model**: with
main's Cub in the build's copy, all 60 crosswind runs failed again, her tail
up standing. Restored, green. In linux-debug the 83-test set of the entry
below passed but for the walking-pace crosswind test, and that only with
`Pitch_propwash` left on `qbar` (the finding above); with it moved, as
committed, it passes. In the `-march=x86-64-v2` debug build and linux-release,
the 26 tests of the Cub, the walking pace, the tail wheels, the crosswind
stops and the brake sweep pass, the outcomes the same (the gross lift run
lifts and stops, the solo ones go over, the calm outcomes as pinned).

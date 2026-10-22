### A STOVL prediction's law changes stage on the server's step, 2026-10-22

**What is still not true first.** The server still tells only the height
asked and the wing-borne autopilot's pitch integral of her law; the
jet-borne and hover laws' own state is each side's own (the item "A
client's prediction of an F-35B in STOVL mode drifts from the server's
flight-control laws", worked next on this branch).

**The cause.** On the same inputs the prediction's law went jet-borne 3
steps after the selector, the server's at 9 s when the fan's doors were
open. A replay flown from a word placed before the selector put back the
motion and the replay state of that step, but the lift system's commands
are not in the replay state - the commands are the controls', set before
every step - and while no law flies her nothing sets them
(`Controller::lift` is null, and she keeps what she was last given). So
the steps before the selector were flown again with the fan's doors
commanded open, as the newest step had them: 0.0094 open by the selector's
step, where the law made again found them part-open and was made
jet-borne. Its hover began 1.2 s sooner the same way.

**The change.** `Aircraft::replay_state` keeps the lift system's seven
commands (`fcs/stovl-cmd-norm` and the rest, the names `set_lift_system`
now sets from one list). Nothing on the wire changes.

**The test.** `a_stovl_prediction_sets_with_her_stick_what_the_server_sets_in_every_stage_on_every_inceptor`
(#236's) now counts, on every server step, whether the prediction's law on
the same input is in the server's law's stage, and the server's changes of
stage: 48,228 steps, none apart, 3 changes (jet-borne, hovering, jet-borne
again), asserted. The converting stage's stick fore-and-aft, left out until
now, is compared: 780 steps, all moved, none apart. Coverage: 10 of the 16
stages and inceptors compared, 6 not read. The pedals' jet-borne turn,
0.63% apart before, is within 1.8e-5 of the server's.

**Verification** (linux-release):

- The test above green.
- **Seen red** with the lift system's commands not kept: the prediction's
  law in another stage on 1,104 steps. Reverted: green.
- The STOVL, prediction, transition, F-35B, hover and replay tests pass
  (`ctest -R "stovl|prediction|transition|f35b|hover|replay"`).

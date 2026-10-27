### The STOVL at-once tests' predicted pitch: her law's pitch trim the server's, 2026-10-28

**What is still not true first.** The item stays open until CI's full run
shows the three at-once tests' pitch on every platform; on this machine
(linux-release) it is 0.001 degree where it was 0.203.

**The cause.** A transition law keeps, from the controls it is made from,
every control it does not fly itself - her pitch trim among them - and flies
her on them for as long as it flies her. The server made a player's
controller on the step her STOVL selector arrived from `held`, which by then
was her first input: its pitch trim nought. Her client's prediction made its
own from the controls it had flown her on the step before - the levers the
server had held her on, whose pitch trim is what her trim found, 0.0746. So
the two laws flew her wing-borne on trims 0.075 apart for the whole flight,
and every replay from a server's word pitched away from the server at a
steady 0.0055 degree a step: 0.20 degree here, where a replay is about 25
steps, and 0.58 and 0.96 on CI's Windows, whose replays ran to 71 steps.

**Found** by printing both sides' whole state at the same server step - the
motion, the controller's pilot and applied controls, the transition law's
and its autopilot's every member, and every number in her JSBSim property
tree - the server's after it flew the step, the client's as a replay began
from the server's word for that step, and diffing them. With the motion
equal to the bit, the controls the autopilot hands on differed in pitch trim
(0 against 0.0746), yaw trim, the gear and the selector; the pitching moment
was -412 lbf ft on the server and +2,679 in the replay. The rest - the
autopilot's integrals, its trends, the FCS's values - differed by parts in a
thousand, the law made a step apart.

**The fix.** The server keeps what each aircraft was flown on last step
(`flown_on`: `held`, or what its controller made of it) and makes the
controller for her selector from that, then gives it her input - as the
prediction's is made. And `Prediction::held_by_server` makes the
prediction's controller from the levers it is told, as the server's is from
what it held her on: a prediction whose very first step selected STOVL had
otherwise made its law from a model just loaded.

**Not answered: a client never told her levers.** The windowed client asks
for them as it joins and gives up after its timeout; without them its
prediction's controller is made from nothing (pitch trim nought), while the
server's is now made from the trimmed hold (0.0746). Before this change the
two agreed on the trim there by accident - both nought - and disagreed on
the throttle the law was seeded with. The client has nothing to make hers
from until it is told, so the case is named here and in the plan item, not
fixed: such a client's law flies her on another trim than the server's, and
its replays pitch away as the at-once tests' did.

**Verification.**
- `server_stovl_at_once.cmake` (the three at-once tests): the server says the
  pitch trim its law flew her on the step it was made, the client the trim
  its law was made with and the trim the law flying her at the end flies;
  all three must be equal to the printed 17 digits, and the server's not
  nought, so that two noughts cannot pass it. **Seen red** with the
  server's controller made from `held` again: "the prediction's law flew her
  on a pitch trim of 0.074587237159337141", the server's 0, and the pitch
  0.201 degree. With the fix: 0.074587237159337141 on both, and her
  predicted pitch within 0.001 degree of the server's over 3,000 updates.
- `a_prediction_told_what_the_server_held_her_on_seeds_a_law_made_on_her_first_input_as_the_server_does`:
  the server's controller made as the server now makes it, from what she
  was held on; its law flies her on her held trim, and the prediction told
  those levers flies her on the same. **Seen red** both ways: before
  `held_by_server` made the controller (the prediction's trim nought), and
  with the server's controller made from her first input.

**Measured here**, linux-release, the plain at-once test: before, 0.201 to
0.204 degree on every run; after, 0.001.

### A STOVL prediction flown on the wrong share of the stick is caught, 2026-10-22

**What is still not true first.** What the stick sets while she converts is
not compared. On the same inputs the prediction's law is jet-borne from
1.5 s and the server's converting until 9 s, so no step finds both
converting. Her hover begins 1.2 s sooner in the prediction too. **The plan item
"No test catches a STOVL prediction flown on 0.8 of the stick" is therefore
not done and stays open**, with the converting stage named as the missing
part. The test names the converting stage's fore-and-aft as left out,
with that reason, and checks it once the stages agree.

**The bug.** A prediction flown on 0.8 of the stick the client sent the
server passed every test (2026-10-15, 2026-10-16). In STOVL mode the stick
asks for height, and the server tells the height asked at every word, so the
prediction was put right and flew to a median of 2 mm through a server.

**The test** (`a_stovl_prediction_sets_with_her_stick_what_the_server_sets_in_every_stage_on_every_inceptor`,
tests/unit/test_prediction.cpp). The server's controller and a client's
prediction are fed the same inputs, 100 ms apart each way. The server's word
comes back every fifth step with the height asked and the law's carried
state, as through a real server. Each side's law is read before and after
each step on the same input, and they are compared:

- the stick fore-and-aft: the step's change of the height asked;
- the lever: the step's change of the speed asked;
- the stick sideways: jet-borne, the step's change of the bank held; in the
  hover, the ground speed to the side asked;
- the pedals: the turn asked, jet-borne and in the hover.

Two flights cover it. The heavy F-35B is held wing-borne in STOVL mode. Then
at her vertical-landing loading she converts, slows jet-borne, hovers, is
moved in the hover and goes jet-borne again on the lever. Each stage takes
every inceptor both ways. **Coverage asserted:** of 16 stages and inceptors,
9 are compared, at least a second each with the server's law moved by the
inceptor. 1 is left out (converting, above). 6 are not read by her law
(wing-borne and converting forward, only the fore-and-aft is hers).

Two values agree if they are within 2% of what the server's set (or 1e-9).
That is a tenth of what the bug puts them apart. Measured in linux-release:
every value agrees to the last bit except two. The lever's speed is within
2.4e-12 of it. The pedals' jet-borne turn is within 0.63%, because it falls
off with her speed along the heading her law holds, and that heading is not
told.

So that the law can be read, `sim::Transition` gains `bank_held_deg()`,
`pedals_turn_radps()` (the pedals' share of the jet-borne turn, now held in
its own member, its arithmetic in the same order) and `hover_asked()`. Nothing it
flies changes.

**Verification.**

- Green in linux-release (5 s).
- **Seen red** with the bug put into `sim::Prediction::step`: the elevator
  and aileron flown at 0.8 of what was sent. The fore-and-aft (wing-borne,
  jet-borne and hovering) and the sideways (jet-borne and hovering) were
  apart on every step compared, by 0.2 of what the server's set.
- With the pedals and the lever at 0.8 instead, it was red on the pedals
  (jet-borne 720 of 720 steps, hovering 1,200 of 1,200) and on the lever in
  the hover (460 steps).
- With the stick bug in, the old height test through a real server,
  `a_players_stovl_selector_through_a_server_holds_a_heavy_f35b_wing_borne_and_the_client_agrees`,
  **passed**: median 0 mm, 95th percentile 1 mm. That is the gap this test
  closes.
- The in-process `a_prediction_whose_stovl_law_was_seeded_off_...` also went
  red with the bug in `Prediction::step`. The bug of 2026-10-15 was in
  glideslope_cli, which no in-process test runs.
- Bug reverted: green again.
- The STOVL, prediction, transition, F-35B and hover tests pass in
  linux-release.

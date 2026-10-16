### #194's STOVL selector: the server tells a client the height her law holds, 2026-10-16 — a red PR's cause fixed; one part left

**Superseded in part by the entry above** (2026-10-16, the pitch
integral): the residual below was shown to be the wing-borne autopilot's
pitch integral, and is now told. As it stood:

**What is still not true first.** **The rest of her STOVL law's state is
still each side's own.** The transition law, its autopilot and their
integrals are seeded on each side from her as it sees her at the selector,
and when the selector reaches the server a step later (or earlier) than
the clocks' difference places it, they are seeded a step apart, diving, and
stay so: the prediction error's median through a server is then 14 to 37
mm, not 0 to 1 - in eight idle runs of the multi-process test, 0, 0, 1, 1,
14, 37, 37 and 37 mm (a ninth, 36), the corrections once joined at most
9 mm in a 37 mm run.
Flown in one process with the server's selector a step late, the median was
10 mm against 0 in step; consistent with the cause, not shown to be it. This
is COMPLETION_PLAN's open item "A client's prediction of an F-35B in STOVL
mode drifts from the server's flight-control laws", now narrowed. The test's
5 cm median bound is met, but by 13 mm at the worst run.

**What was wrong.** With the clocks' difference exactly right and the steps
matching, the multi-process test
`a_players_stovl_selector_through_a_server_holds_a_heavy_f35b_wing_borne_and_the_client_agrees`
showed a **steady** prediction error, the median up to 108 mm and the worst
165 mm under load. In sim::Controller, STOVL mode selected seeds
`stovl_height_ft_` from her height above the ground as that side sees her at
that step, and the stick then moves it alike on both. The server's words put
the prediction's motion right, and sim::Prediction puts back the
controller's state it kept with each step - its own, never the server's - so
what the prediction's height was off by at the selector it asked for as long
as the law flew her. ddc9f8fa made the test select once the prediction had
settled (37 mm under load, 3 mm idle): a test choice, not the fix.

**The change.** Protocol version 13 (`0D`): a client's own motion
(net::OwnMotion) ends with `stovl_height_asked_ft`, an `f32` after the steps
into the input - the height above the ground the transition law is asked to
hold, ft, or exactly -1 when no law flies her lift system; NaN, infinities
and any other value below nought are refused, written or read. The server
fills it from the player's controller (`Controller::stovl_height_asked_ft()`
while `transition()` is not null) at the same moment as the motion.
`Prediction::reconcile` takes it (`std::optional<double>`, none at -1) and,
after the controller's state kept at the word's step is put back, puts the
height asked to the server's before the steps since are flown again
(through `PilotsLaws`, the one place the state lives) - **only where a law
flies her in the state put back**: where the server's law flies her and the
prediction's does not yet, or no longer, the two disagree about the
selector or its moment, there is no law to tell and a law made on the next
step is seeded afresh anyway; the next word that finds both flying one puts
it right. `glideslope_cli` and the windowed client (`Flight::reconcile`,
`Online`'s word) pass it on. TRANSPORT.md gives the field, the fourteen
numbers, the full packet's 1,142 bytes (1,172 sealed), and `0D`; the client
written from it reads the field.

**Verification** (linux-release and linux-debug):

- `a_prediction_whose_stovl_law_was_seeded_off_is_put_right_to_the_servers_height_asked_and_then_agrees`:
  the F-35B at 45,400 lb, held wing-borne, trimmed at 1,000 ft and 200
  KCAS over flat ground, the prediction begun 2 m above the server's and
  again 2 m below (both signs, counted), told nothing until both selected
  STOVL mode at one second (seeded 6.56 ft off, asserted), then the stick
  half forward a minute, the server's word every fifth step 100 ms each way
  with the height rounded to an `f32`. All 1,436 words had a law on both
  sides (asserted equal to the words heard); the worst correction over
  1,411 words once the clocks' difference was known 0.0000 m (bound 1 mm)
  and the heights asked at the end 0.0000 ft apart (bound 0.001). **Seen
  red** with the height not put on (a deliberate bug in
  `Prediction::reconcile`): corrections to 5.2 mm and the heights asked
  6.56 ft apart at the end; reverted, green.
- `a_state_packets_stovl_height_asked_is_none_at_minus_one_and_refused_below_nought`:
  thirteen values, counted - -1, 0, -0, 1e-30, 100, 450.25 and the largest
  float read back the same, -1 alone as no law; one float either side of
  -1, -0.5, -2, the least negative and the most refused by the writer and,
  put in the bytes, by the reader. The NaN-and-infinity walk goes to 24
  fields and 72 cases; the own-motion test pins the steps and the height as
  the motion's last six bytes; the envelope test refuses `0B` and `0C`; the
  gearstick refusal is `474c44530d0401`; the doc agreement and the
  version-with-the-ground tests updated.
- **The multi-process test**, linux-release, settled selection (kept):
  idle, the median 0, 0, 1, 1, 14, 37, 37 and 37 mm over eight runs, the
  95th percentile 1 to 50 mm, all passing. Selected at the 150th input
  whatever the settling (as before ddc9f8fa; built for the measurement and
  reverted) with 40 busy loops beside it (twice the cores, killed by their
  PIDs): the median 1, 25 and 37 mm, against 108 mm before the change. Every
  idle run selected with input 150 anyway: the prediction settles before it
  here. Settled selection is kept as a test choice - built, not hoped for -
  since the law's other state is still each side's own. In linux-debug,
  idle: the median 3 mm. A ninth release run, in the full set: 36 mm.
- The net, protocol, state, transport, prediction, F-35B and STOVL tests,
  the gearstick refusal and the multi-process test: 120 of 120 in
  linux-release and in linux-debug.

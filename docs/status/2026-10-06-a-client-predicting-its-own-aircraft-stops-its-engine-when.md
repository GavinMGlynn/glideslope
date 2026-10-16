### A client predicting its own aircraft stops its engine when the server says one has, 2026-10-06 — closes a tail

**What is still wrong first.** **Only the first engine stopped is named**
(below, from the review): a twin with both stopped is told of one, the other
left to its own model - which stops it too when its fuel runs out as the
server's does. **The window client** is wired the same way (`client::Flight`)
and flown by no test of it. **The worst error is not what the median is**:
half a second at a time, every few seconds, a predicting client is put right
by a metre or more whether its engine has stopped or not - 1.6 m in 40 s of
this flight with none, 5.2 m once with the engine stopped and 1.2 m once with
it run on - and the worst is what that made it. That is the clock estimate's
open tail ("A server that falls behind real time..."), not this one.

**What was wrong.** The prediction's flight model ran its engine on after the
server's stopped - a failure is the server's alone - and the client was put
right by the thrust it did not have, correction by correction: the median
prediction error after the stop was 0.18 m against 0.04 m before.

**What it does now.**
- `sim::Prediction::hear_engine_stopped(bool)`: told the server's word before
  it reconciles, so that the inputs since are flown again with the engine
  out, it stops engine 0 (`fail_engine(0, false)`) when the server says one
  has stopped and none has here; and starts it again
  (`sim::Aircraft::restart_engine`: ignition or fuel on, running,
  unfeathered) when the word says none has - an aircraft flown again after a
  wreck - but only one it stopped for that word. `sim::Aircraft::any_engine_stopped()` is the
  test both sides use: the server's `Fleet::engine_stopped` now calls it
  rather than reading each `set-running` property (the same thing).
- `glideslope_cli`'s predicting client and the window client
  (`client::Online`, `client::Flight::reconcile`) pass their own aircraft's
  condition from each update.
- `glideslope_cli connect --predict --until-engine-compared N` stays until N
  updates have been compared since the server said its engine had stopped,
  and the client reports its worst error and correction before and after,
  how many engines it stopped, and the median error before and after.

**Verification.**
`a_client_predicting_its_aircraft_stops_its_engine_when_the_server_says_and_is_put_right_no_more`
(`tests/cmake/server_engine_prediction.cmake`, port 24803, relay 24804):
`glideslope_cli connect --predict --until-engine-compared 300` | server
`--fail-engine-at 20` | relay at 250 ms each way. It holds that the client
stopped one engine for the server's word, compared 300 updates after, and
that its median prediction error after the stop is no more than its median
before or 2 cm: 14 mm against 42 mm here (the worst 1.05 m and 0.08 m, for
the reason above). 36 s (Linux debug). **Seen to fail** with the prediction
never stopping its engine: "with the engine stopped the median prediction
error was 181 mm, more than the 42 mm before it or 2 cm"; reverted.
`a_prediction_stops_its_engine_on_the_servers_word_and_starts_it_again_on_its_word`
(unit): stopped once for the word, kept stopped through a second of flying,
started again on the word of none; one stopped by itself is neither stopped
again nor started for the word. **Seen to fail** with the start taken out
("and starts the one it stopped again"); reverted.

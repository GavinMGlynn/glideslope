### A player who joins selecting STOVL has her law seeded as the server's, 2026-10-21

**What is still not true first.** Only the throttle integral, and the
other controls her law is seeded from (the stick, the pedals), are made
alike; what the law reckons from her motion at that step - her speed, her
attitude - is each side's own, and the prediction's motion at its first step
is the server's word from before her first input arrived, not the server's
motion at that input. How far that leaves the two apart is not measured
here. The windowed client is changed the same way (it already waited for
the server's levers before its first input) and compiled, not run.

**The cause.** The transition law is seeded from the flight-control
commands she has when it is made (sim::Controller, "seeded from what she
flies"). On the server, a law made on her first input was seeded from what
it had held her on until then - since 2026-10-20 her trim's controls. In the
prediction, from a model just loaded: a throttle of 0 where the server's was
0.4685. There was a second fault behind the first: ReplayState does not
carry the commands (every step sets them again), so a law made in a step
flown again was seeded from the *newest* step's commands - each word heard
before her first input reached the server put her back to her first step and
made the law again, from what the law itself had flown a round trip later
(0.216 and 0.229 against 0.4685, below).

**The change.**

- `sim::Prediction::held_by_server(const Controls&)`: what the server holds
  her on until her first input, put on her before the prediction's first
  step. Each step held keeps the controls on her as it began
  (`Applied::commanded_before`), and a replay puts them back on her with the
  controller, after the replay state - only for an aircraft whose controller
  flies a lift system, the one place a step reads them before setting them.
  `sim::apply` returns the controls it put on her.
- The server holds a player's aircraft on `held_until_moved` as her levers
  say it (`WATCH`): each control a 16-bit fraction, as an input is, so that
  what it says is what it holds to the bit. Held to the double, the
  prediction's integral was 0.4685201574755089 and the server's
  0.46852824379049418. The server now says when it makes a player's STOVL
  law, at which step, and its throttle integral.
- `frontend::held_as_told` (src/frontend/held_levers.hpp) turns the levers
  into the controls; the windowed client calls `Flight::held_by_server` with
  the levers it already asked for at joining.
- `Transition::throttle_integral_made_with()` and
  `Autopilot::throttle_integral()`, read only. Nothing in the jet-borne
  lateral law was touched.
- `glideslope_cli connect --stovl-land --stovl-at-once` selects STOVL mode
  with its very first input and joins as the windowed client does: it asks
  for its own levers, sends nothing until it has them, begins its
  prediction on them, and says the throttle integral its law was made with,
  on which input, and that of the law flying her when it leaves.
- No message changed: TRANSPORT.md is as it was.

**Verification.**

- `a_client_selecting_stovl_on_its_first_input_seeds_its_transition_law_with_the_servers_throttle_integral`
  (tests/cmake/server_stovl_at_once.cmake): a real server, the F-35B at her
  model's own loading at 2,200 ft over Sydney, 100 ms each way through the
  relay, STOVL selected with input 1, staying until 250 updates are
  compared. linux-debug: the server's law made at its step 216 with
  0.4685201574755089, the prediction's at its step 0 with the same, and the
  law flying her at the end made with the same. Seen red three ways, each
  reverted: `held_by_server` doing nothing (0 against 0.4685...); the replay
  not putting the commands back (the law flying her at the end made with
  0.2289); the server holding her unquantised (0.4685201574755089 against
  0.46852824379049418).
- `a_prediction_told_what_the_server_held_her_on_seeds_a_law_made_on_her_first_input_as_the_server_does`
  (tests/unit/test_prediction.cpp): in one process, the server's F-35B
  trimmed and held a second, its controller made on her first input as the
  server makes it, with 0.46852824379049418; a prediction on a model just
  loaded, told those controls, made with the same and made again with the
  same after a word that put her back to her first step; not told, made with
  0 - the situation pinned, asserted. Both cases counted. Seen red with
  `held_by_server` doing nothing, and with the replay not putting the
  commands back (made again with 0.2164).
- The 112 tests matching prediction, STOVL, untouched, held and selftest,
  linux-debug, all green. The replay's commands put back move the STOVL
  tests through a server only within their spread:
  `a_players_stovl_selector_through_a_server_holds_a_heavy_f35b_wing_borne_and_the_client_agrees`
  median 0 mm, 95th percentile 1 mm, worst 1.264 m, pitch within 0.028
  degrees; the vertical landing median 0 mm, worst 20 mm, pitch 0.038; the
  untouched F-35B 1,828.8 to 1,830.7 m, the C172P to the tenth of a metre.
- `glideslope_cli selftest`: hash 831a4e7954233b0a in linux-debug. It flies
  the C172P through no prediction, no server and no lift system, none of
  which this changes.

**Rebased onto #238, 2026-10-23.** Two things broke, both seen on CI (run
39790592126) and here. The test's port, 24996, was taken by #238's stall
test meanwhile: it is 24998 now. And #238's command-line client flies what
is owed on the input before each new one; before the first input there is
none, and it flew those steps on its own default controls, so her law was
made with a throttle integral of 1 against the server's 0.4685201574755089.
Before the first input she is flown on the levers the server said it holds
her on (`held_or`), as the server flies her: the test passes again, as do
#238's stall test and the prediction tests beside it.

### A prediction come down on water flies again when the server does, 2026-10-23

**What is still not true first.** The NaN that aborted the run was not
reproduced here, and its cause is unknown. The item closes, by the
owner's decision of 2026-10-23, on a guard and a trap instead (below):
a prediction given a NaN or an infinity is let go of before JSBSim takes
it, and a recurrence in CI or the nightly says where the NaN was made and
which numbers went with it. A NaN made *inside* one JSBSim step, from
finite inputs and a finite state, and looked up by a table in that same
step, is before the guard can see it: that one still asserts in a debug
build, and only the trap's account of it is new. What was found, on the same
path, is a prediction held on the water's surface for good after its first
water wreck; that is fixed. Whether it was the NaN's cause is not shown.

**The failure.**
`a_client_predicting_its_aircraft_stops_its_engine_when_the_server_says_and_is_put_right_no_more`
on Ubuntu linux-debug, run 39883628869 (PR #247), job 120195354289. The
client's Cessna, her engine stopped by the server at 20 s, glided into
Sydney Harbour: "is a wreck: came down on water", "flies again", twice, and
then glideslope_cli asserted in JSBSim at `FGTable.cpp:618`
(`Factor >= 0.0 && Factor <= 1.0`): a table looked up at a NaN.

**What was wrong.** `Aircraft::step` holds an aircraft that meets water
down (JSBSim's hold-down: no force or moment acts, its velocity and rates
held), and only `Aircraft::initialize` let it go. The server initializes
an aircraft it flies again; a client's prediction is only put to the
server's motion (`Aircraft::set_motion`). A prediction runs ahead of the
server's word, so it met the water before it heard the wreck, and was held
down from then on: flown again at the server's word, every step after flew
on its velocity alone, put right at every word.

**The fix.** `Aircraft::set_motion` releases the hold-down: the motion set
is the server's, at another moment than the step that met the water, and a
step that meets the surface again holds it down again.

**Verification.**
- `a_prediction_come_down_on_water_flies_again_when_the_server_does_and_never_goes_to_a_nan`
  (tests/unit/test_prediction.cpp) builds it: a Cessna over water, her
  engine stopped on the server and heard stopped, dived in, wrecked, five
  seconds held, started again at 2,200 ft and flown ten seconds - twice.
  The prediction is asserted to have met the water before the wreck was
  heard. Green: no NaN, never held on the water in 593 words flying again,
  6.5 mm from the server at worst in the last seconds. **Seen red** with
  the release removed: held at 593 of 593 words, 3.7 cm off.
- The run itself, reproduced: server, relay (250 ms each way) and client as
  `server_engine_prediction.cmake` runs them, but the client kept on to
  280 s (`--until-engine-compared 100000`) so that both water wrecks come
  (wrecks at about 127 s and 216 s, respawns 1.5 km away). Unfixed and
  fixed, no assert in this debug build; after the first respawn the median
  error 7.6 cm against 7.5 cm, the worst 0.60 m against 0.19 m.
- Unit and prediction tests green locally (`-R
  prediction|predict|wreck|motion|replay|put_right|put_back|reconcil`).

**Not reproduced: the NaN.** Neither the unit test nor the full run, both
unfixed, gave a NaN in this machine's debug build - which uses FMA
(x86-64-v3), where CI's does not. Nor did an x86-64-v2 debug build
(`build/v2`, `-march=x86-64-v2`), unfixed: the unit test (held at 593 of
593 words, no NaN) and the full run four times to 280 s, both water wrecks
in each - two side by side, two pinned together to one core with their
server and relay, the frames up to 95 ms; CI's were up to 2.4 s. Searched the web, JSBSim's discussions included
("Crash Detection", #819), on FGTable's assertion, NaN after a hold-down
and re-initialising after a crash: nothing that names this.

**A second symptom, not this path.** Run 39887624529 (PR #236), job
120202399092: the same test failed with 287 updates compared after the
engine stopped, not 300, the worst correction 1,148.756 m. That
correction is the respawn's snap (1.5 km here). But the comparisons had
stopped long before any wreck: the client's machine stall at 62 s had
none compared in the 60 words after it, and the first water wreck comes
about 107 s after the engine stops. The client frames were up to 2.4 s
(1.4 s its own) and it was flying at 1.57 of its machine's clock at the
end: a client that could not keep up on a loaded runner, which a release
of the hold-down does not touch. That is what #245 ("A starved predicting
client reconciles against the newest update only, and catches up")
answers; it is not yet on #236's base.

**Second round, on #245's main: the NaN still not reproduced.** Every
run below had the client's floating-point *invalid* exception unmasked
(a preloaded `feenableexcept(FE_INVALID)` with a SIGFPE backtrace), so
the first NaN made anywhere in it - not only one that reached a table -
would have stopped it where it was made. That trap was seen to fire: a
NaN made by 0/0 in a thread of an ASan program aborted it with the
trace; nothing in `src/`, JSBSim or SimGear resets the floating-point
environment. The runs are the scenario as `server_engine_prediction.cmake`
runs it (relay 250 ms each way), the client kept to 520 s so that the
water wrecks come, each run's server and impair unpinned:
- **This branch, debug (FMA)**, client `--slow-steps 2000`: 3 wrecks
  heard and flown again, no trap; 280 s plain, 1 wreck, no trap.
- **This branch, x86-64-v2 debug (no FMA)**, `--slow-steps 2000`:
  3 wrecks, no trap.
- **This branch with the hold-down release taken out** (the code CI
  ran in that respect), debug, `--slow-steps 2000` and `3500`: 3 and 2
  wrecks, no trap. The unit test, unfixed and trapped, fails only on its
  held count: no trap.
- **CI's own commit** (e365c623, run 39883628869's: before #245, before
  this fix), debug, its client pinned to one core with 1, 2, 3, 6 and 10
  spinning loops on it: 5, 5, 5, 5 and 4 wrecks on the server; the
  client's longest frames 82 ms, 219 ms, 1.5 s, 0.85 s; no trap in any.
  At 10 loops the client heard its engine stop and **none of the four
  wrecks** - as CI's did, whose log has no wreck heard by the client -
  and was flying at 2.0 of its machine's clock, its corrections stopped
  at 41: no trap there either.
- `--slow-steps 6000` on this branch is past what this client can fly
  (its clock ran to 447 s while the server's was at 6 s, and it left at
  its 520 s): no wreck reached, no trap; not a test of this.

Searched again: JSBSim's #809 (the same assertion, an F-16 diverging in
pitch and crashing; not found), #815 and #818 (NaNs after hard impacts:
"compresses its gears and contact points to a ridiculous amount",
bcoconni), #1495 (a SIGFPE on a propeller's first step, closed
unreproduced). The assertion at FGTable.cpp:618 is the one-dimensional
lookup, and a key that is NaN is the only one that fails it: every
comparison is false, so the search stops at the first breakpoint and
the factor is NaN. Which table, and which input made the NaN, remain
unknown; the item stays open.

**Closed on a guard and a trap** (the owner's decision, 2026-10-23; the
cause unknown).
- **The guard** (`sim::Prediction::lost`): every input a prediction puts
  on JSBSim - each step's controls, the server's motion it is put to - is
  looked at before it is put, and the motion each step leaves after it is
  flown, forward or again. The first NaN or infinity stops the prediction
  flying, and says what it was (the control by name, the motion's
  component, and the first JSBSim properties not finite, by name) and at
  which of its steps. `glideslope_cli` then lets it go as after a take-back:
  a new aircraft, started from the server's next word; it says so once for
  each thing a prediction went to, with the step and the aircraft, and
  counts every one ("let go of: N predictions ..., M updates compared since
  the last"). A server's word not finite is not started from. The window
  client (`client::Flight::let_go_if_lost`) takes the server's motion back
  on the aircraft it has, not a new one; that path is not under a test.
- **The trap** (`--trap-nan`, `platform/nan_trap.hpp`): with glibc the
  floating-point *invalid* exception is unmasked, so the first NaN made
  stops the client with a backtrace of where (its symbols exported on
  Linux, so JSBSim's functions are named); everywhere an abort - JSBSim's
  assertion - prints the same. Either way every number of the predicted
  aircraft not finite is printed by its property name. Set in
  `server_engine_prediction.cmake`, the run that failed, and in the new
  test; both print the programs' standard error when one fails, so it
  lands in the test log, on CI and in the nightly's repeats.
- **Verification**:
  `a_prediction_given_a_nan_is_let_go_of_and_its_client_flies_on_from_the_servers_state`
  (`tests/cmake/prediction_let_go_of_a_nan.cmake`): the client gives its
  prediction alone a NaN elevator at 10 s (`--test-nan-at 10`), the trap
  set, through a 100 ms relay. Green: all three programs exit 0, no trap,
  the let-go said once for "the input's elevator", and 288 of 500 updates
  compared since - it flew on from the server's state. **Seen red** with
  the guard taken out of `Prediction::fly`: the client stopped on SIGFPE,
  "nan trap: a NaN made here", the backtrace through
  `JSBSim::FGFCSComponent::Clip`, and "nan trap: fcs/elevator-cmd-norm"
  as the number gone - the account a recurrence will give. Guard put back,
  green again.

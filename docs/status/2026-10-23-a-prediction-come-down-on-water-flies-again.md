### A prediction come down on water flies again when the server does, 2026-10-23

**What is still not true first.** The NaN that aborted the run was not
reproduced here, and its birth is not found. What was found, on the same
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

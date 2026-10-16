### Prediction, interpolation and the player limit through a worse network, 2026-09-24 — item done

**What was built.** The Phase 6 item "Network checks in CI with injected
latency, loss and jitter".

- **`glideslope_impair`** (`tests/tools/impair.cpp`) is a UDP relay that
  delays, jitters and drops datagrams both ways, drawn from a seed. It gives
  each client a socket of its own towards the server, so the server still
  tells them apart. It does what `tc netem` does, without needing root or
  Linux, so the same check runs on every CI platform. It stops when its
  standard input ends, which, last in a test's pipeline, is when the server
  has gone.
- **`glideslope_cli connect --predict MODEL`** is what a real client does,
  measured. It flies its own JSBSim on a pilot whose controls never stop
  changing, and reconciles it from each update's own-motion block. It draws
  every other aircraft 100 ms behind at 60 Hz. `--track FILE` writes down
  every update heard and every frame drawn.
- **`glideslope_interpolation_check`** judges one client's frames against
  another client's updates. That other client is connected straight to the
  server and hears everything.
- **The tests** are
  `prediction_interpolation_and_the_player_limit_hold_at_100_ms_with_jitter_and_loss`
  and `..._at_200_ms_...` (`tests/cmake/server_impaired.cmake`):
  - 100 ms with 30 ms of jitter and 5% loss; 200 ms with 60 ms of jitter and
    10% loss.
  - Each runs a server with room for two, the reference client, the predicting
    client, and a third client through the relay that must be refused as full.

**The bounds, and one run's figures (Linux, debug build).**

| | bound | 100 ms | 200 ms |
|---|---|---|---|
| prediction error, worst | 8 m / 10 m | 4.6 m | 4.6 m |
| correction, worst (none may snap) | 20 m | 3.7 m | 5.1 m |
| interpolation error, worst | 2 m | 0.052 m | 0.171 m |
| frames carried on past the newest update | some | 20 | 94 |
| the third client | refused as full | refused | refused |

The test also asserts:

- that the relay lost datagrams both ways;
- that at least 150 updates were compared and 600 frames judged (a Windows
  client, whose shortest sleep is longer, drew 1,492 where Linux drew 2,200);
- that some frames were carried on past the newest update, so that the
  guessing and the blending back were judged, not only interpolating between
  two updates;
- that joining took no more than 30 inputs.

**Each was seen to fail** on a deliberate bug, and each bug was reverted:

- reconciliation that replays nothing: a worst prediction error of 30.7 m;
- an interpolator that draws the present instead of 100 ms ago: every frame
  was 4.8 m out;
- a player limit off by one: the third client was let in.

The 20 m correction bound and the relay's loss check were not made to fail.

**What was learned on the way.**

- **Correction size cannot see a client that lags.** The first measure was the
  size of each correction. With the replay deleted, it still passed: a client
  put back to each update, which never flies forward again, is always behind
  by the latency, but only a little behind the previous update, so its
  corrections are small. The prediction error is now measured directly. Where
  the server says the aircraft was on input *k* is held against where this
  client had flown it on input *k*.
- **A client that joins is a trip behind, and says so.** The first update is a
  trip old, and the server flew the aircraft for that trip before any input of
  the client's arrived. The client now keeps the inputs it flies before it has
  an aircraft and flies them forward from the first update. The inputs flown
  before the server has applied any are counted and not compared (12 to 17 at
  200 ms).
- **The first interpolation judge judged itself.** Held against the updates it
  had heard, a client can only agree with itself, because what it lost it
  never knew. The judge is now the reference client that hears everything.
  For its first 100 ms, an aircraft is held where it was first heard to be
  (`net/interpolation.cpp`, by design). Those frames, six to eight a run, are
  counted and left out.
- **Corrections are metres, not the millimetres measured in-process.** The
  server applies an input when it arrives, so an input made late by jitter is
  flown late. The server also does not say how far into its latest input it
  had flown. At 50 m/s, 60 ms of jitter and an input's thirtieth of a second
  are 5 m. That is what the bounds were set from, with room for a slow
  runner, and it is a tail in `COMPLETION_PLAN.md`.
- **An update that arrives after a newer one was put right by** (found by the
  review). The relay reorders updates when the jitter is wider than the 40 ms
  between them. The client put its aircraft back to the older update and could
  not fly the inputs since, because the newer update had already released
  them, so it ended up an update behind. It now uses only an update newer than
  the last one it used. No test pins this on its own: the figures it moved sit inside
  the bounds either way.
- **What the predicting client does not do yet.** It flies a Cessna because it
  is told to (`--predict c172p`), not because the server says so: nothing on
  the wire names an aircraft's model. It flies over flat ground at sea level,
  not the DEM. Both belong to "Four machines in one sky", where the client
  with the window flies on a server. The measured figures also depend on
  three programs keeping time on a loaded machine, as a real network does;
  the nightly run repeats them five times.
- **A client whose time is up stops predicting.** Since the input test's fix
  it waits, making no new inputs, for the server to apply its last one. A
  prediction keyed by input can only replay nothing there, and fell a round
  trip behind (31 m). It no longer predicts or measures once it is waiting.
- **A program in a pipeline that outlives the next one is killed.** In a
  CMake pipeline each program's standard output feeds the next. A client ahead
  of one that left sooner died of SIGPIPE when it printed. The pipeline is now
  ordered by lifetime, and the test holds every exit code (the refused client
  1, all others 0).

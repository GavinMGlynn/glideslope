# A player joining after another has left is always put on a clear layer: its test waits on events

The server was right. The test was timed by the clock, and it now waits on
events.

## What was wrong

On 2026-11-13, nightly dispatch 43507588675 (branch
nightly-multiprocess-shards, linux-debug, shard 5 of 7) ran
`a_player_joining_after_another_has_left_is_put_on_a_layer_no_aircraft_near_the_start_holds`
five times. The second run failed:

    server_rejoin_layer.cmake:63: 2 players' starts were said, not the three joins
    ...
    admitted 9e3a15de to slot 0
    admitted 727f6d39 to slot 0
    refused 127.0.0.1:44268: the server is full, ...
    let go 127.0.0.1:58202 after it said it was leaving, 6.556 s after it was admitted

All three clients started with the server, each on a clock:
- A stayed 6 s;
- B joined 1 s in;
- C joined 12 s in.

A's 6 s count from when it was admitted. C's 12 s count from launch. A
debug server on a loaded runner took more than about 6 s to start flying.
So A was still in when C knocked, and the server, with room for two
players, refused C as full. The rule the test is for was never tried.

## Reproduced

The test's script was run with a server that starts 7 s late
(scratchpad wrapper: `sleep 7; exec glideslope_server "$@"`). This stands in
for a slow debug server on a loaded runner.
- Before the fix: 3 runs out of 3 failed with CI's message, "2 players'
  starts were said", and C was refused as full.
- The same test under load did not reproduce it: 15 repeats, `-j4` with
  its two layer siblings, pinned to four cores. Locally the server starts
  too fast.

## The fix: tests/cmake/server_rejoin_layer.cmake

Each client now waits on the one before:
- A joins once the server says it is flying (`--after-ready` on the
  server's `--ready-file`). It leaves once B is in (`--until-exists` on B's
  `--told-file`) and writes `--done` when it has gone.
- B joins once A is in, and stays until C is in.
- C joins once A has gone, and leaves once it is in.

The test now also checks that the situation was built. The server's
starts and lettings-go must come as start, start, let go, start: C joined
with A gone and B still at the start. It also checks that B is not on A's
layer, and that C is above B. Before, it only checked that C was not on
B's layer.

The run is shorter as well, with no 40 s stay: about 4.5 s locally on
linux-debug, where it took 44 s before. The cost tables in tests/ci_costs/
still hold the old figures. They only raise its time limit, and the next
re-measure brings them down.

## Verification

- **With the slow server, after the fix:** 13 out of 13 passed (A, B and C
  on layers 3, 4 and 5).
- **Under load, after the fix:** `--repeat until-fail:15`, `-j4`, on four
  cores, with its siblings: all passed.
- **A deliberate bug turns it red.** `free_layer` was made to skip its
  held-at-start search. The test then failed: A, B and C started on
  layers 3, 4 and 4, at "not above B's". The bug was reverted.

No server code changed. The selftest hash does not move.

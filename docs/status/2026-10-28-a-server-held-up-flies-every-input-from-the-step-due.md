### A server held up flies every input from the step due when it came, and lets slip only what it cannot catch up, 2026-10-28

**What is still not true first.** A server stopped alone still puts its
client metres off at worst (4.9 m after 3 s, Linux debug): the inputs that
came in the stall are read in one pass and the newest alone is flown, so
none of the stick's moves in those 3 s is flown on the server - 4.4 m off
the path the client flew. Spacing them as the client flew them needs inputs
that say which of the client's steps they began on, a protocol change the
project owner put off on 2026-10-24 (the first-input nudge); it is the
next step here. A server persistently behind is no better or worse than
before (its worst 2.1 to 2.6 m in a 40 s run, either way). The window
client's 6 to 13 m, and Windows' two-step spread, are untouched. So the
Phase 12a item stays open.

**The mechanism, measured.** An input read while the server owes steps was
flown from the step it had flown to plus what it owed, **but at most 30 on**
(`net::most_steps_held`). After a hold-up longer than a quarter of a second
the inputs read were flown early by the rest; the client's clock difference
for them (where the server began the input, less where the client did) fell
by it, and the least of two seconds of words kept that low value while the
server caught up, placing every word that far off. Then, the server caught
up at four steps a pass and every input read meanwhile was flown early by
what it still owed over 30, falling as it caught up: the difference rose and
fell (the "1.4x" of #252's round 4). Linux debug, the paced test's setting at
real time's pace, a 3 s stop of the server alone: placed 329 steps off,
the worst 103.9 m.

**The rule** (src/net/catching_up.hpp, net::CatchingUp; HeldInputs stamps
at the step due, never sooner). What is owed is caught up while the server,
flying flat out, takes steps faster than they come due - measured over the
last second of its passes that left steps owed, a pass of a quarter second
or more counting only after another as long (a lone one is a hold-up, not a
pace), and taken as able until a quarter second of it has been seen. When it
is not catching up, what it owes past 30 steps slips: its clock falls behind
the wall by it for good, and a client sees its clock difference fall, which
the least of its words takes at once. Never more than 420 owed (3.5 s, inside
the 4 s of inputs a client keeps). A slip costs a correction of the flying
not done, where catching up costs none, so a stall is caught up and only a
pace is slipped - as Gaffer's "Fix your timestep" gives a loop headroom to
catch up and slows it when it has none. The server no longer sleeps 2 ms in
a pass that leaves steps owed. The steps let slip are flown at the end with
the rest owed, so that a run still says it flew the time it was given.
No protocol change: what a client hears is unchanged, only the steps the
server flies inputs from.

**Directions weighed.** (a) Slipping everything over a quarter second and
telling the clients: a slip is the server's aircraft not having flown those
steps, so a 3 s stall slipped is a correction of 2.75 s of flight, 137 m at
this speed - worse than the 104 m it replaced; telling the client the slip
does not move the aircraft back. Kept only for a server that cannot catch
up, which today's rule already did in effect. (b) A clock-difference
estimator that tolerates early samples: the samples were not noise but a
real change in which step the server flew the inputs from, and its rise
during catching up is what the least of two seconds cannot follow; flying
them from the right step makes the samples right. (c) Inputs naming their
step (Overwatch's command frames, Source's user commands): the robust end,
and what the remaining 4.4 m needs; put off, above.

**Verification** (Linux debug; figures per run of 600 updates through a
100 ms relay, the paced test's flight).

| server | main's code: 95th / 99th / worst | now |
| --- | --- | --- |
| stopped alone 3 s, pace 1 | 15.3 / 55.8 / 103.9 m | 0.62 / 3.6 / 4.9 m |
| held up 150-600 ms every second, pace 1 | 9.7 / 13.0 / 14.9 m | 0.35 / 1.2 / 1.9 m |
| whole machine stopped 4 s (the stalled test) | across the stall 6.9 m | 1.07 m, back within a metre from word 4 |
| persistently behind, `--test-step-ms 10` (0.76) | 0.47 / 2.1 / 2.6 m | three runs: 0.44 to 0.98 / 2.1 to 2.6 / 2.1 to 2.6 m; 766 slips in one |
| paced (0.6) beside 40 busy loops on 20 cores, one run each | 0.43 / 2.0 / 2.0 m | 0.43 / 2.6 / 0.7 m outside a 303 ms stall of the client's |

- `a_client_predicting_against_a_server_stopped_alone_for_seconds_is_within_two_metres_for_ninety_five_updates_in_a_hundred`
  and `..._held_up_unevenly_every_second_...` (server_paced_prediction.cmake,
  `-DSERVER_HELD=alone|unevenly`, the new `--test-held-up EVERY_S MS`): the
  paced test's bounds; stopped alone, the 95th percentile held to 2 m and
  the worst to 8 m (the named limit above: the 75 words of the server's
  catching up are each off by the stick's moves it did not fly - CI's
  Windows debug, run 40621099406, 1.51 m and 4.7 m on the first draft). Seen red with main's stamping (`stepped + min(owed, 30)`): 95th
  percentile 24.8 m and 9.3 m, worst 104 m and 16.0 m.
- `a_server_held_up_once_and_fast_enough_after_catches_up_every_step_it_owes`:
  stalls of 0.3, 1, 2 and 3.5 s caught up with none slipped, and 5 s slips
  the 180 steps over 3.5 s - five of five. Seen red with owed always slipped
  past 30: the 0.3 s stall let 6 slip.
- `a_server_persistently_behind_lets_what_it_owes_past_a_quarter_of_a_second_slip`:
  four steps a pass at 34, 45, 90 and 400 ms - four of four paces - never
  owes more than 30 and a pass's steps, the rest slipped; a 2 s stall then
  0.74 of real time is slipped within two seconds. Seen red with the server
  always taken to be catching up (74 owed at 0.98, growing); and the 400 ms
  pace owed the most caught up, 420, until a run of long passes counted as a
  pace.
- `an_input_heard_while_the_server_owes_steps_...`: owing 200, flown from
  300 (was 130).
- **Windows debug** (`tools/windows_build.sh`, this machine): the paced
  test, the machine's stall, both new multi-process tests and both new unit
  tests pass. `an_input_heard_while_...` printed `passed` and then the
  executable was stopped by an access violation in a thread after the
  program began to exit (0xc0000409) - the exit seen before on this machine
  and not on CI (Webroot), the same executable whose other two tests passed.
- **Not changed**: a busy machine. Beside twice as many busy loops as cores,
  one run each, the paced test's figures are the same within a run's spread;
  that item's step-level spread is a different mechanism, and stays open.
- The related tests, Linux debug (`-R` first_input, impaired, starved,
  server_behind, behind_real_time, rate_limit, take_over, take_back, stall,
  held_inputs, owes, slip): 187 of 187 passed.
- **The full suite**, Linux debug at `-j4`, before rebasing on #252: 1,385
  of 1,386 passed (the network-and-key tests skipped as ever).
  `a_client_selecting_stovl_on_its_first_input_seeds_its_transition_law_with_the_servers_throttle_integral`
  failed once beside the rest and passed run alone (128 s); its log was
  overwritten before it was read, so whether this change or the load moved
  it is not known - a thing to watch on CI.
- `--test-say-slipped` says each slip on the error stream (a test's server
  runs in a pipeline whose output is not read).

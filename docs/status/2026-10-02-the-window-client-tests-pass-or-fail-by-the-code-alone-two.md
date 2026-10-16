### The window-client tests pass or fail by the code alone: two of their three flakes fixed, 2026-10-02 — tail still open

**What is still missing, first**: the month of runs is not counted (it
starts with this change), and a third flake is not fixed: the window
client's prediction is now and then put right 20 to 31 m after a long frame
(below; a tail of its own in COMPLETION_PLAN). The windows-release fix is
argued from the runs' own numbers and not reproduced here: no machine here
is a four-core Windows runner.

**The survey.** `gh run list --workflow ci.yml --limit 80` reached back to
2026-10-01 05:09 UTC: 73 runs finished, 18 passed, 19 failed, the rest
cancelled by a newer push. Of the 19 failures, six were the branch's own
code, failing on every platform (the server.txt window test on three
branches, a shadowing error on runway_ground.cpp twice, the mixture branch's
ceiling and stall tests), and two were GitHub's network (a 504 fetching
ccache or a pinned download on Windows, twice; macOS unable to fetch
SPIRV-Cross). **The other 11 failed only on flakes**, all window-client
tests:

| Failure | Test | Job | Times |
|---|---|---|---|
| "cannot reach" the server | rides along in an AI aircraft | linux-debug | 3 |
| " | refused by a forger ... goes back to it | linux-debug | 2 |
| " | flies the server's aircraft and draws the others | windows-debug | 1 |
| frames of 67 to 102 ms around a switch | hands its aircraft to the AI and takes it back | windows-release | 4 |
| " | takes over the AI aircraft it rides along in | windows-release | 1 |
| put right 20 to 31 m, too far to hide | flies the server's aircraft and draws the others | linux-debug | 3 |
| " | frames held most of a second, never put right too far | windows-release | 1 |

**The start-up race (six failures).** Each of these scripts started the
server and the window client at once. The server binds its port, then builds
its terrain and aircraft, and only then answers a handshake; the client's
handshake gives up five seconds after it begins. A debug server sharing a
runner with three other tests took longer: in run 36860567602 the client
wrote "cannot reach 127.0.0.1:24711" before the server wrote "listening on
port 24710", and the server then admitted it - the initiation had waited in
its socket - after the client had gone. Every one of the six logs has
"cannot reach". Not a port collision: each test has its own port
(test_ports.cmake), and the server did listen on it.

**Fixed** by waiting on the event: the window client takes `--after-ready
FILE`, as `glideslope_cli connect` already did, and connects only once the
file exists; the scripts give the server `--ready-file`, which it writes
once its aircraft fly. Every script that runs the window client against a
server with aircraft does so: rides along (both tests), on a server (both),
forged refusal, hands over, copilot, and joins again (both). The online-
window test's server flies nothing and answers at once; it is left alone.
From the review: a relay between server and client (`glideslope_impair`,
in the forged-refusal, hand-over and joining-again scripts) counts its
limit from its own launch, not from the flying, so those limits go from 290
to 600 s, past the server's 300 from the flying; each still ends when the
server's output does. (The command-line client's relay scripts keep 290.)

**Reproduced, before and after**, in linux-debug with the server pinned to
one core shared with three busy loops (`taskset -c 3`, started and killed by
PID; port 31788, so as not to meet another working copy's tests): the rides-
along test as it was failed 5 runs of 5, each "cannot reach 127.0.0.1:31788";
with the fix it passed 5 runs of 5 (some 8 minutes each, the server at a
quarter of a core).

**The windows-release frames (five failures).** The two switch tests assert
the 20 fps their bounds are claimed at, and failed it. What the client said
of the longest pass around the switch is the evidence: in four of the five
that pass took 3.0, 4.5, 6.4 and 18.7 ms of frames of 72, 71, 67 and 102 ms
(runs 36850472653, 36850385103, 36855488898, 36859801694) - the client was
not working for most of the frame, but waiting for a core, while three other
tests shared the runner's four, one of them drawing on the software
renderer. In the fifth (36866316016) the render took 72.9 ms of 76.8, which
is the same contention inside the GPU driver's threads. **Fixed** by building
the situation the bound is claimed for: the two run `RUN_SERIAL`, alone on the
runner, as the server take-over tests already do. Each takes 40 to 55 s on
windows-release, so a shard waits that long twice. The bounds are unchanged.

**Not fixed: the prediction put right too far to hide (four failures).**
`flies_the_servers_aircraft` on linux-debug (runs 36859801694, 36866316016,
36866464324) and the held-frames test on windows-release (36855488898) were
put right 22.6 to 31.0 m against a 20 m bound, once in a run, each time after
a long frame (142, 284, 167 and 730 ms): about one long frame's travel at the
Cessna's 37 to 59 m/s. It is not the machine's alone - the held-frames test
says the bound holds at any frame rate, and mostly it does - so it is not
moved to another tier; nor is it a start-up race. A guess not yet tested: the
clocks' difference (sim::Prediction::hear_clock, the least of those heard) is
set by where an input began here, and an input that began at the head of a
long pass's catch-up ticks says the clocks differ by up to that pass. It
needs a test that reproduces it before a fix; it is a tail of its own.

**Verified**: the reproduction above; `ctest -L timing -N` still lists three
tests, two of them now RUN_SERIAL; the window-client tests whose scripts
changed pass in linux-debug (ctest -j4, DISPLAY unset): the six non-timing
ones, the copilot one at the second attempt - the first time another working
copy on this machine held its port 24714 ("cannot listen"), and the client
waited out its five minutes for a ready file a dead server never wrote,
failing as it should - and the three timing ones, one after another.

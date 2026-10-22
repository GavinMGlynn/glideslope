### A machine's stalls counted apart, and the prediction back within a metre after one, 2026-10-22

**What is still not true first.** A server stopped while its client runs on
still puts the client tens of metres off (55.6 m after 3 s): the inputs that
came meanwhile are flown at most a quarter of a second late
(net::HeldInputs). That is Phase 12a's "A server that falls behind real time"
and is not fixed here; a server stopped alone is not something a client can
see, so its updates are not counted apart. The client with the window names
its stalls and counts their corrections apart, but no test builds one and
watches it recover: stopped by hand its run ended within the stall's words.
A blocking wait of a client's own that it does not time (a GPU readback,
an untimed file read) is counted the machine's.

**The failures, from their logs.** Each shows the client not run for seconds
between two frames, medians of 0 to 10 mm either side:

| test | run | worst | longest frame |
|---|---|---|---|
| paced prediction, windows-clang | 39701548318 | 26.3 m | 2,756 ms |
| paced prediction, windows-release (#231) | - | 138 m | 3,583 ms, 3,503 the machine's |
| paced prediction, windows-clang (#233) | - | 29 m | 1,017 ms |
| take-over at 100 ms, windows-clang | 39701548318 att. 2 | 53 m (41 m correction) | 1,267 ms |
| STOVL vertical landing, windows-clang (#228) | 39701548318 att. 3 | 6.2 m | 5,214 ms |
| held frames (window client), windows-release | 39686519535 | 52.6 m | 3,613 ms against 700 held |

The builds before #229 do not say the machine's share; #231's does: 98% of
the frame was not the client's.

**What stops it, measured by stopping programs by PID** (SIGSTOP/SIGCONT,
3 s, Linux release, one run each): the client alone 0.44 m, the relay alone
1.0 m, the server alone 55.6 m, all three 4.5 m. The CI runners stop all of
them. After a whole stall the server catches up the steps it owes - a word
every 4.8 - and the word placed by the inputs it then flies early puts the
clocks' difference (the least of 50 words) 21 steps low until it leaves the
window.

**The rule** (src/frontend/machine_stalls.hpp, both clients): a frame
0.25 s or more of which, and more than half, was not the client's thread's
CPU time, its sleeps or its timed writes is the machine's stall. The words
about a session time before the client's reckoning at the frame after it -
the server's catching up - and the 60 after (offset window 50 + pace window
10) have their updates and corrections counted apart and named: count, the
worst, the corrections and any too large to hide, and from which word after
it the error was back within a metre. Every word after is judged as before;
medians and percentiles are of all updates. 0.25 s is `most_steps_held`:
shorter, the server flies the inputs where they came.

**Verification.**

- `a_client_predicting_against_a_server_behind_real_time_whose_machine_stops_for_seconds_is_back_within_a_metre_sixty_updates_after`:
  the client (`--stopped-once 12 3500`) and server (`--test-stopped-once`)
  each run nothing for 3.5 s at 12 s on the session's clock. Requires exactly
  one stall of 3.4 s or more named, all 60 words after it heard, back within
  a metre by the last, and the paced test's bounds on everything else.
  Linux release: 53 words catching up and 60 after, 113 updates apart, the
  worst 5.6 to 6.3 m, back within a metre from the 4th to 8th word after;
  elsewhere the worst 0.475 to 0.945 m, the median 2 mm (four runs).
- Seen red, each reverted: the stall made a busy wait of the client's own -
  0 stalls named, the worst 5.56 m; and the server's word misplaced 30 steps
  on alternate inputs after the stall, a recovery that never comes - 9.6 m
  after the window.
- The paced, take-over, STOVL and window-client tests say every stall; the
  window client's 700 ms held frames are its own sleeps and count none (seen:
  0 stalls), and its client and server stopped 3 s by hand: one stall of
  2,728 ms named.
- CI's Windows test jobs now print every stall from the test log, and whether
  Defender scans in real time on the runner. It does not: on the
  windows-2025-vs2026 image (20261007.264) `RealTimeProtectionEnabled` is
  False, so Defender's scanning is not what stops them, and no exclusion was
  added.
- On CI's windows-clang (run 39733569271) the built stall was harder: 52.2 m
  across it, one correction too large to hide, back within a metre from the
  55th word of 60 - inside the 50 + 10 words the rule is derived from - and
  0.536 m at worst after. Its steps were 41 apart across the stall (3,262 to
  3,221), the pace steering by a clocks' difference the stall had moved; the
  steps are now summed outside the machine's stalls (Linux: 2,326 to 2,328).
- Before making an input the CLI flies what is owed on the input before, as
  the server does through its silence: 0.584 to 0.491 m worst through a stall
  of the client alone (one run each), a small effect, not what failed on CI.

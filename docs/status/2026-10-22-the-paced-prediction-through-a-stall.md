### The paced prediction through a stall, 2026-10-22

**What is still not true first.** The item is not done. The CI failures are a
whole machine stopping - client, server and relay at once - and through that
the prediction is still metres off: stopped together 3 s by hand on Linux
release, 4.5 m at worst against the test's 3 m. Nothing here pins or bounds
that. What is pinned is the client stopped alone.

**The failures, from their logs.** All four show the client not run for
seconds between two frames, with medians of 2 to 3 mm either side:

- windows-clang, run 39701548318 (attempt 1, job 119576705703): worst 26.3 m,
  95th 2.4 m, the longest between frames 2,756 ms (a build before the
  machine's share was printed).
- windows-release on #231: 138 m, 3,503 of 3,583 ms between frames the
  machine's, as the client's thread CPU time counts it (platform::thread_cpu_s).
- windows-clang on #233: 29 m after a 1,017 ms gap.
- `a_client_whose_frames_are_held_most_of_a_second_is_never_put_right_too_far_to_hide`,
  windows-release run 39686519535: the windowed client, 52.6 m once, its
  longest frame 3,613 ms against the 700 ms the test holds - the same
  machine stall, in the other client; not worked here.

So each is the machine, not the client: the client's own time between those
frames was under a tenth of the gap.

**Which stall does it, measured by stopping each program by its PID**
(SIGSTOP/SIGCONT, 3 s, 20 s into the paced test, Linux release, one run each):

| stopped              | worst error | test   |
|----------------------|-------------|--------|
| the client alone     | 0.44 m      | passes |
| the relay alone      | 1.04 m      | passes |
| the server alone     | 55.6 m      | fails  |
| all three together   | 4.5 m       | fails  |

The client recovers from its own stall. The server does not fly a stall's
inputs where they arrived: those read in one pass after it are all stamped
due at most a quarter of a second on (net::HeldInputs, `most_steps_held`),
which that file names as a limit. The whole machine stopped is the CI case;
its 4.5 m (and CI's tens of metres, after longer stalls and Windows' 15.6 ms
timers) is where the server's and relay's catching up meets the client's.
Which part of that is the prediction's real behaviour and which a recovery
that could be better is not yet established.

**What landed.**

- `glideslope_cli connect --stopped-once S MS`: runs nothing for MS
  milliseconds, S seconds in, uncounted as its own - a stall built, not waited
  for.
- `a_client_predicting_against_a_server_behind_real_time_stopped_alone_for_seconds_is_still_off_by_centimetres`:
  the paced test with the client stopped 3.5 s; the same bounds held - worst
  0.491 m, 95th 0.253 m, median 2 mm, steps 2,875 to 2,875, through a frame of
  3,518 ms. It also checks the stall happened (a frame of 3.5 s, 3.4 s of it
  the machine's): seen red with the stall set to 1 s ("its longest frame
  1016 ms"), and restored.
- Before making an input, the client flies what is owed on the input before,
  as the server does through its silence. Without it the stopped test still
  passed, at 0.584 m worst (one run): the order matters little with inputs a
  thirtieth of a second apart, and it is not what failed on CI.

**Decisions needed.** Whether a whole machine's stall is judged by the test
(then the server's handling of a backlog of inputs is the work) or excluded
from it on the client's own time (updates across a frame mostly the
machine's counted apart, and how many named).

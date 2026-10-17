### The engine-stop prediction test does not depend on the client's pacing, 2026-10-17 — item done

**What is still not true first.**
- **The prediction itself still depends on the pace.** A server held up by
  fits and starts moves the least of the clocks' difference by tens of steps
  for seconds, and the client's prediction is placed that far off: with the
  server stopped for up to 800 ms at a time here, the median distance after
  the engine stopped was 1.5 to 7.3 m and the worst 26 to 34 m. Only the
  test stopped measuring that; the open items "On a busy machine the
  server's word is placed a step off" and "The engine-stop prediction test
  fails on Windows clang when the server is held up" (its month of CI runs)
  stay open.
- **The new figure has no CI numbers yet.** A passing CI run prints none of
  its figures (`outputOnFailure`); the bound rests on this machine's runs and
  on what every failing CI log says the old figure was made of.

**The cause.** The test held each of the server's words against the speed
of the step the clocks' difference placed it at
(`sim::Prediction::reconcile`'s `at_step`), and the clocks' difference is
the least, over 50 words, of where the server began an input less where this
client did. A server behind real time by fits and starts - a loaded Windows
runner - moves it by tens of steps for seconds, and the client's pace loop
chases it (paced at 0.87 to 1.69 of its clock in the failures). A word held
against a step that far from its own pays the speed's change over those
steps, and with the engine stopped the Cessna is slowing into her glide, so
after the stop that costs more than before. Every failing CI log since
2026-10-07 says so: the median *distance* after the stop, 0.43 to 12.9 m
(1 to 28 steps at her speed), against 0.04 to 3.8 m before.

| run | preset | paced at | off (worst) | client / server steps | median distance after, before | speed after, before |
|---|---|---|---|---|---|---|
| 37759639784 | windows-release | 1.014 | 2 (17) | 3588 / 3589 | 0.428, 0.077 m | (judged in distance then) |
| 37785824165 | windows-clang | 0.921 | -26 (-81) | 4588 / 4667 | 12.871, 3.020 m | 0.1786, 0.5179 m/s |
| 37798907886 | windows-clang | 1.690 | 34 (68) | 3986 / 3612 | 6.300, 1.398 m | 0.0962, 0.2607 m/s |
| 37882235513 | windows-clang | 0.943 | -15 (61) | 3864 / 3626 | 3.332, 3.830 m | 0.0541, 0.4071 m/s |
| 37891045143 #1 | windows-clang | 1.643 | 43 (63) | 3934 / 3938 | 5.083, 0.474 m | 0.1387, 0.1341 m/s |
| 37891045143 #2 | windows-clang | 1.283 | 50 (-72) | 3791 / 3778 | 4.271, 0.240 m | 0.0874, 0.1030 m/s |
| 38803123702 #2 | windows-release | 0.998 | 2 (-30) | 3715 / 3745 | 3.911, 0.280 m | 0.0798, 0.1454 m/s |
| 38980963376 | windows-clang | 0.873 | -32 (95) | 4343 / 4243 | 9.181, 0.039 m | **0.3298**, 0.1176 m/s |

(Read from 321 failed jobs' logs, every failed attempt of every run since
2026-10-07, `gh run view --log`. One more failure, 37925898269 on
linux-debug, was the client aborting, not a bound.)

**Reproduced here** by stopping the server (SIGSTOP) for 20 to 800 ms every
0.2 to 1.2 s, on linux-debug: paced at 1.19 to 1.33, the client flew 4290 to
4435 steps while the server flew 3606 to 3656, the median distance after
7.3 to 16 m against 0.04 to 0.26 m before - CI's signature. Stopped up to
280 ms, or the client or the relay stopped instead, or the whole run pinned
to two busy cores, nothing moved: the server's held inputs (a quarter of a
second, `net::most_steps_held`) cover those.

**The change.** The command-line client (`--predict`) now also holds each
word against the speed of the step it had flown **nearest to where the word
puts her** - where it was, not when - among the steps it keeps, now from two
seconds before the placed step on. A misplacing is taken out: the step
nearest in place is the one the server's word is about, whatever the clocks
said. An engine run on is not: she got there faster. A word whose nearest is
either end of the steps kept is left out and counted, as its nearest may lie
beyond. It says `engine stopped: the median speed error where it was ...`,
with how many steps from there the clocks' difference had placed the word.

The test (`tests/cmake/server_engine_prediction.cmake`) holds that figure:
after the stop under **10 cm/s**, no more than half again before, and at
least 270 of the 300 placed. The speed placed by the clocks and the
distances are reported, not held - they are the pace's - except slowed,
where the distances' 5 cm still catches a client that does not follow the
server's pace.

| run (linux-debug) | where it was after, before | steps off at median | by the clocks after | distance after |
|---|---|---|---|---|
| plain | 0.0054, 0.0996 m/s | 0 | 0.0054 m/s | 0.006 m |
| server stopped up to 800 ms | 0.0088, 0.0863 m/s | 15 | 0.0300 m/s | 7.324 m |
| the same again | 0.0058, 0.0686 m/s | 3 | 0.0227 m/s | 1.450 m |
| engine run on, plain | **0.3578**, 0.0985 m/s | 0 | 0.3549 m/s | 0.134 m |
| engine run on, server stopped | **0.3785**, 0.0630 m/s | 6 | 0.3539 m/s | 2.700 m |
| engine run on, slowed to 0.6 | **0.2282**, 0.0761 m/s | 0 | 0.2276 m/s | 0.038 m |
| slowed, client unpaced | 0.0375, 0.1588 m/s | 42 | 0.1574 m/s | 12.472 m |

**10 cm/s, why**: eleven times the worst passing figure here (0.0088), and
the least engine run on (0.228, slowed) is two and a quarter times it.

**Seen red.** The engine left running (`Prediction::hear_engine_stopped`
returning before it stops anything): red plain (0.3578 m/s, "not under 10
cm/s"), red with the server stopped by fits and starts (0.3785), red slowed
(0.2282); reverted, green. The slowed server with the client unpaced
(`-DSLOWED=ON -DUNPACED=ON`): red on the distances, 12.472 and 9.041 m
against 5 cm. The fix with the server stopped up to 800 ms: green twice,
where the figure the test held before was 0.0300 m/s by the clocks with a
median distance of 7.3 m - CI's failures' shape.

**Verification.** `ctest -R stops_its_engine` on linux-debug: both the test
and its slowed twin pass; the runs above were made with the same script by
hand (`-DSERVER_EXTRA`, `-DSLOWED`, `-DUNPACED`, and a SIGSTOP loop on the
server's process id).

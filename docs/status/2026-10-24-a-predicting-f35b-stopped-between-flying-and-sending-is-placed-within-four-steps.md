### A predicting F-35B stopped 3 s between flying and sending is placed within four steps, 2026-10-24

**What is still not true first.** Nothing of this item (ticked 2026-10-28,
round 5 at the end). What its evidence is not: round 4's reproduction - five
clients and servers, debug, on two CPUs - was not run again with the start
not owed; the evidence that a slow start no longer puts a client off is the
slow start built in the test, on Linux and Windows. And the windowed client's
test was seen red with its stamps wrong (962 m), not with the start owed:
the start's fix is shown red in glideslope_cli's test alone. A server's
session clock now stays behind the wall's by its start's length, for good;
nothing compares the two but the weather's timer, which counts on the
session's.

**What a plain stop does, and does not do** (release, F-35B from her held
trim, through a 100 ms relay; `scratchpad` runs): unstalled, the worst 0.000 m;
stopped 3 s at the top of a pass (`--stopped-once` at 1.2, 3 and 12 s) the worst
2.76 m, two steps; five such setups on two CPUs, two steps. A plain stop does
not do it.

**What does: a stop between flying and sending, on a client slow at her
prediction.** `--stopped-sending S MS` (new) stops after the steps owed on one
input are flown and before the next is sent, where a starved machine takes the
CPU away part-way through a pass; `--slow-steps 2000` makes every step 2 ms
dearer. Stopped 3,000 ms at 12 s on the session's clock: the worst 115.8 and
122.7 m outside the stall's words, 239.9 and 246.8 m across them; with the
committed test, 250.9 and 84.1 m.

**The cause, measured.** A word's sample of the clocks' difference was the
step the server applied an input at, less the step this client flew it from.
An input is sent once the steps owed on the one before are flown; stopped
between, it leaves seconds after that step, and the server applies it that
much later. Logged per word (`sample` in `--track`: the input, the sample
taken, how late it was sent, and the sample from where it began), across the
stall:

| input | steps sent late | sample from where it began | stamped at sending |
|---|---|---|---|
| 170 (before) | 3 | 119 | 116 |
| 171 | 198 | 315 | 117 |
| 172 | 123 | 239 | 116 |
| 173 | 96 | 212 | 116 |
| 176 | 20 | 136 | 116 |
| 189 | 11 | 127 | 116 |

The excess is the lateness, step for step. The least of 50 words
(`sim::offset_window`) ignores one high sample, but the slow passes after the
stall kept every input late: the window filled with them, its least rose 114
to 127 as the samples before it left, and each step it rose placed the next
words a step further off.

**The fix** (`sim::Prediction::sent`, `glideslope_cli`'s `Predicting::sending`):
each input is stamped, as it leaves, with the step this client's flying has got
to - the step it will be flown from plus the steps the paced clock has moved
since that was reckoned - and its sample is taken from there: the time it
spent on the network and in the server's queue. This is NTP's: RFC 5905
section 8 takes the transmit timestamp as the packet leaves, and the clock
filter (section 10) keeps the least-delay sample of its window; Mills' clock
filter page gives why - the offset error grows with the delay, so the
least-delay sample is the best. Once a client stamps, **an input it did not
stamp gives no sample** - the input in force when the prediction began, or one
sent before it existed: its lateness cannot be known, and a sample that may be
low as well as high is a spike, which section 10's popcorn spike suppressor
discards rather than trusts. A client that never stamps (the windowed client)
is sampled as before.

**And a gap is flown at the pace the loop learnt** (`sim::Pacing`,
`steered_without_a_word_s`, 0.25 s). With stamping alone, one run in seven
still placed words six steps off (8.27 m): the client was being steered 2%
fast (pace 1.0208) to close a two-step difference as it stopped, and flew the
whole 3 s at it, gaining eight steps on the server that no word asked for; the
words after were placed by a clocks' difference that had fallen 113 to 105.
The loop's proportional part asks for the time until the next word (40 ms);
past a quarter of a second without one the client now flies at the loop's
integral alone - the server's pace as learnt. `would_be_at`, which the stamp
uses, says the same without advancing the clock.

**Verification.**

- `a_predicting_f35b_whose_client_stops_three_seconds_between_flying_and_sending_is_placed_within_four_steps_travel`
  (tests/cmake/starved_f35b_placed.cmake): the F-35B at `--slow-steps 2000`,
  `--stopped-sending 12 3000`, on every run. It requires the stall named (2.9 s
  or more), an input sent at least 120 steps late, at most two machine's stalls
  (the one built and the slow catching-up pass after it, 486 to 833 ms seen),
  300 updates compared, and the worst, outside the stall's words and across
  them, within four steps' travel (5.51 m). Not `machine_stalls_check`'s "back
  within a metre": one of her steps is 1.378 m. Linux release, six runs with both
  fixes: the worst outside 0.041 to 2.764 m and across 0.035 to 1.381 m; 360 to
  364 steps sent late.
- **Seen red** with the stamp not given (main's sampling, the pacing fix in):
  250.888 m and 84.086 m, "past four steps' travel". Restored, green. Stamping
  without the pacing fix: green six runs in seven, 8.27 m in the seventh - not
  a red to rely on, which is why the unit test below pins it.
- `a_clocks_difference_is_sampled_from_where_each_input_was_sent_and_an_input_not_stamped_says_nothing`:
  three cases - an input sent 360 steps late samples as one sent at once, an
  unstamped input says nothing, a client that never stamps is sampled from
  where each input began. Seen red with the stamps ignored.
- `through_a_gap_without_a_word_a_paced_client_flies_at_the_pace_its_loop_has_learnt`:
  3 s flown as a quarter second steered and the rest at the integral's pace,
  and `would_be_at` agreeing. Seen red with the gap flown at the steered pace.
- The 35 pacing and prediction tests that run no server (`-R "pac|predict"
  -E "server|client"`) pass.

**The first-input test under load**
(`a_players_f35b_and_c172p_flown_from_their_held_trim_..._predict_alike`,
whose 95th percentile was 8.3 m once under load on this machine before):
five runs with the fix, Linux release, pinned to two CPUs with a busy loop on
them - all passed; the F-35B's 95th percentile 0.000 m and worst 0.0 to
1.377 m (one step), the C172P's worst 0.001 to 0.463 m. Whether the fix is
what covers it is not shown: the failure was not reproduced on main here, and
the debug build was not tried in the time.

**The client with the window, the same** (round 3). It sends a frame's input
before flying the frame's ticks, which its clock has already reckoned, so the
step its flying stands at as the input leaves is its prediction's plus the
ticks owed, plus whatever its clock moved on since they were reckoned:
`frontend::stamp_sent` (src/frontend/sent_stamp.hpp), shared with
glideslope_cli, through `Flight::sent` and `Online::fly`'s new steps owed
(`client::fly_a_pass` hands the link the frame's ticks). The first input,
flown at once, owes none. Its frame loop flies a frame's gap through
`sim::Pacing::flown_in`, as glideslope_cli's clock does. `--stopped-sending S
MS` builds the stop. Kept apart from #249's `OwnWords`: the change is in
`Online::fly`, `Flight::sent`, `pass.hpp` and the frame loop's clock, none of
which #249 moves. The window client's 28 tests (`the_client_with_the_window`,
`a_client_whose_frames`, the pass's unit test) pass.

**The debug build's 5 to 15 steps, explained** (round 4). Five clients and
servers, debug, pinned to two CPUs, with this fix: the worst 9.7 to 67.5 m
over fifteen runs, all 3.4 to 4.5 s in, each placed 5 to 14 steps along a path
flown alike (0.000 m off it), with no client frame over 98 ms and no input
sent more than 2 steps late - so neither the client's stall nor its sampling.
Logged per word (the sample, the window's least, the newest ten's, the pace,
the input's lateness) and per server pass (a probe, not committed: each pass
of 30 ms or more, with the server's clock and the wall's):

- **The server's first pass is its start, 1,656 to 2,202 ms under this load,
  and then it catches the time up**: its clock went 0.033 s to 2.200 s while
  the wall went 2.202 to 3.765 s - 1.4 times real time for 1.6 s. Its passes
  meanwhile were 22 to 46 ms. The client joins in that catch-up; the session
  clock's rate fit said 1.40 to 1.61.
- **The samples rise with it and fall after**: 300 at 1.76 s, 363 at 2.38 s
  (the server applying inputs at a clock running fast), then down to 334 by
  4.1 s as the client, held at 1.0 and steered to 1.22, flies after it.
- **The window's least lags the samples**: 318 while the newest ten said 340,
  then 321, 329, 333, 336 as the early samples left; each jump placed the
  next words 8 to 12 steps off (15.2 and 20.7 m in that run).

That is the server catching up steps owed - what #247 found, inputs flown
early through a pass over a quarter of a second - here the server's own start.
It belongs to Phase 12a's "A server that falls behind real time puts its
clients' prediction off by metres", which now names it.

**The selftest hash** does not move: the selftest flies `sim::selftest`, not a
prediction or a pace.

**A server's start is not owed** (round 5, 2026-10-28). The fleet's first
steps make what it flies; counted from the beginning of that pass, a debug
server on a loaded machine owed its 1.7 to 2.2 s start and caught it up at
1.4 times real time, while a client joined (round 4; CI run 40087273404, 23 m
off, and #240's first-input test on Windows debug, run 40541575889, 27.6 m
for a client joined 0.47 s in). Now the clock runs on from the end of that
pass (glideslope_server's loop). `--test-slow-start MS` makes the first steps
slow, for a test; `server_flags.cmake` knows it.

- `a_predicting_f35b_whose_client_stops_three_seconds_between_flying_and_sending_is_placed_within_four_steps_travel`
  builds the slow start, 20 s (`--test-slow-start 20000`), long enough that
  catching it up would take seconds on an idle machine. Linux debug: worst
  0.000 to 1.379 m over three runs, and 0.007 to 1.379 m over two pinned to
  two CPUs with two busy loops on them. **Seen red** with the start owed
  again: 1,662 and 1,677 m. A first version built the catch-up's slowness by
  slowing every step instead (`--test-step-ms 4`): red 136 to 167 m, but on
  CI's loaded Ubuntu debug the server so slowed fell behind real time and put
  her 10 steps off with the start not owed (run 40575068951) - Phase 12a's
  server behind real time, which the test must not build.
- `the_client_with_the_window_predicting_an_f35b_stopped_three_seconds_before_sending_is_placed_within_four_steps_travel`
  (`client_starved_f35b.cmake`, a GPU's): the windowed client stopped 3 s
  between reckoning a frame's ticks and sending, the server's start slow.
  **Seen red** on Windows debug with twice the steps owed stamped: 962 m.
- Their ports moved into the tests' block (24826, 24814): CI's port check had
  failed on 24600.
- **`--test-step-ms` waits out its time** (`take_at_least`): slept, Windows'
  15.6 ms timer made 4 ms a step 62 ms a pass of four, half real time, and the
  test failed there on a server always behind (29 m), not the one it built.
  It now sleeps while more than 20 ms is left and waits out the rest. Windows
  debug (`tools/windows_build.sh`): the starved test, the window's and
  `a_server_behind_real_time_hears_every_client_and_lets_none_go_that_is_joining`
  (30 ms a step) pass. The last 20 ms of each step are waited out with the
  CPU's yield, so a server given `--test-step-ms` keeps a core busy: the
  starved test is run alone (`RUN_SERIAL`), as the window's is, and
  server_behind.cmake's sleeps 10 of its 30 ms.
- This round is in this file, the item's own, as rounds 1 to 4 are.

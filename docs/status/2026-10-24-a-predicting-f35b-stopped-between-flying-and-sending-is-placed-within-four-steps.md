### A predicting F-35B stopped 3 s between flying and sending is placed within four steps, 2026-10-24

**What is still not true first.** The windowed client (`client::Online`,
`client::Flight`) does not stamp its inputs: it is sampled from where each
input began, as before, and a frame of its own stalled between flying and
sending would still inflate its clocks' difference. The Phase 9 item stays
open for it. The reported 5 to 15 steps (five clients and servers on two CPUs,
debug build, 2026-10-21) is not reproduced here; what is reproduced, built,
is the same fault larger. The windowed client also does not fly a gap at the
pace its loop learnt - it shares `sim::Pacing`, so it does, from this change,
without a test of its own.

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

**The selftest hash** does not move: the selftest flies `sim::selftest`, not a
prediction or a pace.

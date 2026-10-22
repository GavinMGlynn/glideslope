### A machine's stall named before the words that came through it are read, 2026-10-23

**What is still not true first.** The windowed client was already right
(it names a stall at the top of its pass, and its keeper thread only polls
the session, never the clock). Whether `CLOCK_THREAD_CPUTIME_ID` under-counts
on CI's Apple-silicon runners (see below) is not measured here; it did not
cause this failure.

**The failure.** `a_client_predicting_against_a_server_behind_real_time_whose_machine_stops_for_seconds_is_back_within_a_metre_sixty_updates_after`
on macOS release, run 39796618052 (PR #242, head 9b536ad0, job 119887797572):
the built stall named, 3,650 ms of the asked 3,500, but **0 words catching up
and 60 after**, 19 compared; the worst outside it 4.161 m, 26.0 s in, 3.1 s
after the stall; p99 3.82 m. Linux names 52 to 53 words catching up.

**Why.** glideslope_cli named a stall at `render`, and a frame is drawn only
once the socket is read dry. A stall that began on a pass straight after a
datagram was read, the client woken after the server had caught up (macOS
woke it 150 ms late, and it stops ~0.1 s after the server), read the whole
burst first. `net::SessionClock` keeps 2 s of words: after 3.5 s it had only
the burst, so its "now" became the burst's newest word, and the stall's
`caught_up_s` with it. The server's catching up that followed was counted as
the 60 words after, the window closed while the clocks' difference was still
low, and the rest was judged with everything else.

Reproduced on Linux release by building exactly that order: the client stopped
on a pass with a word waiting (`!drained`) and for 4,000 ms against the
server's 3,500 - 0 words catching up, the worst outside 5.9 m, p99 5.8 m.
Either alone does not: 4,000 ms on an idle pass gives 121 apart and 0.956 m.

**The fix** (src/frontend/cli/main.cpp, `Predicting::pass`): the stall is
measured from pass to pass at the top of every pass, before anything is read
- thread CPU time, sleeps asked and timed writes, as the frame measure was -
the way the windowed client already does. Frames keep their own longest-gap
figures.

**The test builds the hard order** (tests/cmake/server_paced_prediction.cmake):
`--stopped-once` now stops on the first pass from its time that follows a
datagram read; the client is stopped 4,000 ms to the server's 3,500; and the
test requires the server's catching up counted (more than 0 words) as well as
the bounds it held.

**Verification.** Linux release, three runs: 60 words catching up and 60
after, back within a metre from word 7 or 8; outside them the worst 0.482,
0.969, 0.958 m, steps 2,293/2,294 and 2,291/2,294. **Seen red**, the stall
named at `render` again with the test as committed: "the server's catching
up was not counted as such", the worst 6.27 m (and 5.93 m before that
assertion was added). Reverted.

**macOS, sourced.** Apple's `CLOCK_THREAD_CPUTIME_ID` on M1 has been reported
to under-count thread time about 40-fold (Apple Developer Forums 711929; the
ratio matches the 125/3 Apple-silicon timebase, QA1398 / Go issue 17610). An
under-count would name a busy frame of the client's own as the machine's - one
stall too many - not the miss seen here, where the one stall was named
correctly. macOS timer coalescing lengthens sleeps (RTI community note on
macOS sleep delay); here it woke the client 150 ms late.

**From the review of #243.** The plan's item is cut to a summary; its detail
is above. A stall never named - `--stopped-once` waits for a pass straight
after a datagram read - now fails saying so; seen with the stop put at 1,000 s
on the session's clock (never reached): "the client did not name the stall
built - it stops only on a pass straight after a datagram was read". The
test's comment gives the stop as 12 s on the session's clock (20 s of real
time at 0.6), not "20 s in".

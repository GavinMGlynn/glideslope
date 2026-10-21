### The macOS network checks keep time on a stalling runner, 2026-10-21

**What is still not true first.** A machine that stops the client while it
is working, not asleep, is counted as the client's own and fails the
check: a thread's CPU time cannot tell its own work from a virtual machine
not run while the guest thought it ran. And a blocking call of the
client's own that it does not time - anything but its sleeps, its wait on
the aircraft being built and its files' writes - is counted the machine's,
under the second the machine's share is bounded at: a 300 ms blocking read
of a pipe in its loop passes, said as the machine's 299 ms. Nothing
portable says how long a thread was blocked (macOS's thread_info gives a
state, not a time). A model introduced after the
client is drawing (another player joining with a model nobody flies yet)
is still built in a frame. The AI's controls shown 0.0051 to 0.0061 off
against 0.005 is another failure of the same check, worked as its own item.

**The red tree.** The nightly's macOS network checks failed on 2026-10-14,
-15, -18 and -20, at
`prediction_interpolation_and_the_player_limit_hold_at_200_ms_with_jitter_and_loss`:
"the predicting client went 265 ms without drawing a frame" (and 311, 465,
490, 498 ms).

**The cause: mostly the client's own.** Its tracks (the failed runs'
artifacts) put the gap at the same place in five of the six: some 0.25 s
after its first update, as its prediction began, or as it took its aircraft
back, with the updates that came meanwhile all read at once after it. Timed
here (linux-debug, sanitized), building the aircraft's JSBSim model, which
the prediction did as it began, in a frame, took 56 to 124 ms of the 60 to
143 ms its beginning took; the frames either side were 100 to 220 ms apart.
A macOS runner, about twice as slow, made that 265 to 490 ms. The sixth, 498
ms at the hand-over, has no work of the client's in it: the machine's. The
interpolation check, run here on all six runs' tracks, passes every one
(the worst 0.33 m against 2 m): frames after a gap were drawn right.

The web says the same of the machine: on GitHub's virtualized macOS runners
kevent and timed waits oversleep by 200 ms and more
(moonbitlang/async#502; actions/runner-images#13452, #13882, #11760).

**The fix, in the client** (`src/frontend/cli/main.cpp`, `Predicting`):

- each model is built as it is introduced, not as it is first flown, and
  the first frame waits until the client's own aircraft has been introduced
  - as a client shows nothing of a session while it loads her;
- the aircraft flown before a hand-over is let go, and a take-back flies
  one never flown: as one is taken, the next of that model is built on a
  thread of its own. JSBSim's instances may be built on threads of their
  own once the first is built (JSBSim issue 666; its logger is
  thread_local since PR 1354), and the first here is built at the
  introduction, on the client's own thread. `sim::Aircraft`'s one write to
  JSBSim's process-wide debug level is now made once.

**Reusing the one flown instead was measured, and refused.** An aircraft
begun again - every JSBSim model's initial conditions (ResetToInitialConditions),
every property the wrapper had written put back - and one just built,
set up alike, then flown ten seconds of the same inputs: every number in
their property trees compared, all 16 aircraft, after flying the first
twenty seconds at full flap, gear up, brakes on, an engine stopped, in
gusting warm low-pressure air. Set up, 7 to 42 numbers differed (the
pilot's accelerations, electrical currents, terrain elevation's last bit);
ten seconds on, up to 240 (the a320 1, the j3cub 193, the learjet35a 240, the c172p none):
JSBSim keeps state no initial condition puts back. Not landed.

**Its files' writes timed.** The track and heard files are written through
a file buffer that times where bytes reach the system - an overflow, a long
write, a flush - and counts the wall time less the CPU time as the
client's own wait. **The machine's share is bounded** at a second, twice
the most a macOS runner was seen to hold the client in this check (498 ms):
over it the test fails, as it may be a wait of the client's own it does
not time.

**And the check tells the machine from the client.** Between two frames,
the client's own time is the CPU time its thread used
(`platform::thread_cpu_s`: CLOCK_THREAD_CPUTIME_ID on Linux and macOS,
GetThreadTimes on Windows), the sleeps it asked for, and what it waited on
its own building; the rest is the machine's, not running it. The test
fails on the client's own over 250 ms, as before, and says the machine's.

**Verification.** linux-debug: the longest between frames went from 113 to
220 ms to 21 to 34 ms over the impaired checks' runs, the client's own 19
to 30; building the aircraft as the prediction began went from 56-124 ms
to nothing. The tests that predict, take over, hand over or fly the
server's weather pass. Seen red: a 300 ms busy wait between frames fails
it, "went 303 ms of its own without drawing a frame". Seen to tell the
machine: the client stopped by its PID (SIGSTOP, 0.5 s, SIGCONT) at 8 s
passes, "the machine held the predicting client 505 ms of the 530 between
two frames, not running it", the longest of its own 50 ms, and the frames
after it judged within the interpolation's 2 m. The client's old build in
a frame is not red here - 220 ms, under the 250 - only on a macOS runner.

**The nightly on the branch (964ddf3d), 20 runs of each check: red, so the
item stays open.** Run 39532098811: 39 of 40 passed; the longest between
frames in the failed run's client 54 ms (24 of it the machine's), and the
one failure the AI's controls 0.0051 off against 0.005 - the other failure
of 2026-10-15, a plan tail of its own. Run 39532102294: 38 of 40; once the
controls again (0.0052), and once the predicting client 430 ms between two
frames, 424 of them counted its own, at 18.2 s into the 100 ms check, long
after the switches, among other frames 60 to 210 ms apart: no work of the
client's is known there. Whether that is the machine stalling it while it
ran - which the client counts as its own, since it cannot tell - or its
own (a track file's write, say) is not known; the thread's CPU time across
the gap would say.

**The nightly on the branch, green twice.** With the AI's controls'
bound worked on the stacked branch (`macos-controls-bound`, its own status
file), runs 39542614480 and 39542618733: each 40 of 40 macOS network
checks passed. The 430 ms gap's CPU time was not had again: no run went
over 250 ms. On that branch's PR CI, once, Windows release's paced
prediction failed with the client 3,583 ms between frames, 3,503 of them
the machine's - a runner that did not run it, now said as such.

**From the review, after those runs.** The files' writes timed and the
machine's share bounded (above). Seen red: a 300 ms block inside a track
write fails it, "went 319 ms of its own"; a 1.5 s blocking read in its loop
fails it, "was not run 1488 ms of the 1519 between two frames: more than a
second". A 300 ms one there passes, as above. **ThreadSanitizer**: a
one-off clang -fsanitize=thread build (linux-release, in the scratchpad;
no sanitizer preset has it) of the client, server, relay and check ran the
impaired checks at 100 and 200 ms, the take-over checks at 100 and 200 ms,
and the take-back checks, with halt_on_error: all passed, no report
written. The two nightly runs above were on the stacked branch, not this
one; this one's own were 39 and 38 of 40, so the item is open again until
its final head's nightlies are green, and the two land together.

**This branch's final head, alone** (run 39556439189, 97bd913a): 39 of 40;
the one failure the AI's controls 0.0063 off, the controls item's, with
the longest between frames 39 ms. Its evidence is the stacked branch's.

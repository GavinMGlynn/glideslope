### The macOS network checks keep time on a stalling runner, 2026-10-21

**What is still not true first.** A stall of the machine while the client
is working, not sleeping, is still counted as the client's own and fails
the check: the client cannot tell its own time from time the machine took
away while it ran. A model introduced after the client is drawing (another
player joining with a model nobody flies yet) is still built in a frame.
The other macOS nightly failure seen on 2026-10-15 - the AI's controls shown
0.0061 off at one frame, against the 0.005 bound - is not this, and is not
worked here.

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
- the aircraft no longer predicted (handed to the AI, or left for one taken
  over) is kept and begun again at the next take-back - its loading, its
  terrain and the server's motion put on it as on one just built - not
  built again in a frame.

**And the check tells the machine from the client.** The client counts the
time each millisecond's sleep comes back past the millisecond, and says,
of the longest between two frames, how much was that; and the longest of
its own - between two frames less what was slept past what was asked. The
test fails on its own over 250 ms, as before, and says the machine's.
Nothing the client does is taken off: what it asked to sleep is its own,
and the asked and the counted are one constant.

**Verification.** linux-debug: the longest between frames went from 113 to
220 ms to 20 to 34 ms over five runs of each impaired check, the client's
own 19 to 30 ms of it; building the aircraft as the prediction began went
from 56-124 ms to nothing. The 129 tests that predict, take over, hand over
or fly the server's weather pass. Seen red: a 300 ms busy wait in one frame
fails it, "went 318 ms of its own without drawing a frame". Seen to tell
the machine: a 400 ms sleep put after the millisecond's, as a runner's
oversleep is, passes, saying "the machine held the predicting client 401
ms of the 415 between two frames", and the interpolation check judged the
frames after it within its 2 m. The client's old build in a frame is not
red here - 220 ms on this machine, under the 250 - only on a macOS runner.
The nightly, run on the branch: (see below). PR CI: (see below).

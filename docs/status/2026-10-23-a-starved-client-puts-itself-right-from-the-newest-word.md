### A starved predicting client puts itself right from the newest word, 2026-10-23

**What is still not true first.** The window client already put itself right
once a frame, from the newest word (`Online::hear`, `Flight::hear_clock`), but
it does not hold the words before the newest against where it had flown: its
prediction error is measured on fewer words than arrive. Named as an open
Phase 9 item; not done here (it needs the Windows build, and this item touched
only `glideslope_cli` and `sim::Prediction`).

**The failure.** On #232's branch (stovl-join-seeded, Ubuntu debug CI), the
STOVL-at-once test's client had one frame of 450.9 s, 434.4 s of it its own
CPU; 250 updates compared, 453 s in; corrections to 42 km. Reproduced locally
by another agent: debug build, one CPU (`taskset`), two busy loops - a frame
of 201.6 s, corrections of 1,832 m, 108 updates compared in 204 s.

**The mechanism, measured.** `glideslope_cli` now says, per time its socket
is read dry, the most words read, put right from, and steps flown again
(`reconciled:` in its report). With every step it flies made 2 ms slower
(`--slow-steps 2000`, its own CPU, busy) and every word putting it right, as
before: one pass that never ended - the socket never read dry again after
joining - with 123 words read and put right from in it, 49,506 steps flown
again (about 400 a word, `sim::most_unacknowledged` being 480), 93 corrections
snapped, the worst 36.8 m, and 0 updates compared in 123 s. At 1,500 us: 141
words in one pass, 51,025 steps, 130 m. At 1,000 us and below it keeps up.
Each word replays every step held; a client that cannot fly them as fast as
the words come reads each later, holds more steps, and never empties its
socket - the spiral inferred, now counted.

**The fix.** In `glideslope_cli` (`Predicting::heard`, `own_word`,
`put_right`), a word on its own aircraft is kept until the socket is read dry;
one read before a newer is superseded: it tells the clocks' difference
(`sim::Prediction::hear_clock`, which now returns the step it places the word
at) and is held against where this client had flown, exactly as before, but is
not replayed from. Only the newest read puts the prediction right. So a pass's
replay is one, bounded by `most_unacknowledged`, however many words came.
This is the standard reconciliation: the client is put back to the newest
authoritative state and re-applies the inputs the server has not yet seen
(Gambetta, *Client-Side Prediction and Server Reconciliation*,
gabrielgambetta.com; Valve, *Source Multiplayer Networking* - the client snaps
to the snapshot and re-runs the user commands after it; the Overwatch GDC 2017
talk, Ford - roll back to the server's movement state and replay buffered
input). Gaffer On Games' *State Synchronization* uses the newest state and a
jitter buffer for ordering. Older states only add replay that the newest
supersedes.

**The pace is still steered by every word** (`sim::Pacing::follow`), superseded
or not: its loop integrates word by word. Steered only from the newest, the
stalled paced test (`..._whose_machine_stops_for_seconds_...`, Linux debug)
came back within a metre from the 22nd to 27th word after the stall, the worst
across it 10.8 m and p95 0.995 m, against the 4th and 13th word and 6.1 m
before this change; steered by every word, the 8th, 6.9 to 7.3 m, p95 0.40 to
0.47 m.

**The statistics stay honest.** Every word is still compared (the error, the
percentiles, the stalls' words); only corrections are counted for the newest
alone, since only it moves the aircraft. The report says how many words were
superseded.

**Verification.**
`a_predicting_client_too_slow_to_replay_every_word_puts_itself_right_from_the_newest_and_keeps_up`
(tests/cmake/starved_prediction.cmake): the client at `--slow-steps 2000`
through a 100 ms relay must compare 400 updates within 120 s, put itself right
from at most one word between two times its socket ran dry, read at most 25
words between them, and hold p95 under 1 m, the worst under 3 m, corrections
under 20 m and none snapped. Locally (debug): 400 compared in 19 s, at most 3
words read and 1 put right from, 28 steps replayed at most, 194 words
superseded, the worst error 0.857 m, p95 5 mm, the worst correction 0.425 m.
Seen red with each word putting it right again (the defect put back): 0
compared in 123 s, 123 words in a pass, 93 snapped; the fix restored, green.

**The selftest hash** does not move: the selftest flies `sim::selftest`, not
the prediction, and nothing in the physics changed (831a4e7954233b0a on this
build).

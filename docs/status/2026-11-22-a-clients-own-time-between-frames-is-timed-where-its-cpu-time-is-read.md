# A client's own time between frames is timed where its CPU time is read, 2026-11-22

A fix to the measure the impaired network checks keep time by, and a word
more from it. No plan item is ticked.

## What is still not known first

What made the nightly's 401 ms is not known. The fix below removes one way
the client misplaced its own time, and the client now says what the
longest of its own was made of and when; the next one will say which part
it was. A virtual machine that stops the client while it works is still
counted as the client's own, as on 2026-10-21: nothing portable tells the two
apart. So a repeat that the breakdown puts in CPU time still would not
place it: telling a pause of the virtual machine from the client's own
work needs the runner's steal time or its system logs for that moment.
The plan keeps this open as a Phase 12a item until it is placed.

## What failed

Nightly 44954156532 (2026-11-21), macOS debug, runner 4, the third of the
five runs of
`prediction_interpolation_and_the_player_limit_hold_at_100_ms_with_jitter_and_loss`:
"the predicting client went 401 ms of its own without drawing a frame", with
0 ms of it the machine's. The other four runs passed. No other nightly
since 2026-11-07 failed this way; the failed jobs of eleven were searched.

## The deliberate long frame was not it

The lead was the test's own long frame: the client draws nothing for 0.4 s
three frames after each switch. That frame is not what the check measures.
`render` reads its CPU time and resets its count on every call, every
sixtieth of a second, and the long frame only skips the drawing of the
client's own aircraft further on (`own_frame`), so the long frame is many
short ones to the check. In a passing run here the longest was 44 ms.

The failed run's track (artifact `network-tracks-macos-4`, run-3) shows a
real gap instead. The client drew at 16.1395 s on its clock and read the word
that came then. It did not read the socket again or draw anything until 16.528 s, when it read four words at once. They had queued while it was not
looping. Its next frame was at 16.5409 s, 401 ms on. That gap fell between
the second and third frames after the prediction began again, so the long
frame's 0.4 s had already passed by itself and the display measured the
same 401 ms. The number agrees with 0.4 by chance. The relay's 200 ms
gap was in the same span. Nothing in the track says what the client was
doing.

## What was found wrong, and fixed

The frame's wall time was taken at the top of the pass that drew it
(`up_s`), and its CPU time inside `render`, after the pass had put words
right and flown its steps. So whatever a pass did before it drew was counted
as CPU time in this frame and as wall time in the next. A 300 ms busy wait
there was said to be "300 ms of it the machine's", and the client's own
longest was 50 ms. The test passed a client that had stalled itself. Now
`render` reads the clock where it reads the CPU time (`since_began`), so
both cover the same span.

And the client says what its longest was made of, and when it ended:
"the longest of this client's own N ms, ending at T s: C ms of its thread's
CPU time, A ms asleep as it asked and W ms waiting on its files' writes".
The test reads the line and repeats it in the failure.

## Verification

linux-debug, in this worktree:

- Both impaired checks (100 ms and 200 ms) pass. The longest of the
  client's own was 36 and 41 ms, made of 37 and 28 ms of CPU time, 3 and 13
  ms asleep, and 0 ms of writes.
- **Seen red.** A 300 ms busy wait at the start of `render`, once after 10
  s, was added. Before the fix the 100 ms check passed with it: "the longest
  between frames 301 ms, 300 ms of it the machine's ... the longest of this
  client's own 50 ms". After the fix it fails: "went 315 ms of its own
  without drawing a frame (ending at 10.309 s: 300 ms of its thread's CPU
  time, 15 ms asleep ...)". The busy wait was then taken out.
- Loading two of the cores with busy loops did not reproduce a gap over
  250 ms. The longest was 136 ms, 99 of them the machine's.

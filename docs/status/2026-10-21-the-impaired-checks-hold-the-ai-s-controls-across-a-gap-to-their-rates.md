### The impaired checks hold the AI's controls drawn across a gap to their rates, 2026-10-21

**What is still not true first.** The flaps and speedbrake have no rate to
derive a bound from - the AI moves them as levers, in steps - so across a
gap they are held to the truth only where the truth held them still, and
otherwise to the line between the updates the client had; no run so far
has had one move across a gap (counted: 0 in every run read).

**The failure.** On CI's macOS nightly (2026-10-15, and twice on
2026-10-21 on #229's branch) `prediction_interpolation_and_the_player_limit_hold_at_*`
failed with the AI's controls drawn 0.0061, 0.0051 and 0.0052 off the
truth, against 0.005: half a hundredth, what the HUD shows them to
(7aa9ea02, 2026-09-25).

**The cause: no client could know.** Each of the three frames was drawn
across 0.29 to 0.33 s with no update - the relay's fifth of a second every
four (`--gap 200 --every 4`) and the losses about it - and drawn, as it
should be, on the straight line between the two updates the client had.
The worst (2026-10-21, runner 4): the throttle rose from 0.9939 to its
stop at 1.0 inside the gap; the line from the update before to the one
after drew 0.9949 where it was 1.0. Every frame drawn between two updates
in a row was within 0.0028 in all 20 runs' tracks read.

**The bound across a gap, derived.** A control whose rate is at most R
misses the chord between two of its values S apart by at most R S / 2
(its slope less the chord's slope m lies in [-R - m, R - m], so the most
it can get from the chord is (R - m)(R + m) S / 2R; see e.g. Atkinson,
*Elementary Numerical Analysis*, 4.2, for the smooth case). The rates are
the autopilot's own (sim/autopilot.cpp): no loop moves a stick or the
pedals faster than a hand, full travel a second; the throttle a quarter
of that. So `tests/tools/interpolation_check.cpp` now holds a frame drawn
across more than two updates' worth (0.1 s, its own figure for the truth)
to the line between the two updates the drawing client had, to 0.005, and
to the truth by 0.005 + R S / 2; frames between updates in a row stay at
0.005 to the truth. It says how many frames were drawn across a gap, and
`tests/cmake/server_impaired.cmake` fails if none were - the gaps are made
every four seconds, so every run has them (16 to 42 in the runs read).

**Verification.** Run on all twenty downloaded macOS tracks (the four
failed nightlies and the branch's two): all pass, the three failures'
frames among them, held to their rates; between updates in a row the
worst is 0.0028. linux-debug: both impaired checks pass, 34 and 38 frames
across gaps. Seen red: the client's controls drawn half again too far
along between its updates fails it, frames out both across gaps and
between updates in a row. The nightly on the branch, twice (runs
39542614480 and 39542618733): 40 of 40 macOS network checks each - 40
runs of each check, not the 100 the plan asked for; a run of 100 of each
on the final head is what ticks it.

**A hundred runs of each check** (run 39556446562, 74fbc8cd, with the
stall item under it): 199 of 200 passed. Every frame of the AI's controls
held - across gaps to their rates - and the longest between frames 46 ms.
The one failure is neither item's rule: the 200 ms check's prediction
error, 10.931 m against its 10 m bound, 4.1 s in, just after the join (95th
percentile 8.3 m; earlier runs' worst seen 5.1 m). Not explained. One
suspect, unproven: the next aircraft is now built on a thread of its own
from the moment the join takes the first, on a three-core runner also
running the server, the relay and another client. The item stays open.

**A hundred runs of each check on the final head** (run 39693946486): 200
of 200. The earlier run's one failure was the stall item's - the client's
build at the introduction holding its clock - diagnosed and fixed there.
The item is done.

### The STOVL at-once tests' predicted pitch: measured, the cause not yet found, 2026-10-25

**What is still not true first.** Nothing is fixed. The three at-once tests
(`server_stovl_at_once.cmake`: the first input, the slow preparation, the
first asking ignored) still fail at random on CI's Windows on the pitch
bound - 0.580 degrees on windows-clang (run 40085938621), 0.960 on
windows-release (run 40099786611). This round measured where the error
comes from and ruled one cause out; the Phase 9 item stays open.

**Measured** (linux-release, `scratchpad` runs of the test's own pipeline
with `--track`; busy loops pinned with the test to four CPUs):

| load | runs | pitch error, degrees | steps placed off |
| --- | --- | --- | --- |
| none | 8 | 0.202 to 0.204 | 0 to 1 |
| 4 loops on 4 CPUs | 6 | 0.202 to 0.204 | 0 to 1 |
| 6 loops on 4 CPUs | 6 | 0.200 to 0.204 | 1 |

Every variant (plain, `SLOW_MS`, `SPEND`) is among them. Load does not
move it here, and it is not the 0.004 degrees the 2026-10-22 status file
measured: something since has added a steady 0.2.

- **It is there from the first word compared to the last**, two values
  most of the time (0.154 and 0.203 in one run), while the position is
  2.2 mm off throughout - her attitude, not her place.
- **It grows with the steps flown again**: 0.111 degrees at 22 steps, 0.120
  at 23, 0.133 at 24 and 25 (one run, 3,000 words) - about 0.0055 degree a
  step, 0.67 degree a second. Flown again from the server's word, the
  prediction pitches away from the server at a steady rate, so a persistent
  difference of what the replay flies, not where the word is placed.
- **Why Windows varies**: CI's failing run flew up to 71 steps again
  between two words, three times what this machine does; at that rate 71
  steps is 0.39 degree, and longer replays after a stall more. So the spread
  is the replay's length times a rate that should be nought.

**Ruled out: the word heard before her first input reached the server.**
Such a word is placed at the first input's first step, though it was taken
1 to 6 steps before the server applied it (`word` lines in the track), so
her law in the prediction is made from her as she was those steps earlier.
Placing it properly once the clocks' difference was known (flying the gap
on the levers held) changed nothing measured - 0.202 and 0.204 - and was
not kept.

**What changed**: nothing in the code. The figures above came from scratch
instrumentation of glideslope_cli's `--track` (each comparison's pitch error
and server step, and a `word` line for each word heard), which was not kept:
rebased onto main's every-word comparison it no longer applied, and the
cause was found without it (2026-10-28).

**Next**: find which state of her law or flight-control system differs
between the server and a replay - the autopilot's integrals other than the
pitch integral, and her held trim, since #248 - by printing both sides' at
the same step, then the bug, then the test seen red on #232's (the
prediction's pitch integral put a degree off: 1.730 degrees then).

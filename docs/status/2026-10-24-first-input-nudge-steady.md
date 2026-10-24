### The first-input test's nudge is held to the steps the server flew it, 2026-10-24

**What is still not true first.** The nudge is not the same length on every
machine and cannot be made so by the client: the server flies an input
from the step due when it arrives (net::HeldInputs), so the step its
centred input lands on moves with how late that one datagram is. Exact
would need inputs that name their step, as a Source engine user command
carries its own duration - a protocol change. The project owner chose
instead (2026-10-24) to hold her attitude to the steps the server flew
it. Under 20 busy loops and more on this machine (itself loaded by other
work then, load average 50) most nudged F-35B runs failed the test's
**prediction** bounds - median and 95th percentile - which the plan's
open "On a busy machine the server's word is placed a step off" covers;
no run failed the attitude bound below.

**Why it varied.** `a_players_f35b_..._predict_alike` (#233) banked the
nudged F-35B 5.15 and 5.27 degrees on CI's Windows debug (runs 39952958721,
39953571577), 2.58 here idle. Her bank is the nudge's length on the server,
about 0.041 degree a step: her flight controls hold the attitude a stick
movement leaves her. The client now says how many steps the server flew the
nudge (from each word's input and how far into it the server was), and:

| nudge | load (busy loops, 20 CPUs) | server's steps | bank, degrees |
| --- | --- | --- | --- |
| before: her first 15 inputs | none (2) | 61, 62 | 2.58, 2.62 |
| before | 20 (5) | 56 to 65 | 2.54 to 2.66 |
| before | 30 (5) | 59, 66, 110, 114, 116 | 2.49 to 4.79 |
| before | 40 (6) | 69 to 200 | 3.14 to 8.15 |
| after: 60 steps once settled | none (3) | 61, 62 | 2.58 to 2.62 |
| after | 20 (5) | 61 to 65 | 2.57 to 2.74 |
| after | 30 (5) | 58 to 64, 99 | 2.45 to 4.11 |
| after | 35 (6) | 57 to 64, 98 | 2.45 to 4.07 |
| after | 40 (6) | 61 to 68, 90, 128 | 2.57 to 5.14 |

Before, the nudge began with her first input, as she joined. The client
sent its fifteen inputs over 0.6 s of a loaded machine, but the server - busy
joining her, owing steps - stamped the first early (at most 30 steps after
the step it had flown) and the sixteenth after it had caught up, so it flew
the nudge 110 to 200 steps. The 40-loop runs that failed past the bank also
failed the prediction's own bounds (words 57 steps off), which the status
file of #233 already says 40 loops does.

**What changed.**

- `glideslope_cli connect --nudge` begins the nudge once its prediction's
  clocks' difference is settled (25 words), and holds it for 60 of its own
  steps, not 15 inputs; her first input is still the levers she was held
  on, so the test still tests a first input that keeps her trim.
- It says `nudged: inputs A to B, N steps here; ...`, and the test fails a
  nudged run that never nudged.
- `glideslope_server --test-say-controls` says each step a player's
  controls change at; the test counts the nudge's steps from it.

**The bound, per step flown** (the project owner, 2026-10-24; refitted
after review). The steps are **the server's own count**: with
`--test-say-controls` it says each step a player's controls change at, and
the nudge is the steps between its two changes (untouched, there must be
none; nudged, exactly two). Counted by the client from the first inputs
its words named, it was up to a few steps short.

Fitted on one set of runs, checked on another, both the nudged F-35B and
C172P through the test's own pipeline (linux-debug, the two sets run side
by side, so each loaded the other too):

- **Set A, fitted**: 9 runs, idle, 15 and 30 busy loops; F-35B 52 to 92
  steps, C172P 59 to 79.
- **Set B, checked**: 12 runs, idle, 10, 20 and 30 busy loops; F-35B 52
  to 96, C172P 54 to 80.

| | line fitted on A, hundredths of a degree | spread about it on A (sd, worst) | on B (least, most) | the bound |
| --- | --- | --- | --- | --- |
| F-35B bank | 9.27 + 4.061 a step | 0.44, 0.83 | -0.92, +0.80 | 10 + 4.1 a step + 25 |
| F-35B pitch range | -12.07 + 1.035 a step | 0.72, 1.24 | -2.16, +0.82 | -12 + 1.1 a step + 25 |
| C172P bank | 13.72 + 1.103 a step | 0.16, 0.21 | -0.27, +0.70 | 14 + 1.2 a step + 25 |
| C172P pitch range | 18.66 + 1.118 a step | 0.27, 0.60 | -1.06, +0.82 | 19 + 1.2 a step + 25 |

The rate is rounded up to a thousandth of a degree a step and where it
begins up to a hundredth. **The margin is a quarter of a degree**: the
spread measured is at most 0.022 degree from the line, as is 3 sd,
so it is more than ten times that. The rest is kept for what one machine
cannot measure - another platform's floating point (CLAUDE.md's
cross-platform tolerances) and the line taken past the 96 steps measured
to the 120 allowed; with the server's own count there is no count error
to add. Every run of both sets is within it by at least 0.28 degree. A
nudge flown more than 120 steps fails, named.

The figures before the refit, counted by the client, for the record (a
run stops at its first failure, so a loaded run that failed the F-35B's
prediction did not fly the C172P):

| load | F-35B steps, bank / most, pitch range / most | C172P | what failed |
| --- | --- | --- | --- |
| none | 60 to 63: 2.53 to 2.66 / 2.83 to 2.96, 0.52 to 0.55 / 0.85 to 0.88 | 62 to 67: 0.82 to 0.85 / 1.12 to 1.19 | nothing |
| 20 loops | 60, 100: 2.53, 4.15 / 2.83, 4.55; 128 | - | prediction twice; once the nudge flown 128 steps, named |
| 30 loops | 44 to 104: 1.88 to 4.31 / 2.15 to 4.73 | 62: 0.82 / 1.12 | prediction twice |
| 40 loops | 80 to 88: 3.34 to 3.67 / 3.69 to 4.04 | - | prediction three times |

**Verification.**

- The test passes here idle (linux-debug): F-35B nudged 61 steps on the
  server, bank 2.58, height 1,524.0 to 1,534.3 m; C172P 62 steps, bank
  0.82; the untouched cases as before.
- Seen red never nudged (the nudge's start made never to come): "the
  f35b, nudged, was never nudged" - failed; reverted.
- Seen red on a nudge flown 128 steps, by load (above).
- After the refit: all four cases pass idle (F-35B nudged 61 server
  steps, bank 2.58 of at most 2.86, pitch range 0.53 of 0.81; C172P 62,
  0.82 of 1.14, 0.88 of 1.19). Never nudged, red again; the trims held as
  the stick, red again: bank 35.89 against at most 1.11.
- Seen red on the bug it pins: the aileron and rudder held as the stick and
  pedals rather than as trims (sim::held_until_moved), the C172P nudged
  then centred banked 35.88 degrees and fell to 1,441.2 m - failed; and again
  with the bound per step, 35.85 degrees against at most 1.09; reverted.
- The loaded figures above, `scratchpad` runs of the nudged F-35B alone
  through the test's own pipeline.

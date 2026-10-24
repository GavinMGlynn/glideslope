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
- It says `nudged: inputs A to B, N steps here; the server flew input X
  from its step S and input Y from T, M steps`, and the test prints it and
  fails a nudged run that never nudged.

**The bound, per step flown** (the project owner, 2026-10-24). Nudged, her
bank and her pitch's range are each held to the most it moved per step the
server flew the nudge, times those steps, plus 0.25 degree; untouched, 5
degrees as before. A nudge flown more than 120 steps - twice its 60 - fails,
named. The steps are the client's figure: the first nudged and first
centred input heard, each from the server step it began at, so a few steps
short when the first heard is not the first sent. From every run above and
the ones below (linux-debug, 44 to 132 steps):

| | per step, measured | held to (thousandths a step) | at 61 steps | at 120 |
| --- | --- | --- | --- | --- |
| F-35B bank | 0.0411 to 0.0430 | 43 | 2.88 | 5.41 |
| F-35B pitch range | 0.0083 to 0.0095 | 10 | 0.86 | 1.45 |
| C172P bank | 0.0121 to 0.0134 | 14 | 1.11 | 1.93 |
| C172P pitch range | 0.0126 to 0.0145 | 15 | 1.17 | 2.05 |

The C172P, nudged, under load: 58 to 68 steps, bank 0.78 to 0.89, pitch
range 0.84 to 0.95; 112 steps, bank 1.35 and pitch range 1.41.

**Re-run with the bound** (nudged, both aircraft, three runs a load; a run
stops at its first failure, so a loaded run that failed the F-35B's
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
- Seen red on the bug it pins: the aileron and rudder held as the stick and
  pedals rather than as trims (sim::held_until_moved), the C172P nudged
  then centred banked 35.88 degrees and fell to 1,441.2 m - failed; and again
  with the bound per step, 35.85 degrees against at most 1.09; reverted.
- The loaded figures above, `scratchpad` runs of the nudged F-35B alone
  through the test's own pipeline.

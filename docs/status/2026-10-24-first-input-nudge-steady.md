### The first-input test's nudge is the same nudge on every machine (part), 2026-10-24

**What is still not true first.** The nudge is not yet the same on every
machine: the server flies an input from the step due when it arrives
(net::HeldInputs), so the step its centred input lands on still moves with
how late that one datagram is. Under 30 to 35 busy loops on 20 CPUs a
60-step nudge was flown 58 to 99 steps, and the F-35B banked 2.45 to 4.11
degrees against the 5-degree bound. Making it exact needs the server to fly
an input at a step the client names, as a Source engine user command
carries its own duration - a protocol change, not this item's.

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

**Verification.**

- The test passes here idle (linux-debug): F-35B nudged 61 steps on the
  server, bank 2.58, height 1,524.0 to 1,534.3 m; C172P 62 steps, bank
  0.82; the untouched cases as before.
- Seen red on the bug it pins: the aileron and rudder held as the stick and
  pedals rather than as trims (sim::held_until_moved), the C172P nudged
  then centred banked 35.88 degrees and fell to 1,441.2 m - failed; reverted.
- The loaded figures above, `scratchpad` runs of the nudged F-35B alone
  through the test's own pipeline.

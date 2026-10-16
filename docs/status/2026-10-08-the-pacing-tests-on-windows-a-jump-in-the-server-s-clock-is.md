### The pacing tests on Windows: a jump in the server's clock is not chased, and the bounds say what the clocks allow, 2026-10-08 — tails stay open for their month of CI

**What is still missing first**: the engine-stop and paced-prediction tests'
month of CI runs without a failure; and the window client's 6 to 9 m, as
before.

**What failed** (PR runs after #121, Windows debug and clang-cl):
- The engine-stop test at normal speed: "the median 1404 mm, more than the
  59 mm before", the client saying `paced: flown at 1.247 ... the clocks'
  difference 36 steps from the one held then`. A starved server had caught
  up at once - four steps a look - and the clocks' difference jumped 36
  steps. Held to the difference from before, the client flew at 1.25 to take
  the jump back, its inputs flown for fewer steps than the server's, off by
  1.4 m all the while. **Fixed in `sim::Pacing`**: between two words the
  difference may move only as fast as two paces can part (`most_parting`,
  half of real time, plus a step); past that, what it is held to moves by the
  excess. **Seen to fail first**: the unit test's new jumps of 36, -36 and
  120 steps against a server at real time - "the client's pace went 0.307
  from the server's" - pass now, the pace within 0.05 of the server's
  throughout.
- That test also failed on Windows on noise alone: run five times on the
  Windows machine here (the jump fixed) its medians were 50 to 63 mm before
  and after, a millimetre either way, and failed twice on "no more than
  before". CI's earlier failure was 47 against 43 mm. The bound is now 2 cm
  over the median before. **Seen to fail** with the engine run on (the
  prediction told nothing of the stop): 214 mm after against 52 before.
- The paced test: "the worst error 1112 mm / 1092 mm, the median 5 / 4 mm".
  Reproduced here on Windows once in five, worst 1.098 m. `glideslope_cli`
  now says its 99th percentile: 0.545 to 0.553 m on Windows in all five, and
  0.28 to 0.45 m on Linux under load - a step's travel is 0.42 m, and the
  clocks' difference is known to a step. Windows' sleeps are 15.6 ms, not
  the millisecond the server, relay and client ask for, which puts up to two
  steps of jitter into a relay told to add none, and one update in 600 off
  by 2.6 steps. **The metre now bounds the 99th percentile**, and the worst
  is bounded at 2 m (under five steps); the median's 5 cm is unchanged.
  **Seen to fail** unpaced: 99th percentile 7.849 m.

**From the review of #125.** The unit test also slows the server all at
once, from real time to 0.3 and to 0.6 of it, 20 s in: followed - the pace
within 0.01 of the server's, the difference within two steps of the one
held - 17.1 and 10.9 s after, never more than 66 and 37 steps off (bounds 20
s and 80 steps). **Seen to fail** with `most_parting` 0.05, nearly all of
the slowing forgiven as a jump: followed only 33.9 s after. Why a half is in
`sim/pacing.hpp`. The jumps' cases also hold the difference within two steps
of the one held at the end and over the last ten seconds.

Not done: Windows' timer resolution (`timeBeginPeriod`) for the server, the
relay or the client - it would be a change to what the programs do, not to
the tests, and the jitter it makes is the kind a real network has anyway.

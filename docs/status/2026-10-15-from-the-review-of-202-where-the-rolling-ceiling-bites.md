### From the review of #202: where the rolling ceiling bites, counted; walking pace at 4.5 kt, found, 2026-10-15 — item still open

**What is still not true first.**
- **The rolling ceiling (0.3) cannot bite on three landplanes**, and is
  named there: her brakes stop her from 3 kt before her autobrake reaches
  it while she rolls. The J-3 Cub reaches 0.29 at most (0.21 in 15 kt
  across), the Mosquito 0.16 to 0.17, and the C172P 0.296 on a dry runway
  only. It is reached on ten of thirteen dry and eleven wet and across, in
  all three builds.
- **The Mosquito's heading check covers under half a second of rolling**
  at 3 kt (0.43 to 0.46 s at 2-5 kt), then her stop. **And held at 4.5 kt
  she still swings**: rolled up to and held at 4.5 kt at taxi power, a
  probe on linux-debug (not committed), she turned 26.9 degrees dry and
  109.1 in 15 kt across, and the Cub in 15 kt across struck at 15.2
  degrees of pitch and 21.1 of roll, 5.0 s after the hand-over. So the
  entry below's "the Mosquito's swing was the old hand's too" holds at 3
  kt only; both findings are added to their open items (the Cub's
  crosswind pitch term, Phase 9; the Mosquito's swing, Phase 10b). **The
  walking-pace item is open again**: its verification is every walking
  pace, 2-5 kt, and its tests fly only 3 kt, where none strikes.
- **The Cub's old name, reconciled**: the crosswind test named her for
  going over "while the test's throttle rolls her up to walking pace,
  before the rollout has her". The
  trace here puts her strike 4.9 s after the hand-over, under the old
  full-power hand, not in the roll-up, which taxi power did not change and
  in which she does not strike.

**The change.** `walking_pace_then_stop` takes the landplanes whose brakes
stop her before the rolling ceiling, with the reason, and records the most
her autobrake reached while she rolled at 2-5 kt. Each landplane not named
must reach 0.3 there, or it is wrong; a named one that reaches it is
counted and said. Asserted: reached plus named-and-not-reached is every
one flown, and reached is at least flown less the named. The `named`
scaffolding for the rest of the judgement is gone: every other check is on
all thirteen. Comments: the hand *tries to* hold her at 3 kt; the C172P's
wet walking time is 0.42 s, measured; the 2026-10-15 seen-red names its
bug. Two tails: the C172P rearing at full power on held brakes (Phase 9),
and the slow go-around test, 26.9 to 778.7 s on CI against 900 (Phase 14b).

**Verification.** The three walking-pace tests and the gone-over test
pass in linux-debug, linux-release and `-march=x86-64-v2`, the same ones
named and not reaching in each: the Cub and the Mosquito in all three
tests, the C172P on a dry runway. **Seen
red**, targeted: with the rolling ceiling made the standing one
(`walking_pace_ceiling` returning 0.45 above 2 kt) the counted ones, and
only they, failed - ten dry, eleven wet, eleven across, wound to 0.31 to
0.45 rolling; the named ones were not. Reverted, green.

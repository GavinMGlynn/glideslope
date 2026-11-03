### The flying boat in gusts, measured on forty-eight seeds: her eight-seed pass on Linux is a sample, and the platforms' failures are its scatter, 2026-11-03 - item still open

**What is still missing, first**: the item. Nothing is fixed here. The S.23
is still named on Windows's seed 5 and macOS release's seed 3, and this file
records why a fix to those two seeds alone would not be one.

**An x86-64-v2 build does not reproduce them.** Built with
`-march=x86-64-v2` (no fused multiply-add in the binary: `objdump` finds no
`vfmadd`), her eight gust seeds give exactly the linux-release figures of
2026-11-01, to the hundredth of a foot (seed 5 rose 0.18 ft, seed 3 0.31).
So her Windows and macOS figures differ from Linux's by something other than
FMA contraction - their maths libraries, most likely - and Linux cannot show
them.

**Forty-eight seeds on Linux** (x86-64-v2, the gust tests' approach and
judge, seeds 1 to 48): she passes the landing limits on 39. On the others:

| seed | 11 | 13 | 22 | 25 | 30 | 32 | 40 | 45 |
|---|---|---|---|---|---|---|---|---|
| fault | 347 ft/min | rose 3.32 ft | rose 0.80 | rose 6.36 | went round, a balloon | rose 2.95, touched at 3.22 deg | 345 ft/min | rose 0.91 |

So the eight committed seeds pass by luck of the draw, and a platform's
floating point, which gives the same seed other gusts at the flare, draws
again from the same spread: Windows's 0.87 ft and macOS's 0.93 are this
spread's, not a platform's own fault.

**Why she rises**: she touches where her wing still carries her. Held off
with her nose capped at 5 degrees, she is let down at 64 to 92 kt, about 1.0 to 1.4
times her measured 66 kt stall, where at 4 to 5 degrees of incidence her
lift is her weight. The water's first touch reverses her sink (seed 13: 100
ft/min down to 170 up within a tenth of a second, her nose pitched up 3
degrees a second by it), and with her lift equal to her weight nothing
brings her back. On seed 25 a down-gust put her on before she reached the
hold at all, at 92.6 kt and 1.6 degrees, and she flew off 6.4 ft.

**What was tried, and did not cure it** (each over all 48 seeds; none
committed):
- The trim the flare learnt, zeroed at the touch as a jet's is: it was 0.01
  to 0.06 at the touch, not the cause.
- Her step attitude on the water 3.25 or 3.0 degrees, at one, two and four
  times the on-the-step gain: four times the gain at 3.0 degrees halves the
  rises (seed 13 1.53 ft, seed 32 0.82) but leaves seed 25 at 9.6 ft; at
  3.25 or 3.0 seed 25 rises 10 ft, its nose raised from its 1.6-degree touch.
- Her nose cap in the hold-off 5.5 or 6.0 degrees, with the firm gain: she
  touches slower but a lull drops her on at 382 to 401 ft/min (seed 3), and
  the rises remain (seeds 13, 25, 32 at 1.3 to 3.6 ft).

**What a fix needs**: her touch brought below the speed her wing carries her
at, without the hold-off's drop, and a touch before the hold handled - a
change to how she is held off, not a gain. No source for the S.23's hull
lines (its deadrise, for Savitsky's correction) or for the seaplane
handbook's skip recovery text was reached in this session (the FAA's
FAA-H-8083-23 PDF returned 403).

**Verification**: none needed of the tree - no code changed. The figures
above are from `glideslope_tests
in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1` with a
scratch seed and aeroplane filter, reverted.

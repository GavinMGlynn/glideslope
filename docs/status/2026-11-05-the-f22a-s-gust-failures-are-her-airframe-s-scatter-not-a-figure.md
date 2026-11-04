### The F-22A's gust failures measured against every knob of the gust flare: none moves her past noise, and nothing is changed - 2026-11-05 - item still open

**What is still missing, first.** Nothing here makes her land more often.
She still fails twelve of forty gust seeds on Linux (named in
`named_on_a_seed`, tests/unit/test_lesson.cpp), fourteen on Windows and
twelve on macOS, as
`2026-11-05-the-f22a-lands-in-gusts-on-twenty-eight-seeds-of-forty.md`
names them. The gust item and "The F-22A flies her stall-figure tests" stay
open. No code, figure or name is changed: this file records what was
measured, so that it is not measured again.

**What was measured** (linux-release, scratch builds not committed: the
gust test flown for the F-22A alone, and with a seed offset so that seeds
41 to 120 serve as a check on what seeds 1 to 40 suggest; each sweep of
forty takes ten seconds):

- **Her scatter is twice the F-15C's.** Over the forty seeds, her touch
  sink averages 230 ft/min with a standard deviation of 97, her most climb
  in the flare -29 with 90, her touch 553 m along with 147. The F-15C's,
  flown by the same lander: 203 and 57, -66 and 53, 617 and 91. On seeds
  41 to 120 she passes 56 of 80 (70%), the F-15C 74 of 80. Her failures
  are the tails of that scatter: a lull or down-gust in the last ten feet
  (hard), a gust's lift with her nose up (balloon), a flare that levels
  her at 15 ft at 160 kt (float).
- **She is flown at 7 degrees of incidence, not near her limit.** Traced
  down final: 156 kt (1.3 times her 114.1 KCAS stall, and half the 15 kt
  gust factor), 6 to 8 degrees of alpha, her nose 4 to 5 degrees up; her
  flare is not flown on her power (`power_flare_` is false on every seed).
  The F-15C is flown at 10 to 11 degrees.
- **Her throttle as the flare begins predicts the outcome**: from 0.25 to
  0.32, she touched at 258 to 462 ft/min, short (seeds 4, 12, 13, 16, 28,
  40); from 0.75 to 0.86, she floated (9, 21, 31, the last 875 m along).
  Her approach throttle swings from 0.25 to 1.0 within a second as gusts
  take her speed, and the gusty flare's power is taken from that one
  instant (`flare_throttle_`).

**Every knob of the gust flare, tried on her alone** (passes of forty;
as committed, 28):

| Knob | Values tried: passes |
|---|---|
| `gusty_retard_ft` (her figure) | 5: 28, 10: 27, 15: 26, 20: 25, 30: 20 |
| `gusty_closing_nose_degps` (her figure) | 2: 28, 3: 27 |
| `gusty_most_nose_down_deg` (her figure) | 2: 28, 3: 28 |
| `gusty_aileron_per_deg` (her figure) | 0.05: 28 |
| `gust_additive_share` (her figure) | none: 27, 0.3: 30, all to 20 kt: 26 |
| Her approach speed, times 0.95 / 0.9 / 0.85 / 0.8 | 28 / 27 / 27 / 19 |
| The flare's power floor, half of `flare_throttle_` | none: 29, a quarter: 31, 0.3: 29, 0.75: 11 |
| `flare_throttle_` her approach's mean over 1 / 2 / 4 / 8 s | 30 / 29 / 30 / 30; over 4 s with a 0.3 floor, 33 |
| Lead of the sink's trend, times 0.5 / 0.7 / 1.5 | 24 / 29 / 25 |
| The trend's filter, 0.15 / 0.5 / 1 s (0.25 s) | 29 / 24 / 26 |
| The let-down for a coming climb, a degree a 100 ft/min a step, times 0 / 0.01 / 0.03 / 0.1 / 0.2 / 0.5 / 2 | 0 / 1 / 11 / 21 / 26 / 27 / 28 |
| The nose let down a further 1 / 2 degrees past where her flare began | 28 / 20 |
| The gusty touch, 100 / 200 ft/min (150) | 27 / 26 |
| The gusty round out, 0.05 / 0.1 g (0.07) | 16 / 25 |
| The flare's nose rate let through, 0 / 2 deg/s (1) | 27 / 26 |
| The flare's attitude gain, 0.03 / 0.08 (0.05) | 11 / 28 |
| The flare's rate damping, 0.025 / 0.1 (0.05) | 27 / 21 |
| The gust cushion's gain, 0.002 / 0.008 (0.004) | 29 / 28 |
| The cushion from 0 / 50 / 300 ft/min past the flare's sink (150) | 22 / 26 / 28 |
| The approach's speed gain, times 0.2 / 0.3 / 0.5 / 1.5 | 25 / 24 / 27 / 27 |

The best of them, the flare's power from her approach's four-second mean
with a 0.3 floor (33 of 40), passed 58 of seeds 41 to 120 where as
committed she passes 56: no better than the forty seeds' own scatter, about
three seeds either way. Nothing beat that. Each figure moves which seeds
fail, not how many; the gust flare's laws are not what holds her at 70%.

**Searched first** (2026-11-05):
- fighter landing in gusts: AFMAN 11-2F-22A volume 3 (found, but its PDF
  is refused to a script and is not archived), AFMAN 11-2F-16 volume 3
  (read, from the Internet Archive): section 3.17, "Final approach normally
  is flown at 11 degrees AOA", and no gust additive; AFMAN 11-2F-15E
  volume 3 (read): the aerobrake is flown with flaps down, and no approach
  angle or gust additive; T.O. 1F-16C-1 and 1F-15A-1 (not public; forum
  accounts only: an F-15 approach at 21 units, about 10 degrees, an
  aerobrake at 23);
- F-22 landing technique, approach speed and angle of attack: nothing
  official; simulator forums give 12 degrees and 140 to 150 kt, not
  citable;
- gust additives: half the gust factor (the FAA's Airplane Flying Handbook),
  half the steady wind and all the gust to 20 kt (several makers;
  Business & Commercial Aviation, November 2020); Gulfstream's G500 after
  AD 2022-10-05;
- fly-by-wire flare laws and turbulence: NASA's F-8 DFBW landings in
  turbulence (pitch only slightly degraded, roll severely); Field
  (Cranfield, 1994), separate flare laws; the flight-path and
  attitude-command flare patents (US 4,956,780 and 5,036,469), on rate
  command's over-flare;
- delta and tailless landings: NASA TM X-125 and the XF-92A reports, on
  flares begun higher at low lift-drag ratio.

**The sourced technique tried, and refuted here**: fly her final at a
fighter's angle of attack. The F-16's 11 degrees (AFMAN 11-2F-16) and the
F-15's about 10 are higher than her 7, so she would be flown slower, and a
gust's change of incidence would then be a smaller share of her lift.
Slowed to 0.95, 0.9 and 0.85 of her approach speed (about 9 to 10 degrees
of alpha at the slowest), she passed 28, 27 and 27, and at 0.8, 19: no
better. Since her approach speed is also every calm-air test's, nothing
would be taken on a gust sweep's say alone in any case.

**What would change her, untried**: her airframe's answer to a gust, not
the lander's - her model's lift and pitch damping near the ground, or her
flight controls' gains with her gear down (tools/make_f22.py). Her flight
manual is not public, so either would be tuned, not sourced. The owner's
call whether her per-seed names close her part of the gust item on their
measured figures (as the owner's rule of 2026-11-04 allows a single seed),
or whether her airframe is to be tuned.

**Verification**: the tables above, each a forty-seed sweep on
linux-release of a scratch build (not committed); `git diff` of this
branch is this file and one sentence of docs/COMPLETION_PLAN.md. No test,
no figure and no name changed, so no gust line of any aeroplane moves, and
no Windows or macOS measurement was needed.

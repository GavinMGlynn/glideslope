### In gusts the flare's power is worked on the sink when a jet's engines will answer, measured on her model; nine of fourteen land within limits on three seeds, five are named, 2026-10-10 — item still open

**What is missing first.** Five are still named in the gusts test: the
737-300 balloons at 113 ft/min in her flare (seed 2, against 100); the A380
meets the runway at 367 ft/min (seed 1) and balloons at 196 (seed 2); the
F-15C meets it at 397 (seed 2) - she passed every seed by 13 ft/min before
this round, and the cushion worked on her spool-up costs her that seed; the
Mosquito's main wheels' touch pitches her nose up from 3.6 to 6.8 degrees
against an elevator already at three quarters of its nose-down travel, and
she bounces 2.9 ft (seed 2) and 2.4 (seed 3); and the S.23, whose flare is
flown to her centre of gravity rather than her hull (Later). Named before
this round: 737-300, 787-8, A380, Learjet 35A, C182, Mosquito, S.23, A320,
F-15C. Un-named, each passing every seed by 25 ft/min and more: 787-8,
A320, Learjet 35A, C182.

**Spool-up, measured on the model** (`sim::spool_up_s`, into
`ApproachSpeeds::spool_s` by `approach_speeds`): each jet standing on her
brakes, her throttle at the approach's spooled quarter for ten seconds and
then opened fully, the seconds until her first engine's N1 has come 90% of
the way to where full throttle takes it in twenty; measured once a model in
a process. 737-300 and A320 5.21 s, 787-8 8.19, A380 7.56, Learjet 3.53,
B-2A, F-15C and F-35B 1.77; every propeller aeroplane 0. (From the idle
stop they are 2.9 to 12.2 s; the approach never flies there.)

**What changed** (sim/lander.cpp; in gusts alone):
- **The cushion is asked of the sink predicted a spool-up ahead** (the
  sink's trend, which carries the gusts' vertical component), and taken off
  by the same prediction - not by the sink now, which had a 737-300's full
  throttle reach 92% N1 as her sink was arrested. **And not asked where it
  would arrive after the touch**: where half her spool-up is more than the
  time left (her wheels' height over her sink), her power stays where the
  approach left it. (The whole spool-up there put the F-15C over 360 on two
  seeds; half of it, one.) This ended the Learjet's and the 787-8's
  balloons and hard touches and the A320's thin margin: 737-300 seed 2 from
  425 ft/min and a 292 balloon to 325 and 113.
- **At the touch, the throttle closed at once** (the AFH: retarded to idle
  once the main wheels are on), not at a hand's pace; and **firmer forward
  pressure** while she can fly - four degrees under the attitude she touched
  at, not two. This ended the C182's rise (1.04 ft, seed 1, to 0.11). A
  jet's nose already comes down and her spoilers out from the touch, as
  before; the A380's 2.6 ft rise went with the cushion (0.44).

**Verification:**
- New: `every_jets_spool_up_is_measured_from_its_model_and_a_propeller_has_none`
  - all fourteen taught the approach, each jet's above 0 and under 10 s and
  each propeller aeroplane's 0, coverage asserted. Seen red: measured from
  the idle stop, the 787-8's 12.2 s and the A380's 11.2 failed it, which is
  what moved the measurement to the approach's quarter throttle.
- The six gust tests pass with nine judged on every seed - 787-8, A320,
  B-2A, C172P, C182, F-35B, J-3 Cub, Learjet 35A, PA-28 - least margins
  sink 30 ft/min (787-8, seed 1: 330), touchdown zone 191 m (A320), rise
  0.56 ft, flare climb 34 ft/min (B-2A). **Seen red**: the 787-8, A320,
  Learjet and C182, judged now, were named on the commit before for
  failing (365 ft/min; 108 ft/min balloon; 1.14 ft rise) or margins under 25.
- 159 tests matching land, lander, learnt, go_around, approach, gust, flare,
  circuit, taken_back, handed and spool pass (linux-release).
- Local linux-debug: seed 1's first half 11.7 s, the spool-up test 2.9 s.
- The selftest hash does not move: 182dd6c996e0ee4c.

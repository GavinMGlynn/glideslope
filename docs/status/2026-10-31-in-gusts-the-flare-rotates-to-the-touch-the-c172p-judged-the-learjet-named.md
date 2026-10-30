### In gusts the flare rotates to the touch: the C172P judged, the Learjet named - two of fourteen, 2026-10-31 - item still open

**What is missing first.** Twelve of the fourteen are still named in the
gusts test. Touch sink in moderate turbulence scatters by about 80 ft/min
(one standard deviation) for every aeroplane here, as real landings do;
against a 360 ft/min limit with a 25 ft/min margin on eight seeds and three
platforms, an aeroplane is judged robust only if her mean touch is far
below the limit, which most are not. Still failing over forty seeds
(linux-release, x86-64-v3): Learjet 35A none here but one on Windows
(below), F-15C 18 of 40 (hard touches to 482 ft/min),
A380 18 (balloons to 223 ft/min and a go-around), 737-300 13 (balloons to
203), B-2A 8 (a wingtip down), Mosquito 6 (bounces, her main wheels'
touch), 787-8 6, J-3 Cub 5, C182 2, F-35B 1 (412 on seed 40), A320 3
(flare climbs to 101), S.23 33 (not touched here; it awaits the owner).

**What changed** (`sim::Lander::fly_laws`, in gusts only - calm air flies
exactly as before):
- **A jet's attitude hold in the last fifth of the flare lets go whenever
  she sinks faster than the flare wants**, not only from 150 ft/min past
  it. Frozen while the sink grew, an A320 settling after a gust's lift met
  the runway at 418 ft/min with her nose held at 3.3 degrees from nine feet
  (asked for five), an F-15C at 451 with hers at ten (asked for twelve).
- **The flare's pitch damping lets a nose-up rate through while the
  attitude asked rises**: up to a degree a second is not damped. Damped
  whole, an F-15C asked for 1.5 degrees more nose turned
  at a degree a second against it and stayed a second and a half behind
  the flare to the touch. **It is a tuned bias, not a measured rate**:
  the attitude asked moves with every gust, and any rise of more than
  1/120 degree in a 120 Hz step reaches the cap, so it lets through a
  degree a second of nose-up rate or none. The cap is tuned from a sweep:
  0.5, 1, 2 and 3 deg/s, 1 the fewest failures. The true rate (the
  attitude asked low-passed over 0.25 s before differencing) did worse
  over the same forty seeds: 103 failures capped at 1 deg/s, 168 at 6
  (balloons), against 80; at 0.5 s and 6 deg/s, 165. The comparison with
  `lower_the_nose`, which damps a planned rate, does not hold.
- Tried and rejected: holding the power while she sinks faster than wanted
  (balloons on every jet, failures 100 to 170 of 520); no nose lowering
  when a balloon is predicted (no better).

**Research.** FAA-H-8083-3C ch. 9, "Turbulent Air Approach and Landing"
(power on to the touch, half the gust factor) and "Ballooning" (hold the
landing attitude and let her settle, a little power to cushion, never a
push near the ground); Boeing's FCTM gust correction (half the steady
headwind plus the full gust, the gust held to the touch) as reported by
line pilots; Boldmethod's balloon recovery (keep a nose-high attitude,
relax back pressure, a slight power cushion). None of them asks for a
frozen attitude while the sink grows.

**Figures, forty seeds each, failures by the test's margins (13
landplanes, 520 landings)**: 100 before, 80 after. Clean on all forty:
none before; C172P, Learjet 35A and PA-28 after. By aircraft, before to
after: 737-300 14 to 13, 787-8 4 to 6, A320 12 to 3, A380 19 to 18, B-2A 4
to 8, C172P 3 to 0, C182 4 to 2, F-15C 22 to 18, F-35B 7 to 1, Cub 5 to 5,
Learjet 1 to 0, Mosquito 3 to 6, PA-28 2 to 0. The B-2A and Mosquito are
worse and stay named. Mean touch, before to after: C172P 198 to 146 ft/min
(sd 91 to 53), A320 272 to about 205, F-35B 257 to about 220.

**Judged now**: C172P and PA-28 - two of fourteen, as before, but not the
same two. The C172P was named for 419 ft/min on Windows (CI run
38006839738, under the lander of 2026-10-10); on forty seeds here her
worst is 335, and CI run 41077933204 passed her on Linux and Windows
debug - her "every platform" waits on the full run. **The Learjet 35A is
named, since this change**: clean on forty seeds here (worst
flare climb 62 ft/min), she climbed at 85 in her flare on Windows debug,
seed 7, in that same run - judged before, she now fails a platform, and
the rule names her. That is the scatter above: forty seeds on one
platform sample a tail that three platforms' eight seeds each reach past.
The named list's reasons for the A320, C182, Cub and F-35B now give their
forty-seed figures.

**Verification:**
- The full linux-release suite passes, 1419 of 1419 (first round). The
  eight gust tests, the stabilized-gate gust test, the light-and-heavy
  calm landings, the headwind and tailwind roll-outs and every test
  matching gust, flare or limits pass (linux-release, 50 tests).
- Seen red: with the gust touch aimed at 250 ft/min instead of 150, seeds 2
  and 7 fail the C172P (361, 356 ft/min) and seed 7 the PA-28 (342);
  reverted.
- The forty-seed sweep was a local test reading its seeds from the
  environment, not committed: forty seeds of fourteen aeroplanes is about
  five minutes of linux-debug, too slow for the suite.
- The selftest flies the C172P from a fixed input log without the lander,
  so its hash cannot move: 197366fd3fe6a7dc.

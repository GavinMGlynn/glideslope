### The F-22A lands in gusts on twenty-eight seeds of forty: her flight controls fed her body's rotation, not the air's - 2026-11-05 - item still open

**What is still missing, first.** The gust item stays open, and so does
"The F-22A flies her stall-figure tests". The F-22A is off
`not_yet_landed_in_gusts()` (tests/unit/test_lesson.cpp), but she does not
land twelve of forty seeds on Linux. Each is named on its seed alone
(`named_on_a_seed`), with its figure (below). On Windows release she does
not land fourteen, each named there. macOS is not yet seen with this change.
The A380 is still named whole.

**What now works**: on linux-release and on an `-march=x86-64-v2` build
(no fused multiply-adds, as CI's), the F-22A lands within every limit with
every margin on twenty-eight of the gust tests' forty seeds, where she
landed on two. She is judged on every seed she is not named on. Every other
aeroplane's forty gust lines are the same to the bit, before and after.

**The cause.** Her flight controls read her rates against the air, not her
body's:
- the pinned model's pitch, roll and yaw loops read
  `velocities/[pqr]-aero-rad_sec`, nine times in `<flight_control>`;
- JSBSim's aero rates are her body rates less its turbulence's rotation
  (FGAuxiliary.cpp: `vAeroPQR = in.vPQR - in.TurbPQR`);
- in turbulence, that rotation is MIL-STD-1797A's (FGWinds.cpp, eqs. 33 to
  35): the air's own pitch rate, from the change of its vertical gust over
  her span (pi/4b), and her span is short, so it is large for her;
- her pitch loop is an integrator on the rate error, so it held her rotation
  against the air at what her stick asked, and the air's rotation became
  hers. Traced on seed 1 (linux-release): her stick beat from stop to stop
  in the flare, her nose swung 7 degrees in 0.4 s, and she met the runway at
  796 ft/min. On seed 2, her stick 0.3 nose-up for a second, her nose fell
  from 5.5 to 3.8 degrees.

**The fix** (tools/make_f22.py, `with_inertial_rate_gyros`; **sourced as
physics**, not from her flight manual, which is not public): a rate gyro
senses the rotation of what it is fixed to, not the air's. The F-16's pitch
loop takes its rate from four pitch-rate gyros (the RSAF's 2024
investigation of its F-16 crash at Tengah, as reported by Janes and Defence
Connect). Her flight controls now read `velocities/[pqr]-rad_sec`, her body
rates against the earth. Not `velocities/[pqr]i-rad_sec`, against inertial
space: the earth's rotation (0.004 degrees a second) is below anything
measured here, and without it her calm-air flights are the same to the bit,
because JSBSim's turbulence rotation is exactly 0 with no turbulence. Her
aerodynamics keep the air's rates: damping is the air's. No lander code and
no figure is changed. Nothing is tuned.

**Tried and not taken** (linux-release, her forty seeds, passes with every
margin, by a local scorer; before the gyro fix, then after it):

| Variant | Before | After |
|---|---|---|
| As it was | 2 | 28 |
| Her flare flown as a pitch-rate ask, 0.04 / 0.07 / 0.1 stick a deg/s | 1 / 4 / 1 | - / 23 / - |
| The same, 0.07, no damping on her own rate | 3 | 25 |
| Her flare's gain at 1, not 3 | 1 | - |
| Her flare's gain at 2, not 3 | - | 26 |
| No gust additive / a third / Boeing's full gust to 20 kt | 0 / - / - | 28 / 30 / 26 |
| Her flare flown on her power, nose held | 0 | - |
| Her nose let rise 0.5 / 2 / 3 deg/s while closing | - | 29 / 28 / 27 |

After the gyro fix, the lander figures moved her between 23 and 30 of 40,
each reshuffling which seeds fail: no change a sweep of forty could tell
from noise. None is taken. The lander is unchanged.

**Forty seeds, before and after** (the test's own judgement):

| Build | Before | After |
|---|---|---|
| linux-release (FMA) | 2 | 28 |
| x86-64-v2 release | 2 | 28, the same figures line for line |
| Windows release | not seen: named whole, not shown | 26 |

**Named, per seed, on Linux** (linux-release and x86-64-v2 alike):

| Seed | What she does |
|---|---|
| 9 | goes round from a float: she would touch 1,071 m along |
| 10 | a gust climbs her at 84 ft/min in her flare |
| 11 | rises 1.23 ft off her wheels after her touch |
| 13 | touches at 462 ft/min |
| 15 | touches at 355 ft/min and rises 1.03 ft |
| 20 | a gust climbs her at 143 ft/min in her flare |
| 21 | goes round from a float: she would touch 1,031 m along |
| 28 | touches at 383 ft/min and rises 1.15 ft |
| 32 | a gust climbs her at 104 ft/min in her flare |
| 34 | a gust climbs her at 88 ft/min in her flare |
| 39 | a gust climbs her at 79 ft/min in her flare |
| 40 | touches at 460 ft/min |

**Named, per seed, on Windows** (tools/windows_build.sh windows-release;
a Windows name covers every Windows build):

| Seed | What she does |
|---|---|
| 1 | a gust climbs her at 100 ft/min in her flare |
| 3 | touches at 351 ft/min |
| 5 | a gust climbs her at 80 ft/min in her flare |
| 9 | goes round from a float: she would touch 1,052 m along |
| 11 | a gust climbs her at 75 ft/min in her flare |
| 15 | a gust climbs her at 77 ft/min in her flare |
| 16 | a gust climbs her at 77 ft/min in her flare |
| 17 | rises 1.04 ft off her wheels after her touch |
| 19 | a gust climbs her at 113 ft/min in her flare; touches 916 m along |
| 22 | a gust climbs her at 144 ft/min in her flare |
| 23 | a gust climbs her at 136 ft/min in her flare |
| 28 | touches at 335 ft/min, rises 0.80 ft, climbs at 80 ft/min |
| 31 | a gust climbs her at 121 ft/min in her flare |
| 37 | touches at 362 ft/min |

Only seed 9 fails on both. Touch sink scatters between platforms, so one
platform's pass is no other's (as the gust item says of every aeroplane).
On Windows release, every one of the forty seeds' failures before these
names was the F-22A's: every other aeroplane passed or was already named.

**Searched first** (2026-11-05): F-22 landing technique, approach angle of
attack and gusty winds; AFMAN 11-2F-22A's gust and crosswind approach
speeds; T.O. 1F-16 and F-15 gust additives and approach angles of attack;
NASA and DTIC reports on fly-by-wire fighter landings in turbulence; rate
command and flare laws. Found: no F-22 figure (her flight manual is not
public); civil gust additives (half the gust factor, the FAA; half the
steady wind and all the gust, to 20 kt, several manufacturers; code7700,
Business & Commercial Aviation, November 2020); FAA AD 2022-10-05 on the
Gulfstream G500/G600, whose alpha limiter engaged in a flare "in unstable
air", gusty winds, for a hard landing; Field (Cranfield, 1994): rate
command laws float in the flare, and most fly-by-wire aircraft fly a flare
law of their own. Then the JSBSim definitions of the aero rates, and the
F-16's pitch-rate gyros, which gave the fix.

**Verification**:
- `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1` to
  `_40` pass on linux-release and on the x86-64-v2 build, the F-22A judged
  on twenty-eight seeds and named on twelve.
- Every other aeroplane's forty gust lines are compared before and after on
  linux-release: 600 lines, the same to the bit.
- **Seen red**: with seed 13's name taken off, seed 13 fails ("f22 (gusts,
  seed 13) touched at 461.848071 ft/min, not 25 inside 360"); restored, it
  passes. A name on a seed she passes fails "take her name off".
- Windows release (tools/windows_build.sh, this branch's first commit):
  seeds 1 to 40 fail on the fourteen seeds above and on no others, each for
  the F-22A alone; with the fourteen named, seeds 1 to 40 pass.
- The linux-release suite but the nightly and gust-sweep labels, no
  display, in three sittings of at most 30 minutes: 1,454 of its 1,469
  tests ran, every one passed or was skipped (the live recordings, with no
  key). The 15 not reached are multi-process client-and-server tests
  (prediction, take-over, weather); they are left to CI.
- `python3 tools/make_f22.py --check` passes; the committed model is what
  the script makes.

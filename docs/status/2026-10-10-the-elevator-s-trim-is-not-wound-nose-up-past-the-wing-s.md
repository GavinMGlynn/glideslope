### The elevator's trim is not wound nose-up past the wing's peak: every aeroplane left thirty seconds in a stall within 2 g; go-around flaps sourced and not kept, 2026-10-10 — item still open

**What is still missing, first.** At the warning the Learjet 35A (385 of
350 ft) and the Short S.23 (184 of 200, inside the lesson's height but not
with the 10% in hand) are named, and the Mosquito is never level again
(3,095 ft lost by the flight's end). Nothing is named left thirty seconds.
The AI's notice keeps its two names (the A380, the Mosquito).

**The rule** (src/sim/autopilot.cpp, the elevator's trim): the trim
integral is not wound nose-up while the angle of attack is past the angle
the wing's lift peaked at (by the three tenths the stall entry uses) - a
pitch a stalled wing cannot have, as the upset rule already says for a
pitch an upset cannot have. Nose-down it still winds. **Why**: traced on the
A320 (the entry of 2026-10-10 below), the 1.95 g came from her trim wound to
its nose-up stop while the altitude hold held her height into the stall;
the recovery inherited the elevator at +0.9 and dived to 168 kt with it.
That winding is the lesson's entry's doing (the altitude hold asked for a
pitch the stalled wing could not give), not the recovery's.

**Seen to fail**: with the rule stepped over and the two names off, the
check left thirty seconds red - "a320 load: 1.896825 g ... mosquito-fb6
load: 2.223588 g, against 2.000000 with 10% in hand" - and green with it.

**The figures** (linux-release; lost / bound, peak g). At the warning
nothing moved (the rule binds only past the peak):

| Aeroplane | at the warning | left 30 s: before -> now |
|---|---|---|
| 737-300 | 777 / 1,300, 1.13 | 1,235 / 2,492, 1.74 -> 968 / 1,928, 1.55 |
| 787-8 | 657 / 1,300, 1.16 | 766 / 1,740, 1.20 -> same |
| A320 | 584 / 1,300, 1.20 | 1,332 / 1,602, **1.90 named** -> 1,313 / 1,606, **1.81** |
| A380 | 908 / 1,300, 1.30 | 1,484 / 2,908, 1.66 -> 1,018 / 1,957, 1.58 |
| B-2A | 113 / 300, 1.14 | 999 / 2,002, 1.61 -> 908 / 1,906, 1.59 |
| C172P | 75 / 300, 1.05 | 75 / 568, 1.29 -> same |
| C182 | 71 / 300, 1.05 | 38 / 539, 1.53 -> 24 / 458, 1.38 |
| F-15C | 303 / 500, 1.33 | 1,431 / 3,467, 1.60 -> same |
| F-35B | 419 / 500, 1.13 | 4,502 / 7,436, 1.71 -> 3,565 / 6,294, 1.70 |
| J-3 Cub | 29 / 300, 1.05 | 42 / 308, 1.26 -> 38 / 307, 1.19 |
| Learjet 35A | **385 / 350 named** | 434 / 1,359, 1.26 -> same |
| Mosquito FB.VI | **never level, named** | 1,258 / 1,899, **2.22 named** -> 904 / 1,484, **1.71** |
| PA-28 | 131 / 300, 1.20 | 120 / 515, 1.26 -> 124 / 503, 1.14 |
| Short S.23 | **184 / 200 named** | 146 / 640, 1.33 -> 142 / 611, 1.34 |

The A320's 1.81 g is inside 2 g with the 10% in hand by 0.009 g (1.81 x 1.1
= 1.991); on another machine it may not be, and CI will say.

**Go-around flaps, sourced, tried and not kept.** Each aeroplane's
published go-around flap, read:
- **Learjet 35A**: 20 degrees. Learjet 35A/36A AFM FM-108 change 23 (the
  archive.org copy docs/ASSETS.md pins), section II, GO AROUND, page 2-36:
  "Thrust Levers - Set to takeoff power or as required. SPOILER Switch -
  Check RET. Flaps - 20°."
- **Mosquito FB.VI**: 15 degrees. A.P. 2019E Pilot's Notes, 1950 (the
  Zeno's copy ASSETS.md records), Part II paragraph 48, "Mislanding and
  going round again": "Raise the undercarriage and while it is retracting
  raise the flaps to 15°, and re-trim."
- **737-300**: 15 degrees. Boeing 737 CL FCTM, revision 20, page 5.58,
  Go-Around and Missed Approach - All Engines Operating: "call for flaps 15"
  (copy at aviationtricks.com, FCTMB737CLRev20.pdf).
- **A320**: configuration 3, flaps 20 (from FULL, flaps 40): the FCOM's
  go-around retracts the flaps one step, as quoted by airbusdriver.net's
  Go Around page; Airbus's own FCOM is not public.
- 787-8 and A380: not found in a public source in the time.
Given to the recovery as it raises the Cherokee's (from the hand-over), the
flaps' lift went before the speed came: at the warning the 737-300 lost
1,136 ft of 1,300, the Learjet 864 of 350, the Mosquito 7,176 of 600, and
the A320 left thirty seconds pulled 2.32 g. Raised only once the wing was
unloaded and within 5 kt of the speed asked: the 737-300 2,289, the
Learjet 400, the Mosquito 7,435. Raised only when also still sinking there:
the same. Each go-around flap is a setting for a go-around from the
approach speed, 1.3 times the landing stall; a stall recovery at 5 knots
over the stall has not the speed to give the flaps' lift up. None is in
the figures files; the Cherokee's alone stays, which she needs to be level
at all.

**The Short S.23** is named only by the 10% margin: 184 ft against her
lesson's 200, and 184 x 1.1 = 202.4. The margin is the one tolerance every
recovery figure is held to, for what another machine's floating point
moves it by (CLAUDE.md's cross-platform tolerances); her lesson's 200 ft is
her class's. There is no evidence that either is wrong for her - nothing
measured on another platform puts her nearer 200 or further - so neither is
changed and she stays named.

**Verified** (linux-release, all targets, on #155 restacked, c6cccbb1): the
286 tests matching stall, recover, upset, lesson, instructor, take, glide,
orbit, rolled or autopilot: all pass. The selftest hash does not move,
`182dd6c996e0ee4c`.

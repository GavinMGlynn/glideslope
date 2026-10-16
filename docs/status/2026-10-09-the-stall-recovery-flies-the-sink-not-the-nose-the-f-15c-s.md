### The stall recovery flies the sink, not the nose; the F-15C's units looked for and not found, 2026-10-09 — neither item done

**What is still missing first.**
- **Stall recovery:** at the warning the F-15C (543 ft of 500), F-35B (773),
  Learjet 35A (409 of 350) and Short S.23 (184 of 200, inside it but not with
  the 10% in hand) are past their lesson's height, and the Mosquito is never
  level again (3,095 ft lost by the flight's end); left thirty seconds, the
  A320 (1.95 g) and the Mosquito (2.22 g) are over 2 g. The AI pilot still
  does not notice a stall.
- **The F-15C's stall:** unchanged - she settles at 42.4 degrees and 116.6
  knots; the speed is missing, and no source turning the manual's units into
  degrees was found (below).

**What the A320's 2.18 g was.** The last entry read it as the wing's lift
returning under full nose-down elevator. Its sign was misread: the
autopilot's elevator is +1 nose **up** (`sim::Controls`). Traced, the A320
was handed over at 156 kt, 26.7 degrees of alpha and sinking 10,000 ft/min
with her pitch command 16 degrees above her nose and her elevator trim wound
full nose-up from holding the height into the stall; the command walked down
at 8 degrees a second, so for two seconds the elevator stayed full nose-up
while she dived to 166 kt, and her wing came back through its lift's peak at
178 kt - 2.18 g. At that speed any wing passing its peak pulls (178/120)² g.
The Mosquito's 2.22 g is the same crossing, at 146 kt.

**The law now** (`src/sim/autopilot.cpp`, the airspeed on the elevator):
- **A pitch above the nose is not asked of a stalled wing**: while the wing
  is past its stall the command comes down to the nose at once. The elevator
  still moves at a hand's pace.
- **The speed comes from the engine and from the least sink**, not from the
  nose: the vertical speed's own gains ask for a sink of 20 ft/min for each
  knot short, and where the speed gains slower than 0.3 kt a second, 50
  ft/min more each second for each knot a second it is slow, until it is at
  the speed. Half a degree of nose down for each knot short, as it was, dived
  for the speed.
- **A schedule on the angle of attack**: the wing is held a tenth below the
  angle the autopilot learnt it stalled at (it learns a little past a steady
  stall: 17.2 degrees in the 172P against the test's 16.0, where she mushed),
  and below the angle that would pull 1.6 g at this speed, from the lift
  coefficient now, the weight and the lift's mean slope up to its peak.
- **Once at its speed and unstalled the nose goes down at 3 degrees a
  second**, not 8.

**Tried and not kept.** Easing the elevator as the angle comes back below the
stall (the caller's suggestion) cannot help the crossings: the lift answers
the angle, so the slower the wing passes its peak, the longer it is there.
Unwinding the elevator trim 3 to 30 times faster while stalled brought the
Mosquito to 1.91 g but put the A320 and A380 past their heights, the A320
at 2.57 g. Allowing the nose to 20 or 30 degrees in the recovery saved the
F-35B 30 ft. A sink integral that wound on the speed short rather than on
its gain sank the airliners thousands of feet.

**The figures, linux-release** (lost from the hand-over to the lowest point
before recovered; g the peak quarter-second mean):

| Aeroplane | (a) at the warning: lost / lesson, g | was | (b) left 30 s: lost / bound, g | was |
|---|---|---|---|---|
| 737-300 | 773 / 1,300, 1.15 | 902, 1.31 | 1,223 / 2,476, 1.72 | 1,314, 1.78 |
| 787-8 | 657 / 1,300, 1.16 | 1,031, 1.37 | 766 / 1,740, 1.20 | 1,072, 1.22 |
| A320 | 610 / 1,300, 1.20 | 917, 1.31 | 1,240 / 1,584, **1.95 named** | 1,535 named, 2.18 named |
| A380 | 908 / 1,300, 1.30 | 962, 1.52 | 1,484 / 2,908, 1.66 | 1,754, 1.78 |
| B-2A | 112 / 300, 1.14 | 414 named | 998 / 2,001, 1.61 | 921, 1.64 |
| C172P | 73 / 300, 1.04 | 104 | 148 / 569, 1.04 | 113 |
| C182 | 78 / 300, 1.06 | 149 | 48 / 546, 1.52 | 73 |
| F-15C | **543 / 500 named**, 1.51 | 590 named | 1,527 / 3,877, 1.66 | 1,493, 1.65 |
| F-35B | **773 / 500 named**, 1.31 | 932 named | 3,252 / 6,257, 1.68 | 3,018 |
| J-3 Cub | 29 / 300, 1.05 | 36 | 42 / 308, 1.26 | 54 |
| Learjet 35A | **410 / 350 named**, 1.26 | 589 named | 480 / 1,401, 1.28 | 586 |
| Mosquito FB.VI | **not recovered, 3,095 lost, named** | 3,462 | 1,258 / 1,899, **2.22 named** | 2.23 named |
| PA-28 | 132 / 300, 1.14 | 328 | 126 / 514, 1.13 | 205 |
| Short S.23 | **184 / 200 named** (not with 10% in hand) | 329 named | 323 / 640, 1.58 | 264 |

Names off: the B-2A's height at the warning, and the A320's height left
thirty seconds. Every remaining name is held to its new figure plus 10%.
The Learjet loses most of hers in the first ten seconds with her elevator
at full nose-up: full power pitches her nose down past what the elevator
holds. The F-15C and F-35B are handed over at their warnings already
sinking 3,400 and 4,400 ft/min. The Mosquito at 20,000 ft with her flaps
and gear down cannot be level at her recovery speed. By the book (handed
over at the entry's end) every aeroplane ends the lesson; the Mosquito loses
504 ft.

**Engaging the recovery and letting it go steps no control** (the plan
item's last test): `engaging_the_stall_recovery_and_letting_it_go_moves_no_control_faster_than_a_hand`
(test_lesson.cpp) - every aeroplane taught a stall (14 of 16, the 747-400
and F-22 named with no stall) handed to the recovery at its warning and left
thirty seconds, 28 flights, and let go for the altitude hold once recovered;
on neither step does the elevator, ailerons or rudder move more than 1/120.
The Mosquito at her warning is named as never let go (never recovered).
Most engagements move exactly 1/120: the hand's pace is what holds them.
**Not seen to fail**: the deliberate bug, removing the elevator's hand's
pace, was refused by this session's permission check and not run, so the
test is untrusted until someone watches it go red that way.

**The F-15C's units.** T.O. 1F-15A-1's full text (archive.org's OCR of the
scan cited in ASSETS.md) was searched: it gives AOA in units throughout and
no conversion to degrees, and section VI says the settled angle "varies with
c.g. position, aft c.g. giving higher AOA". An AFIT thesis on F-15 wing rock
(DTIC ADA256613) is reported to say the F-15D's gauge "is not calibrated in
degrees" and its calibration unknown; DTIC refused the download, so it is
not cited. The conversions found (degrees = (units - 3.715)/1.089; units =
0.7728 × degrees + 12.22) are forum posts with no source, and disagree.
Nothing was changed.

**The Cherokee on the FAA's curve, full rich below 5,000 ft, tried with this
law and not kept.** `make_pa28.py` given the float carburettor and the FAA's
mixture curve (as the 172P's) and the catalogue's `mixture-lever 5000`: by
the book she ends the lesson (213 ft), but recovered at her warning she
loses 1,223 ft against 300, and left thirty seconds 722 against her bound of
515. The least sink does not help her: "recovered" asks for level at the
lesson's recovery speed, and at that speed with 40 degrees of flap and full
power she cannot be level (the entry below); slower she climbs, but is
short of the speed. What is missing is a recovery the lesson can count -
the flap brought up to an intermediate setting as the FAA's recovery does,
or a recovery speed she can hold level - which is the lesson's, not the
engine's or the autopilot's. Both Cherokee items stay open; nothing of hers
is committed.

**Verified** (linux-release): every test matching stall, lesson, autopilot,
figure, committed, selftest or instructor (167, the new one among them):
none failed. **Seen to fail**: the two stall tests with their old names,
against this law - "b2 is named for its height and now meets 300 with 10%
in hand (112.4): take its name off", and the A320's height (1,240 against
1,584) the same.

The selftest hash does not move, `be036519d2c19ea0`: it flies no stall
recovery.

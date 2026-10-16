### The walking-pace tests' hand holds her at taxi power, and every strike is judged, 2026-10-15 — item still open (see the entry above)

**What is still not true first.** **She is rolled at walking pace for under
a second now, not half a minute**: at taxi power the autobrake stops every
landplane in 0.42 to 1.09 s (the C172P on a wet runway the least, against
`least_walking_s` of 0.4, as before), and she stands on it, taxi power on,
for the rest of the half minute. The autobrake's bound while rolling is
judged over that second; at a stop, over the rest. **The Mosquito's swing
was the old hand's too**, as far as these tests show (at taxi power she
turns 12.9 to 17.2 degrees); the Phase 10b item stays open to confirm
nothing else swings her. **The full suite was not run**: the tests below.

**This supersedes** the entry below where it says the Cub's walking-pace
run in 15 kt across still goes over and is named: at taxi power she stays
on her wheels and is judged. Braked in the sweep, stick central, she still
goes over (the open item "The J-3 Cub's nose is pushed down by a
crosswind").

**What the hand did, on the real output** (a per-step trace of the
C172P in linux-debug, dry). Handed over at 3.01 kt on 0.342 of throttle,
her autobrake came on - up to 0.3, then 0.45 as she slowed, by its law -
and stopped her in 1.25 s. The hand's error was then 3 kt, its
proportional term alone 0.9 of throttle, and its integral wound on: full
power by 1.1 s, against the brakes that had stopped her, her stick full
back (the rollout's, below half her reference speed). Her nose rose from
2.8 degrees at 1.25 s, pitching up at 24 deg/s by 2.2 s, and at 11.2
degrees her `TAIL_STRIKE` point met the runway 2.21 s after the hand-over;
she sat on it, 11.2 to 11.4 degrees, for the rest of the half minute. No
other landplane struck in the dry run; in 15 kt across the Cub struck her
airframe, at 16 degrees of pitch and 21 of roll, 4.9 s after the hand-over (linux-debug),
where linux-release took her on over (#201's -45 degrees).

**The fix, in the test's pilot.** `walking_pace_then_stop`
(tests/unit/test_lesson.cpp): the hand holding her at walking pace puts on
no more than her **taxi power** - the throttle that rolled her up to 3 kt
on the roll-up's slow ramp - as a pilot taxis, and does not open the
throttle against brakes that have stopped her. Its gains are unchanged.
And a `sim::GroundJudge` watches every step from the roll-up to the stop:
anything but her wheels touching the runway is wrong, on every landplane,
named or not, and the first strike is printed with when and how she sat.
**None is named now**: the Mosquito (named for her heading) and the J-3 Cub
in 15 kt across (named for going over) are both judged in full. The
walking time is printed to a hundredth.

**Verification.** The three walking-pace tests and the gone-over test pass
in linux-debug, linux-release and a `-march=x86-64-v2` debug build, every
one of thirteen landplanes judged and none named, no strike, figures the
same to the printed digit in all three (but the Mosquito's dry turn, 13.0
debug and v2, 12.9 release). **Seen red**, the strike: with the old hand
put back (its throttle clamped to 1.0, not taxi power) the C172P struck
her tail in all three tests at 11.1 to 11.2 degrees, 1.92 to 2.27 s after
the hand-over, and the Cub her airframe in 15 kt across; reverted, green.
**Seen red**, the autobrake judgement still biting at taxi power: with the
walking-pace ceiling lifted (`most = 1.0` in `Lander::fly`), all thirteen
were wound to 1.0 below walking pace and ten past 0.3 rolling (the
C172P, the Cub and the Mosquito stopped too soon to be caught rolling);
reverted, green.
The selftest hash does not move: no simulation code changed.

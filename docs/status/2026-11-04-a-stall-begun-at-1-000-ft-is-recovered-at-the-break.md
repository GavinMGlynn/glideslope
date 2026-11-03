### A stall begun at 1,000 ft is recovered at the break: all sixteen handed over above the ground - 2026-11-04

**What was not true.** In
`a_stall_recovery_whose_wing_stays_stalled_gets_full_power_within_five_seconds_and_near_the_ground_at_once`,
begun at 1,000 ft, eight aeroplanes were in the ground before the hand-over
and were named in the test: the 737-300, A320, A380, F-15C, F-22A, F-35B,
Mosquito and S.23. The plan's tail (from
2026-11-03-the-f22a-s-stall-figure-tests-investigated) named five of them.

**The cause, measured: not the recovery, which never began.** A trace of the
low flight (linux-release, not committed) shows every entry flown well: each
lost under 100 ft, at idle, to the end of the lesson's
entry (stall+25). The test then left each **thirty seconds in the stall at
idle, with the autopilot still holding the height**, before handing it to
the recovery. Its wing broke within about ten seconds, and from there it
sank with its nose up and its wing deep in the stall:

| aeroplane | entry ended | in the ground | sinking at | alpha there |
|---|---|---|---|---|
| 737-300 | 933 ft, 130 kt | 28 s after | 4,800 ft/min | 25 |
| A320 | 949 ft, 138 kt | 26 s after | 8,200 ft/min | 27 |
| A380 | 959 ft, 130 kt | 26 s after | 5,900 ft/min | 29 |
| F-15C | 927 ft, 130 kt | 16 s after | 6,400 ft/min | 43 |
| F-22A | 952 ft, 130 kt | 16 s after | 6,200 ft/min | tumbling |
| F-35B | 930 ft, 121 kt | 16 s after | 7,500 ft/min | 56 |
| Mosquito | 974 ft, 119 kt | 24 s after | 6,200 ft/min | 22 |
| S.23 | 968 ft, 80 kt | 21 s after | 9,100 ft/min | through the stall |

So none of it is the recovery technique the AI flies (FAA-H-8083-3C ch. 5,
the Airbus FCTM), and none is a model fault that shows here: a stall held
at idle sinks that fast. AF447's crew, holding hers, descended at about
10,000 ft/min (BEA final report, 2012). The A320 and A380 in normal law
would not be stalled at all (alpha protection); the lesson flies their
stall as in alternate or direct law, as before, and that is unchanged.

**What is flown low.** The FAA trains a landing-configuration stall at
1,000 ft AGL and recovers it on recognising it (AC 120-109A, appendix 3,
scenario 3: "At 1,000 feet above ground level (AGL), reduce thrust ...
Upon recognizing the impending stall, perform the stall recovery
procedure"). Nothing trains one held for half a minute at that height, and
AC 120-109A sets no altitude-loss figure for a recovery ("There is no
predetermined value for altitude loss"), so there is no published height
loss to judge the thirty-second figures against. The rule this test judges
low - under 1,500 ft the recovery gives full power at once, the wing still
stalled or not - needs the aeroplane handed over above the ground, not
thirty seconds of mush.

**The change** (tests/unit/test_lesson.cpp, test only; no model, autopilot
or lesson is changed):
- `fly_a_stall` takes `at_the_break`: the hand-over comes `left_s` after
  the entry ends, or sooner at the break - the wing past the angle its lift
  peaked at by 0.3 degree, the autopilot's own `past_the_peak_deg`.
- The low flight is flown so. Every aeroplane is required to be handed
  over above the ground; the named set is gone. The flight where practised
  is unchanged (thirty seconds).

**Figures now, low** (linux-release, FMA): handed over at 898 (737-300),
402 (747-400, which never breaks: thirty seconds), 496 (787-8, the same),
930 (A320), 932 (A380), 969 (B-2A), 637 (C172P, the same), 917 (C182), 511
(F-15C: her lift still rising past 39 degrees), 913 (F-22A), 891 (F-35B),
950 (Cub), 319 (Learjet, the same), 935 (Mosquito), 920 (PA-28) and 930 ft
(S.23). Full power at once in all sixteen; the wing still stalled at the
power in seven low (three before), ten where practised.

**Seen to fail.** With the low flight's `at_the_break` false (the old
flight): "8 recoveries held the power off too long or were never handed
over; the first: 737-300 begun at 1,000 ft was in the ground before the
hand-over", the eight above listed. Restored, it passes.

**The rule seen red under the new flight** (from the review of #311). With
the hold's floor in sim/autopilot.cpp, `least_unload_agl_ft`, set from 1,500
to 0 ft (so the power is held off low too), the test fails: "7 recoveries
held the power off too long or were never handed over; the first: 737-300
held back at 897.602808 ft had no full power at the hand-over: 4.991667".
The seven are the ones whose wing is still stalled at the hand-over: the
737-300, A320, A380, B-2A, F-15C and Mosquito, at 4.99 s, and the S.23 at
0.63 s. Reverted, it passes.

**The hand-over comes at the break**, later than the first indication AC
120-109A's scenario recovers at. That is so the wing can still be stalled
when the power comes, which is what the rule needs to be tested.

**Verification.**
- linux-release (FMA): the test passes, figures above.
- x86-64-v2 build (`-march=x86-64-v2`, no FMA, as CI): passes, every
  hand-over height the same to the foot.
- Windows debug (`tools/windows_build.sh windows-debug glideslope_tests`,
  MSVC): passes, 53.6 s.
- Every test matching `stall` in linux-release (`ctest -R stall -LE nightly
  -j4`, no display): 126 of 128 passed. The two that failed, in 0.01 s,
  were the client-rejoin tests, whose client and server programs were not
  built here (only the test binary was); CI runs them.
- The selftest's hash cannot move: only a test changed.

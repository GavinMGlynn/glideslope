### A plan's first leg strays further at the published climb speed because a turn's size goes with the square of the speed, 2026-10-13 — item done

**What is still missing, first.** Nothing of the item, which asked for the
cause and its explanation in the test; the stray is not removed, because
nothing the navigator does can lead a turn onto a leg that starts where she
is. Waypoint turns are not led either (the navigator turns once abeam),
which is the same geometry mid-plan and is a new open item in Phase 10a.
No behaviour changes, and no platform code, so it was not built on Windows.

**The premise was wrong.** The item expected the faster climb to stray
less, from the test's old account that the heading sat 2.5 to 3 degrees
short of the leg until the airspeed built. Those degrees are the
navigator's intercept, 30 per kilometre off the leg, at 80 m off: an effect
of the stray, not its cause. The cause is the turn onto the leg. The leg
starts where the take-off hands over, the aeroplane still on the runway's
heading, 18 to 24 degrees right of the leg's bearing. The autopilot banks a
degree per degree off the course, 25 at most, rolled at 5 degrees a second,
so she is carried along the old heading while the bank builds. The 172P's
bank peaks at 18.6, never 25, and her track comes round to the leg 8 s after
the hand-over, at her greatest stray. At a given bank she turns at
g tan(bank) / V and covers V metres a second, so the stray goes with V
squared.

**Logged** (the first-leg test, the 172P, every half second from the
hand-over, at her published climb speed and at the slower one for her
weight, `DepartureSpeeds::climb_for_any_weight` set false for the
measurement only):
- Published, 74.8 KCAS, 38.7 m/s true, track 68.0 against the leg's 44.4
  (23.7 off): her bank -5.97 at 2 s, -16.8 at 4, -18.6 at its peak at 5,
  -10.0 at 8; 30 m off at 2 s, 57 at 4, **79.7 at 8.0 to 8.5 s**, as her
  track crosses the leg's bearing; back to 38 m by 22 s.
- For her weight, 66.1 KCAS, 34.3 m/s true, 22.9 off: bank -18.1 at its
  peak at 4.5 s; 26 m off at 2 s, 48 at 4, **63.1 at 7.5 s**.
- (38.7 / 34.3)^2 = 1.27; the strays are 79.7 / 63.1 = 1.26.

**The geometry.** The steady circle at the bank limit, r = V^2 / (g tan 25),
is 262 m at 34.6 m/s and 331 at 38.9, but she never reaches 25 degrees; the
stray is set by the roll-in and the bank law, not the limit. So the test
now flies those laws alone at her speed - bank a degree per degree off the
course, 5 degrees a second, 25 at most; the course 30 degrees a kilometre
back toward the leg; turning at g tan(bank) / V at a constant true speed,
her bank the one asked at once (`turn_onto_leg_strays_m` in
tests/unit/test_navigator.cpp). The model reads the figures the autopilot
and navigator fly by, not copies of them: the bank per degree off, the roll
rate and the intercept are now in src/sim/turn_laws.hpp, which
autopilot.cpp and navigator.cpp include in place of their own constants,
and the bank limit is `most_bank_deg` (sim/plan.hpp), the autopilot's full
limit. A turn that has spent its 3 kt may bank less, but none of these banks
past 18.6 degrees. Retuning any of them moves the prediction with it.
Predicted / flown, metres:
- Published climb speed: 172P 78.3 / 79.8, 182S 79.0 / 80.0, Cub 39.3 /
  40.3, Cherokee 61.5 / 59.5.
- Slower climb speed for her weight: 172P 62.0 / 63, 182S 63.1 / 65, Cub
  28.4 / 30, Cherokee 46.7 / 46.
Within 2 m everywhere: nothing but the turn strays her, at either speed.

**The selftest hash does not move** (`182dd6c996e0ee4c`, linux-release):
the constants moved to a header and kept their values, so nothing flies
differently.

**Verified**, linux-release:
- `a_plan_that_takes_off_leaves_its_runway_and_flies_its_waypoints_in_every_light_aeroplane`
  now also checks each aeroplane's stray within 4 m of what her turn
  predicts from her true speed and the turn at the hand-over, as well as
  the 100 m it had. Its comment carries the cause and the figures above.
  The bound is 4 m, not a share of the stray, because the model's misses
  (bank lag, speed change, sideslip) are 1 to 2 m whatever the stray: the
  most is the Cherokee's 2.0 of 61.5. A share would give the Cub 2 m and the
  Cherokee 3. Twice the worst miss leaves another platform as much again.
  **Seen to fail** with the autopilot alone (not the shared figure) rolling
  at 4 degrees a second instead of 5: "c172p strayed 85.315147 m from its
  first leg, against the 78.348309 m its turn onto it at 38.744257 m/s
  true and 23.658199 degrees predicts (within 4)". At 3 a second it strayed
  93.5. Both are under the old 100 m bound, so the old check alone stayed
  green. Restored, green.
- Every test whose name speaks of the navigator, a take-off, a first leg, a
  plan, a departure, a heading or a ninety-degree turn, the docs test and
  the selftest's, but not the server, client or renderer ones: 207 by
  `ctest -j4 -R "navigator|take_off|takes_off|takeoff|first_leg|plan_|departure|selftest|heading|turns_ninety|docs" -E "server|client|vulkan|window|shot"`,
  203 passed and the 4 live-model `_now` tests skipped (no key given).

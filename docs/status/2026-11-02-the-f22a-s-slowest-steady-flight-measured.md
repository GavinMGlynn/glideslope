### The F-22A's slowest steady flight measured from her model; at 1.3 times it she flies her final on the centreline but touches over 360 ft/min - 2026-11-02

**What is still not true first.** The "runway too short" item stays open. The
F-22A still has no approach speed, no landing figure, and every landing is
still refused her:

- **At 1.3 times her measured slowest steady flight she touches too
  hard.** Her VS0, 90.0 KCAS at her combat loading, 54,960 lb, is 95.2 at
  her model's own 61,570 lb, at which the tests fly her: 1.3 times it is
  123.8 kt. From 5 nm she flies her final on the centreline (within 0.49 m
  in calm air, 2.47 and 2.80 m in 15 kt across either way) and rolls out on
  it, but touches at 392 to 417 ft/min, over 360. (`glideslope_cli land`
  flies her at its own full tanks, 63,960 lb, at about 126 kt, and from its
  2 nm gate she touches at 374 to 383 ft/min.) Her flare is the shortfall:
  below.
- So no stall figure is written to `assets/figures/f22.xml`: written, it
  would give her an approach speed, and every landing test would fly her.

The rounds below are in the order they were worked; figures in the earlier
ones are as they were then.

**Her roll-out, fixed (second round, 2026-11-02): her yaw loop's ground
mode.** At about 150 kt (a stall figure of 115 put in to try it) she flew
the approach well, touched at 130 ft/min 1 m off the line, and then a yaw
swing grew from under 1 to 18 degrees a second, the lander's rudder at its
stops, and she left the runway, 196 m across in calm air and 380 m in 15 kt
across. The cause: her pedals reach her rudders only through her flight
controls' yaw rate loop - a yaw rate commanded, an integrator on the rate
she is short of (clipped at 0.436), the rate washed out - so on her wheels
the integrator wound against her tyres' side force, a second integrator in
the lander's own heading law, and its clip and gains gave her rudders 0.15
radian of their 0.52 at full pedal. Ruled out on the way:
- **Nose-wheel steering**: no aircraft here has any. JSBSim steers a nose
  wheel only from `fcs/steer-cmd-norm`, which glideslope never sets, so the
  F-15C, F-35B and every airliner roll out on their rudders and the
  lander's steering brake alone (src/sim/lander.cpp, `toe_brakes`), as she
  does. Her own FCS schedules `fcs/steer-pos-deg` from that same zero; her
  80-degree `max_steer` is never reached. Set to 5, nothing changed.
- **Brakes**: none were on (the trace's brakes 0.00 to 0.00) through the
  swing; her main gear's spacing, damping and friction are the pinned
  model's and the F-15C's alike rest on the same law.
The fix, as her pitch loop's (tools/make_f22.py, `with_yaw_ground_mode`):
on any wheel the yaw integrator is washed out at YAW_GROUND_WASHOUT_PER_S
and her pedals drive her rudders in proportion, full pedal full travel -
the A320's ground mode, stick to surface, the source her pitch ground mode
already cites - fading back to the flight law over five seconds after her
wheels leave. She now stops 0.0 m across in calm air and 0.2 m in 15 kt
across from `land`'s gate. `the_f22a_landed_at_150_kt_rolls_out_on_the_centreline_in_calm_air_and_15_kt_across`
pins it (0.85 m and 5.66 m at most after the touch from 5 nm), seen red on
her model before the change: 117 m across in 15 kt. Seen red again
after the fourth round, with her yaw damper's 0.3 in place and only the
yaw ground mode taken out of her model: 212 m across in 15 kt across.
Verified, linux-release, `ctest -R "f22|selftest|takeoff_speeds|plan_speeds|lands|landed|lander|roll" -LE nightly -j4`,
no display: 129 tests, 127 passed and 2 live ones skipped, 665 s; the
script's committed output checked.
- **Not yet held**: in that test she rises 3.7 ft after her first touch at
  150 kt, so it holds her upright, not down.

- So no stall figure is written to `assets/figures/f22.xml`: written, it
  would give her an approach speed, and every landing test would fly her.

**No published approach or stall speed was found (searched again
2026-11-02).** The Accident Investigation Board reports found - Fallon 2018
(take-off data only, 143 and 164 KCAS), Elmendorf 2010, Edwards 2009,
Nellis 2004 - and the AETC release on the Tyndall 2012 mishap give none for
a landing; forum and simulator figures (140 to 150 kt) are not citable; a
"F-22A Flight Manual 1.0" on Scribd is an uploader's, not T.O. 1F-22A-1.

**What now works.** Her minimum steady flight speed in her landing
configuration - 14 CFR 1.2's VS0, "the stalling speed or the minimum steady
flight speed in the landing configuration" - is measured from her model:
90.0 KCAS at her combat loading, 54,960 lb, gear down and no flap, at 5,000
ft, where her stabilators reach their stop at 27.5 degrees of alpha. A figure
flight `stall_speed` given `steady="1"` (src/sim/figures.cpp,
`steady_flight_speed`) trims her level at 1 g from its entry speed down in
whole knots, each trim afresh, then halves to the tenth between the last
that trimmed and the first that did not. It is the same from 160, 200 and
240 KCAS.

Why not the handbook's deceleration, which every other stall here is
measured by: from 5,000 ft at idle her test pilot's speed loop, against her
rate-command stick, dives her 12 to 16 degrees nose down, her speed swings
three knots and the measurement ends there, at 9 to 12 degrees of alpha,
nowhere near her wing's limit: 131, 143 and 149 KCAS by its entry speed
alone, and the same from 25,000 ft (136 to 149) - 131 to 149 in all from 160 to 280 KCAS, where the 2026-09-23 attempt
(tests/unit/test_lander.cpp) found 128 to 149. Her lift table rises to its
peak at 45 degrees.

**Her trim's alpha range (tools/make_f22.py, `TRIM_ALPHA_MAX_DEG`).** The
model gave no `<alphalimits>`, so JSBSim's trim tried -5 to 20 degrees alone
and stopped below 114.75 KCAS, 17.8 degrees of alpha, her stabilators a third
of the way to their stop. The limits are now her lift table's range, -10 to
45; JSBSim reads them for its trim and a stall hysteresis she does not have.
Her published figures fly as before (supercruise 1.77, acceleration 52.7 s,
turn 7.35 deg/s, Mach 2.15, climb 8,854 ft/min at 50,000 ft).

**Verification.** `the_f22a_s_slowest_steady_flight_landing_configured_does_not_hang_on_its_entry_speed`
measures it from 160, 200 and 240 KCAS, each 90 within a tenth, coverage
three of three. Seen to fail: run against her model without the alpha range
it measured 114.75 and failed. `the_committed_f22_is_what_its_tuning_script_makes`
passes after the script was run. linux-release, `ctest -R
"f22|selftest|takeoff_speeds|plan_speeds" -LE nightly -j4`, no display: 35
tests, 34 passed and the live recording one skipped for want of
GLIDESLOPE_LIVE_MODEL, 64 s. The full suite was not run: the item is not
done, and the time box ran out.

**Her approach at 1.3 times VS0 (third round, 2026-11-02): not flown
within her limits by the approach laws as they are.** With her roll-out
fixed and VS0 = 90.0 put in a scratch copy of her figures, `land f22` from
the 2 nm gate at full tanks (63,960 lb) flies 123.8 kt at 17.3 degrees of
alpha and 14.3 of pitch, holding the glidepath at 660 ft/min on 56 per cent
throttle. What fails:
- **A lateral oscillation that grows down final**, about 9 s a cycle: in
  calm air from 0.3 degrees of bank at 400 ft to 9.8 at 60 ft, sideslip to
  6.3, and she touches 8.9 m off the line at 461 ft/min, her nose 12.2 up.
  In 15 kt across it grows from the first seconds - sideslip to 30 degrees
  by 490 ft - and she departs, 65 degrees of alpha, and strikes the ground
  2,219 m short at 2,508 ft/min.
- **Its cause is the lander's rudder law on her rate-command yaw loop.**
  The lander's approach rudder (src/sim/lander.cpp: sideslip, its integral
  and a body-axis yaw damper) commands her pedals, which in the air command
  a yaw rate through her flight controls' own integrator and washout - a
  yaw damper already. With the lander's rudder held at nothing (a
  diagnostic, not committed), she flies down level in calm air and touches
  0.9 m off the line; in 15 kt across she no longer departs, but touches
  83 m off it, crabbed and never decrabbed. The yaw damper on the
  stability axis instead (r cos alpha - p sin alpha) made it worse: 1,233
  ft/min in calm air. Her catalogue's `yaw-damper` is the autopilot's and
  the lander does not use it: 0.05 to 0.6 changed nothing.
- **The flare does not bring her inside 360 ft/min**: with the rudder held
  out she still touches at 383 ft/min, her nose 14.6 up. On final her nose
  is already 14.3 up, so the flare's pitch adds little; her figures' 200
  ft/min is not reached.
- At about 150 kt (VS0 of 115 put in) she flies the same laws well and
  touches at 130 ft/min, 1 m off the line.

So 1.3 times her measured VS0 cannot be flown within limits by the lander
as it is: it needs a rudder law that leaves a fly-by-wire yaw loop to damp
itself (and decrabs her), and a flare that works from 14 degrees of nose.
No other multiple is chosen: none is sourced.

**Her final at 1.3 times VS0, fixed; her flare, not (fourth round,
2026-11-02).**
- **The lander's yaw damper for pedals that command a yaw rate.** Neither
  the A320's figures nor the F-35B's say anything of fly-by-wire yaw - their
  models' pedals move their rudders - so a figures attribute is added:
  `yaw_rate_pedal_per_degps`, the pedal the approach yaw damper asks for each
  degree a second, 0.05 for every aircraft that does not give it, as before
  (src/sim/lander.cpp; read and refused outside 0 to 1, sim/figures.cpp).
  Hers is 0.3, a tuning value: with the yaw damper taken out entirely she
  sideslipped 4 degrees and drifted 107 m off in 15 kt across, and with the
  rate term on the stability axis she was worse; at 0.15 she touched 4.0 m
  off, at 0.3 within 0.5 m. Other aircraft fly the same arithmetic as before.
  `the_f22a_flies_her_final_at_1_3_times_her_measured_vs0_on_the_centreline_in_calm_air_and_15_kt_across`
  flies her at 123.8 kt from 5 nm in calm air and 15 kt either way: final
  within 0.49, 2.47 and 2.80 m, rolled out within 2.4 m, rising 0.08 to
  0.47 ft after the touch (under 0.75); seen red at 0.05 (she departed in
  calm air from 5 nm).
- **Not de-crabbed in the flare**: no aircraft here is; she touches crabbed
  and her roll-out (now held) straightens her. Not done.
- **Her flare does not bring her under 360 ft/min: 392 to 417 ft/min** at
  123.8 kt (374 to 383 from `land`'s 2 nm gate). The flare's nose limit is
  her tail strike less two degrees, 14.54, and her final is flown at 14.3:
  a quarter of a degree to flare with. The flare's power cushion, from 300
  ft/min past the flare's sink, comes in and goes off within her 1.8 s
  spool: tried from 150 (390 ft/min) and 50 (648, floated then dropped), and
  a flare begun higher, 45 ft (549) and 60 (320 calm, 512 across). None is
  kept. No F-22 or delta-wing source for a power-on, attitude-held
  touchdown figure was found in this round. So 1.3 times her measured VS0
  is not landed within 360 ft/min, by 30 to 60 ft/min.
- **The 3.7 ft rise at 150 kt** is that speed's: 21 per cent over 1.3 times
  VS0, her wing still lifts her at the touch (3.3 to 3.7 ft). At her own
  speed she rises under half a foot; the 150 kt test is held upright, not
  down, and the 1.3 times test pins the rise.

**Full suite, 2026-11-02** (`ctest --preset linux-release -LE nightly -j4`,
no display): 1,430 tests, all passed or skipped (25, for want of a key, a
display or a live model), 4,639 s. Every other aircraft's landing tests
pass unchanged, and the selftest's hash tests with them.

**From the review of #296**: comments and names only - the make script's
docstring gives the yaw ground mode, its blend is YAW_GROUND_BLEND_S and its
full pedal YAW_CMD_FULL; the script rerun writes her model byte for byte as
before. `ctest -R "f22|selftest" -LE nightly -j4`: 34 tests, 33 passed and
one live one skipped.

**Left for the next round**: a flare for her at 14 degrees of nose (a sourced
figure), then her landing distance by `tools/measured_landing.py` and her
stall figure, which would give every landing test her.

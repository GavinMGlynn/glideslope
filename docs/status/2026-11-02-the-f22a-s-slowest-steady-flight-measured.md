### The F-22A's slowest steady flight measured from her model; at 1.3 times it she does not land - 2026-11-02

**What is still not true first.** The "runway too short" item stays open. The
F-22A still has no approach speed, no landing figure, and every landing is
still refused her:

- **At 1.3 times her measured slowest steady flight she does not land.**
  Given it as her landing stall in a scratch copy of her figures (90.0 KCAS
  at 54,960 lb, so 117 kt there and 124 kt at her model's 61,570 lb), `land
  f22` in calm air touched at 461 ft/min, 8.9 m off the centreline (her
  limit is 360) and ran off the side of the runway, 135 m across. In 15 kt
  across she struck the ground 2,219 m short of the threshold at 2,508
  ft/min.
- **Her roll-out leaves the runway at any approach speed.** At about 150 kt
  (a stall figure of 115 put in to try it) she flies the approach well and
  touches at 130 ft/min 1 m off the line, then the roll-out's rudder drives
  a yaw swing that grows from under 1 to 18 degrees a second with the
  rudder at its stops, and she leaves the runway 196 m across in calm air
  and 395 m across in 15 kt across. Her nose wheel's steering read 0 degrees
  through the roll (`fcs/steer-pos-deg[0]`); setting her nose wheel's
  `max_steer` from 80 to the other fighters' 5 did not change it. Not found.
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
alone, and the same from 25,000 ft (136 to 149). Her lift table rises to its
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

**Left for the next round**: her roll-out's yaw swing on her wheels, then her
approach at 1.3 times 90 (about 17 degrees of alpha), then her landing in
wind and crosswind, then her landing distance by `tools/measured_landing.py`.

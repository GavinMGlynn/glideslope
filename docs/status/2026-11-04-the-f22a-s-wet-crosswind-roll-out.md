### The F-22A's wet crosswind roll-out comes off her names: her own roll-out rudder gains, 3.85 m wet against 6.13 - 2026-11-04

**Second round.** It supersedes the first round's "still named" below.

**What is still not true.** The item ("The F-22A flies her stall-figure
tests") stays open: her gust landings are still named, on forty seeds, as in
2026-11-04-the-f22a-s-gear-down-alpha-limit-tried.

**Why a stronger track rudder banked her.** Traced every eighth of a second
in `an_approach_taken_back_at_the_touch_is_landed_to_a_stop` (calm air,
taken back at 144.5 kt):
- It is not her roll-out stick, and it begins in yaw, not in roll. Her
  aileron stays within 0.15 until her heading swings past 10 degrees, and
  her bank follows her yaw.
- Even on main's gains, while her nose is still up after the touch, her
  rudder chatters from -0.4 to +0.3 and back each quarter second, her yaw
  rate changing sign each time. It dies out once her nose wheel is down.
- On her wheels her pedals drive her rudders directly, through an actuator
  limited to 2.67 of its travel a second (tools/make_f22.py's yaw ground
  mode). The roll-out's yaw-rate damping, 0.5 of rudder a degree a second,
  drives that rudder from stop to stop at about two cycles a second, and
  the actuator lags it.
- At 0.8 a degree of track, the oscillation grows: the rudder at its stops
  each half second, her yaw rate 16 to 27 degrees a second. Her heading
  swings 20 degrees either way, she is 25 m off the line, and she banks 16
  degrees.

**The fix: half the damping, and her own track rudder.** Her figures'
`roll_out_yaw_damping_share` is 0.5, and `roll_out_track_rudder_per_deg`
is 0.8 (both read by sim::Lander). Both are **tuned, not sourced**. Every
other aircraft flies all of the damping and 0.5, as before.

Measured on linux-release, her lines only:

| track | damping | wet, m | dry, m | bank at the touch | landed by hand |
|---|---|---|---|---|---|
| 0.5 | all (main) | 6.13 | 2.35 | 0.0 | 0.0 |
| 0.8 | all | 4.75 | 1.85 | 15.6, fails | 0.0 |
| 0.5 | half | 5.09 | 1.94 | 0.0 | 0.0 |
| 0.7 | half | 4.20 | 1.61 | 0.1 | 0.0 |
| **0.8** | **half (written)** | **3.85** | **1.48** | **0.1** | **0.0** |
| 1.0 | half | 3.29 | 1.28 | 0.1 | 0.0 |
| 1.2 | half | 2.85 | 1.11 | 0.0 | 0.0 |
| 1.3 | half | 2.67 | 1.05 | 16.8, wrecked | 0.0 |
| 1.5 | half | 2.36 | 0.93 | 12.0 | 0.0 |
| 0.8 | 0.3 | 3.55 | 1.34 | 0.0 | 0.0 |
| 0.8 | 0.4 | 3.69 | 1.41 | 0.1 | 0.0 |
| 0.8 | 0.6 | 4.03 | 1.56 | 0.1 | 0.0 |
| 0.8 | 0.75 | 4.30 | 1.67 | 0.1 | 0.0 |
| 1.0 | 0.3 to 0.6 | 3.03 to 3.44 | | 0.1 at most | 0.0 |
| 1.2 | 0.3 to 0.4 | 2.64 to 2.73 | | 0.2 at most | 0.1 at most |
| 1.5 | 0.4 | 2.26 | | 17.0 | 13.9 |

So with half the damping, the track rudder works from 0.5 to 1.2 and
weaves from 1.3. 0.8 is written: inside that band, and 1.15 m inside the
5 m. Half is written for the damping: 0.3 to 0.75 all work at 0.8.

**Every other aircraft is unchanged.** In the crosswind roll-out test,
linux-release, before and after, every line but hers is the same to the
hundredth: 737-300 3.08 m wet, 747-400 2.92, 787-8 2.47, A320 4.04, A380
2.03, B-2A 0.84, C172P 0.77, C182 0.48, F-15C 3.04, F-35B 0.98, J-3 Cub 1.63,
Learjet 35A 1.10, Mosquito 3.36, PA-28 2.02, and the tail-wheel slopes.
Their code multiplies by the same 0.5 and 1 as before.

**Verification (second round).**
- The test passes with her name off: 3.85 m wet, 1.48 dry.
- Seen red: with both attributes taken out of the build's copy of her
  figures, she went 6.13 m and the test failed, "f22 went 6.128057 m from
  the centreline rolling out wet". The copy was restored.
- 479 tests on linux-release (with FMA), no display: every test matching
  the roll-out, landing, wet runway, crosswind, braking, gust, approach,
  touch, take-back, copilot, recording, selftest and committed-file tests.
  All pass, her gust names among them, and the live recording tests are
  skipped. Her measured landing still passes.
- In an x86-64-v2 build (no FMA, as CI builds) her lines are the same:
  3.85 m wet, 1.48 dry, bank 0.1 at the touch, 0.0 landed by hand. The 373
  roll-out, landing, wet, crosswind, braking, take-back and touch tests of
  its test program pass. The 28 that need the other programs were not
  built in it, and failed at once for that.
- Windows was not built: the change is C++ with no platform code.
- `python3 tools/make_f22.py --check` passes. Her model is not changed.

---

### First round: traced, and still named at 6.13 m

**What is still not true.** The item ("The F-22A flies her stall-figure
tests") stays open. Her wet crosswind roll-out is still named, at 6.13 m
from the centreline wet (2.35 dry), as on main. Her gust landings are still
named too. No model, code or figure is changed. Only this note, the test's
comment and the plan's text are.

**What the test means.** `every_landplane_rolled_out_dry_or_wet_as_15_kt_of_crosswind_arrives_stays_within_5_m_of_the_centreline`
hands each landplane to the landing's roll-out (`Lander::on_its_roll`) and
holds her to a stop within 5 m of the centreline. So the AI is to stop her,
not take off.

**Why she goes 6.13 m.** Traced on linux-release, every quarter second of
her wet roll-out:
- handed over at 133.6 kt, her nose 4 degrees up from her take-off's
  rotation, her nose wheel off the runway for 3.0 s;
- as 15 kt arrives across, her sideslip is 6.4 degrees and her side force
  is 11,000 lb, while her wheels bear 15,000 lb of her 61,400. Her model's
  CYb is -1.79 a radian (the pinned JSBSim f22's);
- the F-15C, handed over in the same way with her nose 5.7 degrees up, has
  2,100 lb of side force against 16,300 on her wheels. Her rudder comes in
  at once, about -0.5, and holds her heading;
- the F-22A's rudder stays under 0.1. The roll-out steers by her track, at
  0.5 of rudder for each degree of track error, and that turns her into the
  wind too slowly. She slides across at up to 22.8 ft/s, and her tyres'
  side force takes 5 s to build to 8,000 lb.

**Lowering her nose sooner is not it.** Tried for her alone through
`lower_the_nose`:

| nose lowered in | to | wet, m | dry, m |
|---|---|---|---|
| 3 s (as on main) | -1 deg | 6.13 | 2.35 |
| 1 s | -1 deg | 5.99 | 2.29 |
| 0.5 s | -1 deg | 6.14 | 2.28 |
| 3 s | -5 deg | 5.67 | 2.10 |
| 0.3 s | -10 deg | 5.27 | 1.83 |

Her nose wheel is down sooner, but the side force is already acting on her.

**A track rudder of her own clears this test, and breaks another.** Tried
as a figure of hers, tuned, not sourced. Every other aircraft kept the
lander's 0.5, and in this test their lines were the same to the hundredth.
- In this test, wet and dry:

  | her gain | wet, m | dry, m |
  |---|---|---|
  | 0.5 (main) | 6.13 | 2.35 |
  | 0.7 | 5.14 | 1.99 |
  | 0.8 | 4.75 | 1.85 |
  | 1.0 | 4.11 | 1.62 |
  | 1.5 | 3.06 | 1.24 |
  | 2 | 2.42 | 1.00 |
  | 3 | 1.69 | 0.73 |
  | 5 | 1.03 | 1.64 |
  | 10 | 20.01 | struck her airframe |

- In `an_approach_taken_back_at_the_touch_is_landed_to_a_stop`, in calm
  air at 145 kt:

  | her gain | bank after touching |
  |---|---|
  | 0.5 (main) | 0.0 |
  | 0.8 | 15.6, fails |
  | 1.0 | 1.9 |
  | 1.2 | 1.8 |
  | 1.3 | 3.0 |
  | 1.5 | 16.9, struck her airframe |
  | 2 | 16.8, struck her airframe |

  At 2, `an_aeroplane_landed_by_hand_and_handed_over_on_its_roll_is_landed_to_a_stop`
  failed the same way.
- The bank does not follow the gain. 0.8 fails and 1.0 to 1.3 pass, so
  passing at 1.0 would be luck, and a platform's drift could undo it. It
  was not committed.
- A stronger gain only while her nose wheel is off the runway was tried
  too: 4 gives 3.79 m wet, 8 gives 4.19, and her bank at the touch is 2.9
  to 5.0 degrees, against none.

**What is left, for the next round.** At 145 kt on her wheels, any extra
rudder rocks her wings. That fragility comes first; a stronger track rudder
can follow it. The bank was not traced in this round.

**Sources searched (2026-11-04).** For an F-22 crosswind limit, her
wet-runway ground handling and her rejected take-off. Found only:
- Air Force photo captions of Raptor 4001's wet-runway tests at 30, 60 and
  90 kt, with no figures;
- general rejected take-off guidance (SKYbrary; the FAA's Airplane Flying
  Handbook as relayed by Boldmethod), nothing of hers.

So every gain above is tuned, not sourced.

**Verification.**
- On main's code, the test passes with her name on: 6.13 m wet, 2.35 dry,
  on linux-release (with FMA) and in an x86-64-v2 build: see below.
- The sweeps were run from the test binary, her lines read, with each gain
  set in the build's copy of her figures. The copy was restored after each
  sweep.
- `python3 tools/make_f22.py --check` passes. Her model is not changed.

**From the review of #321.**
- The lander's comment now points to this file. It also says that at 0.8 of
  track, with all the damping, she banked 15.6 degrees and failed, and that
  at 1.5 and 2 she struck her airframe.
- `her_own_roll_out_rudder_gains_are_read_and_refused_at_0_or_past_their_range`
  (tests/unit/test_figures.cpp) reads her two gains back. It also refuses 0
  and a value past each one's range: 2.5 for the track rudder, 1.5 for the
  share. Four refusals, asserted.
- Seen red: with the track rudder's limit widened to 5, it failed, "3 of
  4". Restored, it passes.
- Linux-release: the 211 figure, refusal, roll-out, take-back, wet and
  crosswind tests all pass.

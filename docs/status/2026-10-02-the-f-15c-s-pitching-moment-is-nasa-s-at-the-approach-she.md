### The F-15C's pitching moment is NASA's at the approach: she flies it at 10 degrees with stabilator to spare, 2026-10-02 — item not done

**What is still missing, first.**
- **Full aft stick does not meet the manual's speed.** T.O. 1F-15A-1,
  section VI: "With full aft stick, AOA stabilizes at 45 units or above with
  airspeed 100 knots or less". Her nose now settles - at 42.4 degrees - but
  at 116.6 knots. The model's lift and drag would give 100 knots only
  settled past 60 degrees, beyond the end of their tables at 50; the
  manual's units are not degrees, and it gives no conversion.
- **Her high-alpha pitching moment is not published.** No F-15 pitching
  moment past the approach was found; past 24 degrees the pinned model's
  table is kept at 0.45 of itself, chosen (below), not sourced.
- **No published stall speed exists**, so the plan item's "stalls near its
  published speed" needed the owner's word. Her stall is measured on the
  model, 112.26 knots, where its lift table peaks. The owner ruled on
  2026-10-02 that she is held to the manual's described stall instead (the
  log entry of that day, above).
- One new named exception: recovered at the stall warning she loses 590 ft
  against the fighter lesson's 500.

**Why.** On #79's 160-knot approach her stabilator sat near its stop and her
nose crept up, and #77's flare wrecked her in the circuit at 1,042 ft/min.
NASA TM-4604 (Corda, Stephenson, Burcham and Curry, 1994, page 8) gives the
NASA Dryden F-15 simulator's derivatives at the approach, 8 degrees of alpha
and -5 of stabilator, each a degree: CL-alpha 0.065, CL-delta 0.005,
Cm-alpha -0.0021, Cm-delta printed -0.00072.

**The change** (`tools/make_f15c.py`, `with_nasa_pitch`, its docstring
entry):
- **Cm-alpha NASA's at the approach.** The table is scaled, its shape kept,
  until its slope across 6 to 10 degrees - not at 8, which sits on the lift
  table's 0.14-radian breakpoint - about the **clean loading's** centre of
  gravity is NASA's. That centre of gravity is worked out from the figures
  file and the model: the empty aeroplane's, with the stores and fuel at it,
  and the 230-lb pilot 187 in forward, which brings it 1.17 in forward, to
  3.41 in ahead of the aerodynamic reference point (it was taken about the
  empty aeroplane's, 2.24 in, before the review). The model's slope there
  was -0.0100 a degree, about five times NASA's; the scale is 0.118. The
  model's lift slope across the same range is 0.058 a degree against NASA's
  0.065 (0.067 below the breakpoint and 0.049 above it).
- **Past 24 degrees the scale is 0.45, not 0.118.** Scaled whole, the table
  left full aft stick nothing to balance against at high alpha: about a
  degree of stabilator at 8 degrees and a flat 2 degrees from 22 to 48, and
  the nose went on rising to 80 degrees in the stall flight - the review's
  finding. 0.45 is about the most of the pinned table's nose-down moment
  that still lets full aft stick carry the nose past the lift's peak at 32
  degrees, as the manual's goes on past its wing rock at 30 units: at 0.468
  or more full aft stick balances below 24 degrees, and the script works
  that bound out and refuses a scale over it. The pinned table's slight
  pitch-up between 24 and 32 degrees is kept.
- **Cm-delta -0.0072, NASA's with the zero it dropped.** The two stabilator
  derivatives' ratio is the tail's arm in chords: -0.00072 / 0.005 puts the
  stabilator 2.3 ft behind the centre of gravity, where the model's
  horizontal tail arm is 20 ft; -0.0072 puts it 23 ft. The model's was
  -0.008.
- **CL-delta 0.005**, NASA's, where the model's was 0.010.

**Before and after.**

| | Main (#79) | This branch |
|---|---|---|
| Approach at the manual's 160 kt, 36,946 lb: alpha | 10.5 to 18.3° | 9.9 to 11.5° (10.8 at 50 ft) |
| Nose-up stabilator used on it | up to 0.98 | at most 0.14 |
| Full aft stick, 200 kt at 10,000 ft, idle | stabilator stop at 18.3°, 151 kt | settles at 42.4°, 116.6 kt, sinking 7,700 ft/min (manual: 45 units, 100 kt or less) |
| Stall (measured) | 151.06 kt, lowest speed | 112.26 kt, first CL maximum, at the lift table's 32° peak |
| Nose wheel off, full aft, military (A3-6: 91.5 / 100.1 / 110.7) | 98 / 103 / 107 | 96.6 / 100.9 / 105.0 |
| Lift-off speed / where she left | 1.15 × stall, 173.7 / 178.2 | 154.4 worked from A3-6 / 159.9 |
| The six published figures | in range | in range: Mach 2.40, 15,469 and 56,925 ft/min, ceilings 47,627 and 58,232 ft, 7.80 °/s |
| Stall recovery at the warning | 281 ft of 500, 1.70 g | 590 ft of 500, named; 1.64 g |
| Left thirty seconds in a stall | 958 of 2,952 ft, 1.89 g, named for load | 1,493 of 3,877 ft, 1.65 g, the name off |

**What had to be measured or pinned again, and how.**
- **The stall.** In the old stall flight the speed went on falling and the
  nose on rising, and "the lowest speed before it gathers three knots"
  meant nothing. 14 CFR 25.103(b) defines a stall speed for an aeroplane
  whose lift never breaks: the speed at which the lift coefficient is first
  a maximum. `stall_speed` measures that where a figure says `clmax="1"`,
  from the wind-axis lift (the body's normal force at sixty degrees is half
  drag). For the F-15C that maximum is where JSBSim's f15 lift table peaks,
  32 degrees - the table's, not a published one. From 160, 200 and 240
  knots: 112.13, 112.26, 112.15.
- **The take-off.** At 1.15 times a stall at 32 degrees she would rotate at
  129 knots and cannot leave the ground there below her tail strike. Figure
  A3-6 gives the normal take-off speed at military thrust, 141.4 at 36,946
  lb, with full flaps; figure A8-1's ratio of flaps-up to flaps-down
  approach speed at that weight, 160.5 to 147.0, makes it 154.4 flaps up,
  which the model is. A figures file may give that as `<takeoff kcas
  loading>`; `departure_speeds` takes off at it, after a flying boat's water
  take-off and before the rest, and says it is **not** published - it is
  worked out (the review). She leaves 4.6 to 6.0 knots past it at every
  loading.
- **The fighter take-off lesson's climbing attitude band starts at 3
  degrees, not 5.** Her least attitude in the initial climb by the book is
  now 7.0 (it was 11.4), and on part throttle, leaving later at 165 knots,
  5.0 - which broke the "names that fault and no other" check with "Hold
  the climbing attitude". The lesson's note gives the measured figures.
- **The climbing speed is kept at 200 knots by decision.** The floor under
  the climb rule chose it when her stall was 151; the floor is now 146, and
  a climb taught at 146 knots is not one the manual flies. The rate at 200,
  24,556 ft/min, is unchanged. Said so in the figure and in ASSETS.md.
- The fighter stall lesson's note: its warning, stall + 8, was 5% of 151.

**Verified** (linux-release):
- New:
  - `the_f15c_flies_its_approach_near_nasas_angle_of_attack_with_stabilator_to_spare`:
    8 to 13 degrees down the whole approach stage, under a third of the
    nose-up travel.
  - `the_f15c_held_at_full_aft_stick_settles_past_the_peak_of_her_lift`:
    over the last 20 of 90 s at full aft stick, alpha within 4 degrees and
    past 32; the speed against the manual's 100 knots is printed, not held.
  - `the_f15c_lifts_its_nose_wheel_near_its_flight_manuals_speed_at_each_weight`:
    within a tenth of the manual's at each loading, and it counts the
    loadings whose nose wheel came off. A tenth, not closer, because the
    manual's speed rises 19 knots across the three where the root of the
    weight gives 10 - the real aeroplane's centre of gravity moves with its
    fuel and stores, the model's carries both at its own - so no single
    centre of gravity meets all three. (The review found the earlier
    tolerance had 0.37 knots to spare: fitted to the result.)
- **Seen red**:
  - With the model made without `with_nasa_pitch`, the approach test failed
    at 10.3 to 18.4 degrees, the nose wheel came off at 97.9 at the clean
    loading, and the measured stall moved.
  - With the high-alpha scale set to the approach's, the full-aft-stick
    test failed: alpha went through every value from -180 to 180 - she
    tumbled. A scale over the bound is refused by the script.
  - The nose-wheel test was red at 96.6 under the old five-knot tolerance.
  - The take-off checks were red at 1.15 times the new stall.
- 194 tests across take-off, figures, lessons, approach, landing, learnt
  landing, departure, stalls, circuits, the autopilot and plans: all pass
  but one. That one, `a_player_takes_over_an_ai_aircraft_with_no_step_at_100_ms_and_a_players_is_refused`,
  is a multi-process test run at -j4 alongside the flights; on its own it
  passes. The committed model matches its script. The selftest hash (C172P)
  is unchanged, `d36123c1eecc3e23`.
- **#70's orbit test on top** (`orbits-flown-on-their-circle` at 4bcdfd9,
  merged on a scratch branch, not committed):
  `the_tightest_orbit_at_the_approach_speed_is_flown_on_its_circle_by_every_fighter_and_bomber_that_can_fly_it`
  passes, and the F-15C, which on main's model lost 457 ft round it at 160
  knots, holds her height within 2 ft in all four orbits, left and right, calm
  and a 10-knot wind, 2.00 turns each, -3 to +101 m off the circle.
- **With #77 on top** (origin/the-ai-flares-to-its-wheels at 3522308, on a
  scratch branch, not committed; measured on the first version of this
  change, before the review): her circuit landing touched at 427 ft/min,
  not 1,042, and was not wrecked. **But she rose 2.3 ft after her wheels met
  the runway** in the approach and circuit lessons, and 3.1 ft taken back
  at the touch: three of #77's after-touch checks are red for her. That is
  #77's flare and her gear, not her pitch.

### The F-22A, the Mosquito and the S.23 trimmed, and the C182 with her propeller governed, 2026-10-22

**What is still not true first.** The F-22A is trimmed in pitch alone
(the owner's decision, 2026-10-22): her roll and yaw are rate commands
through integrators, which no aileron or rudder moves while JSBSim trims
with time stopped, so a lateral trim cannot succeed on her. She has no
propeller's torque to hold off; untouched she banks 0.01 degrees at most.
She is named in the catalogue test, which checks her wings within 1 degree
of level with her height. Built on #233 (a player's first input keeps the trim the
server held), whose `WATCH` now carries her held trim too.

**Why each could not be trimmed, and what changed.**

- **The F-22A** (tools/make_f22.py). Three things in her flight-control
  system did not move while JSBSim trims, with time stopped:
  - her pitch trim is added to the stick, and so commands a pitch rate,
    which her pitch loop's integrator turns into elevator only over time.
    The pilot's trim is left so (the owner's decision: she keeps her
    rate-command feel). A second input, `fcs/pitch-trim-held-norm`, is
    added to the elevators' command after the loop: the deflection the
    server holds her at until her player's first input. JSBSim's trim
    cannot move it - its pitch-trim axis is the pilot's - so glideslope
    finds it (src/sim/aircraft.cpp): at each held trim tried JSBSim trims
    her angle of attack and power (a custom trim), and the held trim that
    leaves her no pitching acceleration, to JSBSim's tolerance of 1e-4
    rad/s^2, is found by the Illinois method between -1 and 1. Trimmed
    level at 300 KCAS it is 0.069, with the integrator at nought;
  - her throttles went through one-second lag filters, so the trim's
    throttle never reached her engines (thrust at 0, 0.5 and full throttle
    alike: 338 lb). Each is now an actuator with the same lag, which JSBSim
    passes straight through while trimming and which lags the same in
    flight;
  - her leading-edge flaps follow her angle of attack through a lead
    filter, (2s + 7.25)/(s + 7.25), which lagged each angle the trim tried:
    her lift hung on the angles tried before and the trim never settled
    (wdot stuck at 2.8e-3 against a tolerance of 1e-3, 61 iterations). It
    is written as twice the angle less its lag, the lag an actuator - the
    same filter under the bilinear transform JSBSim uses for both.
- **The S.23** (tools/make_short_s23.py). Each Pegasus is fed from a
  quarter-pound float chamber, which the model started empty and its fuel
  system fills once she flies; started running she was starved, and no
  throttle made power in the trim ("udot doesn't appear to be trimmable").
  The chambers start full, a pound in all, the pound taken from her two
  centre tanks so that she weighs what she did (CI caught her 45,000 lb
  take-off test at 45,001).
- **The C182 and the Mosquito** (src/sim/aircraft.cpp). JSBSim's trim
  finds the throttle by running each engine to a steady state half a second
  at a time, which a constant-speed propeller's governor cannot follow, and
  its default propeller lever is the lowest rpm. The C182's governor drove
  her blades to full coarse, trimming her at 2,015 rpm, which her governor
  took to 2,400 in flight; the Mosquito's Merlins overran and stopped. Now
  a trimmed start sets the propeller lever where every pilot's `Controls`
  leaves it (1, the highest rpm), and an aircraft with a governed
  propeller has her throttle found by glideslope: at each throttle her
  engines are started again and run alone at the simulation's step until
  their rpm and blades settle, JSBSim trims the rest (a custom trim: angle
  of attack and pitch trim, and laterally bank, aileron and rudder) with
  the engines held as they settled, and the two repeat until the blades
  stay put; the throttle is found by the Illinois method between closed and
  open, to JSBSim's own tolerance on udot. The C182 trims at 2,400 rpm,
  blades 23.08 degrees, throttle 0.789; the Mosquito at 1,260 propeller
  rpm (3,000 engine), blades 31.68 degrees, throttle 0.715.

**Her held trim told her client** (on #233, protocol `0F` still). The
held trim is no control: no input carries it. Her `WATCH` carries it after
the yaw trim, an `i16`, or `-32768` where her model has none
(docs/TRANSPORT.md), and a predicting client puts its prediction's at what
it is told as it joins (`sim::Prediction::held_by_server`, both clients).
Untold, her prediction is still within the first-input test's bounds -
her pitch loop's integral winds up to take the held trim's place (0.0095
at most, against 0.0011 told) - so the client also says the held trim it
put and that integral, and the test checks both.

**The Mosquito's roll and yaw trim** (tools/make_mosquito_fb6.py). #233
holds the aileron and rudder a trim found as roll and yaw trim, and her
model read neither; untrimmable then, she was named and held on nought.
Trimmed laterally now, her model sums each with its control, as her pitch
trim always was, and #233's test names only the F-22A (no yaw trim; her
roll trim commands a roll rate, and she is trimmed in pitch alone).

**Verification.**

- `every_aircraft_a_player_is_given_and_leaves_untouched_holds_her_start_height`
  (tests/unit/test_catalogue.cpp) leaves no aircraft out: all 16, started
  at 3,000 ft at their catalogue speed as the server starts a player's and
  held thirty seconds on `held_until_moved`, are trimmed - laterally, but
  for the F-22A, named, in pitch alone with her wings within 1 degree of
  level - and within 20 ft, the count asserted (16 of 16). The bound was
  100 ft; the worst trimmed is 7 ft, and main's C182 climbed 95, inside the
  old bound, so it is 20 ft: three times the worst, a fifth of hers. linux-debug: F-22A 3,000 to 3,006 ft (pitch
  alone), Mosquito 3,000 to 3,003, S.23 3,000 to 3,004, C182 3,000 to 3,000;
  every other as before, within 7 ft. Before: F-22A 2,496 to 3,000, Mosquito
  1,476 to 4,794, S.23 climbing 340, C182 2,999 to 3,095.
- Seen red per aircraft: with main's F-22A and S.23 models in the build's
  data, the F-22A is not trimmed and falls to 2,496 ft and the S.23 is not
  trimmed and climbs to 3,344, both named; with the governed-throttle search
  switched off (JSBSim's own trim, the lever at 1), the C182 is not trimmed
  (2,524 to 3,510 ft) and the Mosquito not (1,476 to 4,794), both named; and
  with main's behaviour for the C182 - the lever at JSBSim's lowest-rpm
  default, JSBSim's own trim - she climbs to 3,095 ft and fails the 20 ft
  bound, named.
- `the_f22as_pilots_pitch_trim_commands_a_pitch_rate_and_the_server_holds_her_on_its_own_trim`
  (tests/unit/test_catalogue.cpp): trimmed level at 300 KCAS, untouched she
  is commanded no pitch rate; a pilot's nose-up trim of 0.2 commands 0.169
  rad/s; the held trim is 0.0692 either way. Seen red with the pilot's trim
  routed to the held trim's place after the loop: no pitch rate commanded.
- `an_approach_twenty_knots_fast_at_the_gate_goes_around_and_lands` named the
  Mosquito as going around for being slow, because she could not be trimmed
  20 kt fast; trimmed now, she goes around 15 kt fast at 499 ft like the
  rest, and the exception is gone. The tests naming the Mosquito, the C182,
  go-arounds, on-final starts and every aircraft (99) pass in linux-debug;
  one window-client timing test that failed under that load passes alone.
- `a_players_f22a_untouched_is_predicted_on_the_held_trim_the_server_tells_her_client_and_holds_height_and_attitude`
  (tests/cmake/server_first_input.cmake, `-DAIRCRAFT=f22 -DTOUCHES=untouched`):
  through a real server and a 100 ms relay, a predicting F-22A that touches
  nothing: 1,524.0 m first heard, 1,524.0 to 1,526.1 m; pitch 3.33 to 3.38
  degrees, bank 0.01; prediction median, 95th percentile and worst 0.000 m
  over 707 updates; her prediction's held trim 0.0693 as told, its pitch
  loop's integral 0.0011 at most (bound 0.004). Seen red with the held trim
  untold: her prediction's held trim 0.0000, told 0.0693, the integral
  0.0095. It runs alone (RUN_SERIAL): on CI's Ubuntu debug, sharing its
  shard's CPUs, one update was put 30.3 m off, the starved machine of the
  open item "A predicting F-35B on a starved machine is put tens of metres
  off".
- `a_state_packet_carries_the_controls_of_the_aircraft_its_client_watches`:
  the held trim read back to the wire's fraction, and none as none.
- `glideslope_cli selftest`: hash 831a4e7954233b0a in linux-debug, the same
  with and without this change (the selftest trims nothing).

### A failed trim said, and the trim walked over loadings, heights and speeds, 2026-10-23

**What is still not true first.** The item is open again: the S.23 is
trimmed at 3,000 ft at her start speed, and not at most other heights -
at 130 kt she is trimmed at 5,000 ft at her model's own loading and at
1,000 ft at the long-range, and nowhere else of the nine (at the standard
loading at 1,000 ft in pitch alone). Not investigated yet. JSBSim's own
note on a failed trim (FGTrim.h) is to check whether the model can fly
steadily at those conditions - speed, altitude, configuration, weight.

**Said, not silent** (the review of #234). A trim that failed fell back to
`untrimmed()` and nothing said so. `Aircraft::trim_fallback()` now gives,
where any trim asked for failed, her model, height, speed, weight, flight
path and flaps, each trim that failed and JSBSim's or glideslope's reason,
and what she was left as (pitch alone, or untrimmed); the server prints it
when it gives a player her aircraft (`slot N's ID was not trimmed as
asked: ...`), and `glideslope_cli` where it trims an approach. The F-22A's
is said at every join: her full trim fails, by design, and she is trimmed
in pitch alone.

**Verification.**

- `every_aircraft_at_every_loading_at_three_heights_and_three_speeds_is_trimmed_as_a_players_is`
  (tests/unit/test_catalogue.cpp): every aircraft, her model's own loading
  and every loading her figures name, at 1,000, 5,000 and 10,000 ft, at the
  plan's slowest for her weight, her start speed and the plan's fastest:
  540 points, walked and counted. 451 trimmed laterally saying no fallback
  (the F-22A's 18 in pitch alone, said); 89 left out by name in the test,
  68 of them not trimmed and every one said: the S.23 everywhere; the
  737-300 and Learjet 35A at the plan's slowest; the Learjet at the plan's
  fastest at her model's own loading; the C172P, PA-28, J-3 Cub and C182 at
  10,000 ft, and the C182 at 5,000 ft at her fastest. Those are the new
  open item "Trimmed at every height, speed and loading a player or plan
  may start at" and this item's missing S.23. Before the exceptions were
  named the walk was red on exactly those 68. Seen red with the fallback
  never recorded: the F-22A's 18 and the 68 "not trimmed, and not said".
  2 min 37 s in linux-debug.
- Rebased onto #233's tip (fac4d3f0): the F-22A's first-input test moved
  to port 24876 (24812 was taken by #233's STOVL test);
  `every_fixed_test_port_lies_outside_the_ephemeral_ranges_and_no_two_tests_share_one`
  passes, as do the catalogue, first-input, untouched and F-22A tests (36,
  linux-debug). `glideslope_cli selftest` hash 831a4e7954233b0a, unmoved.

### Every point that does not trim, triaged, 2026-10-23

**What is still not true first.** Four defects found and open: the S.23,
the Learjet at her fastest, the light aircraft trimmed full rich at height,
and the J-3 Cub at 10,000 ft unsourced. Fixed: the 737-300 and Learjet at
the plan's slowest. Each point below is excluded with its source, fixed, or
open; the walk now trims 478 of 540 and leaves out 62 by name (47 not
trimmed, every one said).

- **The 737-300 and Learjet 35A at the plan's slowest, clean - fixed.**
  JSBSim's trim searches angles of attack between the model's
  `<alphalimits>`, -5 to 20 degrees where it gives none (FGTrimAxis.cpp),
  and checks for a change of sign at the ends alone (FGTrim::checkLimits).
  Both wings stall below 20 degrees, so at 20 her lift is less again than
  at the start and the trim said "wdot doesn't appear to be trimmable"
  (737 at 172 kt, 5,000 ft: wdot +39.2 at -5 degrees, +3.06 at 20). Both
  models now give alpha limits up to the greatest angle any of their flap
  settings reaches before it stalls - the 737's leading-edge stall, 0.30
  rad; the Learjet's flaps-8 peak, 0.2617 - through tools/make_737_300.py
  and tools/make_learjet35a.py. Nothing else reads them (JSBSim's
  `aero/alpha-max-rad` and its impending stall only). Seen red without them
  (21 points), green with.
- **The S.23 - open, a defect.** 130 kt at 1,000 ft is well inside her
  envelope. Her variable-datum boost control
  (aircraft/short_s23/Systems/engines.xml) is an `<integrator>` in her
  flight controls; JSBSim runs the flight controls, integrators and all, at
  every pass of its trim (FGPID::Run, the channel's own step), so the
  throttle the trim tries moves on beneath it: udot cycles through -1.9e-3,
  3.3e-2 and 1.1e-2 with throttle 0.55 and never settles (61 iterations).
  The fix belongs in tools/make_short_s23.py or in glideslope's trim (settle
  the boost loop as the governed propeller's is settled); not done.
- **The Learjet at 300 kt at her model's own loading - open, a defect.**
  Under her 350 kt limit (TCDS A10CE). At either stop her stabilizer leaves
  her pitching nose up (qdot 7.28 and 0.034 rad/s^2); at the landing
  loading she trims at 300 kt. Her centre of gravity at that loading or her
  stabilizer's authority, not yet found.
- **The light aircraft at height - mixed.** Excluded, past the book:
  the C172P at 110 kt at 10,000 ft (127 KTAS; POH figure 5-8, 121 KTAS at
  8,000 ft at 77%); the PA-28-180 at 110 and 112 kt there (127 KTAS and more;
  75% at its optimum 7,000 ft is 124.3 KTAS); the C182 at 129 kt there (150
  KTAS; her greatest 145 KTAS at sea level, 140 at 80% at 6,000 ft) - all
  from assets/figures/. Open, a defect: the C172P at 100 kt at 2,400 lb and
  the C182 at 64, 71 and 120 kt at 10,000 ft, and the C182 at 129 kt at
  5,000 ft: the trim starts her mixture full rich, which at height JSBSim's
  piston burns ever worse (its combustion efficiency falls as the
  equivalence ratio climbs with height; the C182's engine stops at 10,000
  ft, udot the same at closed and open throttle). Her book's figures are
  leaned. Unsourced: the J-3 Cub at 60 and 62 kt at 10,000 ft (no mixture
  lever; no Cub figure gives a speed at height) and the C182 at 120 kt there.
- CI on f5443f30: one failure, on Ubuntu debug, the window client's
  `pressing_m_3_times` test - its imagery server reset the connection
  ("the client drew nothing"), nothing of this change's.

### The S.23's boost settled through her trim; the rest measured, 2026-10-23

**What is still not true first.** Open: the S.23 at 10,000 ft at 130 and
141 kt and at 5,000 ft at 141 kt heavy (fine pitch); the C172P and C182 at
height (trimmed full rich); the J-3 Cub at 10,000 ft (unsourced). The
Learjet at 300 kt at her model's own loading is past her book. The walk
trims 497 of 540, 43 left out by name (31 not trimmed, every one said).

- **The S.23 - 19 of her 27 points fixed.** Two causes, each at its cause
  in tools/make_short_s23.py:
  - her variable datum boost control's integrator ran at every pass of
    JSBSim's trim (FGPID integrates at its own step whether or not time is
    stopped; the trim suspends integration, so `simulation/dt` is nought).
    Its rate is now gated on `simulation/dt` above nought, and the model
    sets `glideslope/trim/settle-throttle-loops`: Aircraft::initialize then
    trims, flies a second, and trims again from her start until a second's
    flight moves no throttle valve by a ten-thousandth - her boost where its
    datum puts it. The server's start and every other start are the one
    path, `initialize`. Seen red with the settling switched off (her valves
    move once she flies: 1,000 ft, 130 kt and every other point off);
  - her `<alphalimits>` reached 24 degrees, past her greatest lift at 14
    (CLwbh's peak, 0.2443 rad): the 737's failure at her slowest (86-98 kt).
    Now 0.2443.
  `the_s23_trimmed_at_every_loading_height_and_speed_flies_on_as_trimmed_her_boost_settled`:
  each of the 27 trimmed and held thirty seconds on what the server holds
  her on, within 20 ft and 2 kt - 19 held, to the foot and the knot; 8
  named open. Those 8 trim in coarse pitch (measured with her lever at 0):
  every trimmed start sets the propeller lever at 1, fine pitch for a
  two-pitch airscrew, and her book's level speeds are flown in coarse (200
  mph at 5,500 ft). Which pitch a start in the air should use is the
  owner's to say.
- **Mixture at height - cause measured, not fixed.** JSBSim's piston meters
  fuel as the lever times sea-level over ambient pressure
  (FGPiston::doFuelFlow), so full rich at height is ever richer and its
  power falls (sim/leaner.hpp). Trimmed with the lever at 0.65 the C182
  trims at 64, 71 and 120 kt at 10,000 ft (162 hp at 120 kt, 70%) and at
  129 kt at 5,000 ft; at 1.0 her engine stops at 10,000 ft. So her engine
  is sound; the start is not leaned. The fix: a start above the catalogue's
  leaning height leans for best power before the trim and holds that lever
  until her first input, through the same held controls as the rest - not
  done in this time box.
- **The Learjet at 300 kt at her model's own loading - excluded.** Her
  centre of gravity there is station 392.0, 36.1% of her mean chord (LEMAC
  362.17, MAC 82.75 in), aft of the flight manual's 30% limit
  (tools/make_learjet35a.py: the NTSB empty weight's 32% with full fuel
  aft). At 250 kt her pitch trim is already 0.95 of its travel; at the
  figures' loadings (18.1% and 24.6%) she trims at 300 kt with 0.50 and
  0.60. That a player given no loading flies her model's own, past her
  limit, is a tail.
- **The J-3 Cub at 60 and 62 kt at 10,000 ft - unsourced.** Searched: the
  Cub's figures file (manual: ceiling 14,000 ft solo; cruise 73 mph at
  2,150 rpm), AOPA's and AeroCorner's J-3 pages (ceiling 11,500 ft, 450
  ft/min, 76 kt at sea level); no source gives a level speed at height.
  By power alone, 72 KTAS there needs about 0.65 of her sea-level power
  where she has about 0.70: too near to call.
- The C182 at 120 kt at 10,000 ft, unsourced last round, is inside her
  model leaned (above).

### Airborne starts' levers as the book flies there, 2026-10-23

**The owner's decision (2026-10-23).** An airborne start sets its levers as
the book flies that height and speed: the mixture leaned to the handbook's
setting for the height, the propeller in the pitch the book flies that
speed at (coarse in cruise, fine for slow flight), held so until the
player's first input. Server, player and AI starts take the same path.

**What is still not true first.** Unsourced and open: the S.23 at her
figures' loadings (40,500 and 45,000 lb) at 130 and 141 kt at 10,000 ft;
the C182 at 3,100 lb at 120 kt at 10,000 ft; the J-3 Cub at 60 and 62 kt
at 10,000 ft. Both plan items stay open. The walk trims 513 of 540; 27
left out by name (20 not trimmed, every one said).

**What was built.**
- Per aircraft, as data: the handbook's leaning rule is the catalogue's
  `mixture-lever FULL_RICH_BELOW_FT`, already cited from each handbook
  (C172P POH figure 5-6, "leaned above 3000 feet"; Cherokee 180 section III,
  5,000 ft; C182S, 3,000 ft); the pitch rule is a new catalogue keyword,
  `airscrew-coarse-above KCAS`, the S.23's 100 kt - Flight's figures fly her
  climb at 100 kt in fine pitch and her 200 mph level speed in coarse
  (assets/figures/short_s23.xml); her book gives no other speed between.
- In `Aircraft::initialize`, the one start every server, player and AI
  aircraft takes: the propeller lever 0 (coarse) above that speed, else 1;
  the mixture full rich, or, with a mixture lever above the handbook's
  height, leaned for the most power - at full throttle, each engine started
  again and settled at each lever (a fiftieth from 0.3 to 1, then golden
  section to a thousandth). JSBSim's `set-running` puts the mixture full
  rich (FGPropulsion::SetEngineRunning), so the levers are set again after
  every start, and after a failed trim's `InitModel`.
- Held until her first input: `held_until_moved` reads both levers, and
  `WATCH` carries them after the held trim (two `i16`, protocol `0F` still,
  docs/TRANSPORT.md); `keep_held` begins a client's inputs from them, so a
  prediction starts from the server's levers.

**Measured.** C172P at 100 kt at 10,000 ft leaned to 0.74, 94.5 hp:
trimmed laterally, both loadings. C182 leaned to 0.59 at 10,000 ft: trimmed
at 64, 71 and 120 kt (162 hp, 70%) at her own loading; 0.72 at 5,000 ft at
129 kt (181.5 hp). The S.23 in coarse pitch: her own loading at 130 and 141
kt at 10,000 ft, and 141 kt at 5,000 ft heavy, trimmed. The J-3 Cub has no
mixture lever (make_j3cub.py: her carburettor meters it) and still does not
trim at 60-62 kt at 10,000 ft. Searched for her: the figures file (manual,
14,000 ft solo; cruise 73 mph), AOPA and AeroCorner (11,500 ft, 76 kt at sea
level), the 1946 J3C-65 owner's manual listings - none gives a level speed
at height. A first build leaned before the trim and left the engine
settled closed: it stopped (-1.5 hp) and the C172P lost her lateral trim at
3,000 ft - seen red, fixed by starting her again.

**Verification.** The walk and the S.23 test as above (S.23: 23 of 27 held
to the foot and the knot, 4 named); `a_state_packet_carries_the_controls...`
with both levers; the catalogue's first-input test (every control to the
bit, now the mixture and propeller with them), the first-input, STOVL and
untouched server tests: see the ctest run in this commit's message.
`glideslope_cli selftest` hash 831a4e7954233b0a, unmoved.

### The last points at 10,000 ft: sourced, derived, or open, 2026-10-23

**What is still not true first.** Open, each her model short of what her
book gives or derives at 10,000 ft: the S.23 at 45,000 lb at 130 and 141
kt; the C182 at 3,100 lb at 120 kt; the J-3 Cub at 922 lb at 62 kt (and at
1,092 lb at 60 kt, within the derivation's spread). Neither item is ticked.

**The C182 at 3,100 lb at 120 kt at 10,000 ft - inside her book, open.**
The Civil Air Patrol's copy of the Cessna 182T Nav III POH
(https://sanfernando35.cap.gov/media/documents/182poh_23224F1C15332.pdf,
182TPHBUS-03), figure 5-9 sheet 8, cruise performance at 10,000 ft, 3,100
lb, recommended lean, standard temperature: 2,400 rpm and 20 in, 69%, 139
KTAS; 2,300 rpm and 21 in, 71%, 141 KTAS. The 182T has the 182S's engine
(IO-540-AB1A5, 230 hp at 2,400 rpm) and weight. 120 KCAS there is 139.6
KTAS; full throttle there (about 20.6 in) makes about 141. Her model, leaned
for best power (0.59), holds 117 KCAS (135.9 KTAS) at 165 hp, 72% - where
the book gives 139 KTAS at 69% - and not 120: about 4 kt slow at 10,000 ft,
where at 6,000 ft she is 1.3 kt slow (cruise_speed_6000_ft, 138.7 against
140). Her drag polar is a compromise between her climb and ceiling
(make_c182.py); a fix is a figure at 10,000 ft and her power or propeller
lapse fitted to it - not done in this time box.

**The derivation** (scratchpad, deriv.py, standard atmosphere): power
required P = (q S CD0 + W^2/(q S pi e A)) V; power available at height the
sea-level thrust horsepower times Gagg and Ferrar's lapse, 1.132 sigma -
0.132 (above a supercharged engine's rated altitude, on the density ratio
to that altitude).
- **J-3 Cub** (J3C-65): 87 mph (75.6 kt) top speed at sea level, 450 ft/min
  climb and 11,500 ft service ceiling at 1,220 lb (AOPA's and AeroCorner's
  J-3 pages; 178.5 sq ft, 35 ft 3 in span). Fitted to the top speed and the
  climb at e = 0.8, her thrust horsepower is 31.2 (0.48 of 65) and CD0
  0.032, and the ceiling comes out 11,300 ft - the published 11,500, which
  checks it (e = 0.6 and 0.7 give 9,700 and 10,500). At 10,000 ft she holds
  at most 57.7 KCAS at 1,220 lb, 60.6 at 1,092 and 63.2 at 922 (e = 0.6 to
  0.8: 55.4-57.7, 59.6-60.6, 63.1-63.2). So 60 and 62 kt at 1,220 lb and 62
  at 1,092 are past her: **excluded, by this derivation**. 60 at 1,092 lies
  inside the derivation's spread (open, named); 62 at 922 she holds by 1 kt
  and her model does not (it holds 60, not 62): open.
- **S.23**: 815 hp an engine at her rated altitude (Flight, 12 November
  1936, in eng_PegasusXc.xml: "Normal output at rated altitude 785/815
  BHP"), 200 mph (173.8 kt) at 5,500 ft at 40,500 lb (Flight's
  specification, figures file), 1,500 sq ft and 114 ft span. Fitted to the
  top speed (e 0.7-0.8, propeller 0.75-0.85), at 10,000 ft she makes 168-
  170 KTAS at 40,500 lb and 164-167 at 45,000: 130 kt (151 KTAS) is inside
  both, 141 kt (164 KTAS) at the edge at 45,000. Her model: started with her
  mixture lever full (rich, the take-off setting with its take-off boost) she
  could not be trimmed at 130 and 141 kt at 10,000 ft at 40,500 lb; her
  catalogue now says `mixture-lever 4500` (rich below her engines' rated
  altitude, 4,500 ft, set for the most power above, as her crews set the
  automatic mixture back from take-off - CAB 1943's take-off, rated and
  cruising ratios in engines.xml) and at 40,500 lb she trims at both (lever
  0.82). At 45,000 lb leaned she still makes too little (udot not
  trimmable): open, named in both tests.

**Verification.** The walk: 513 of 540 trimmed, 27 left out by name (18
not trimmed, every one said). The S.23 test: 25 of 27 held to the foot and
the knot, 2 named open at 45,000 lb. 93 related tests pass in linux-debug
(the S.23's, every aircraft's, the catalogue's, first input, the state
packet and the transport document, the fixed ports); `glideslope_cli
selftest` hash 831a4e7954233b0a, unmoved.

### Why each is slow at 10,000 ft: measured against the books, 2026-10-23

**What is still not true first.** The S.23 at 45,000 lb at 130 and 141 kt
at 10,000 ft is still open, its cause found and not fixed; neither item is
ticked. The C182 and the J-3 Cub were not slow: the figures they were held
to were wrong, and are corrected below. No model changed.

- **The C182 - past her own book.** The 139 KTAS at 69% cited last round
  is the 182T's (figure 5-9, 182TPHBUS-03): another airframe, 150 kt at sea
  level against the 182S's 145. Her own Information Manual (182S, 1997,
  figure 5-9) at 10,000 ft, 3,100 lb, standard: 2,400 rpm and 20 in, 66%,
  133 KTAS; 19 in, 62%, 128. Its cruise tables from sea level to 10,000 ft
  fit one polar at a constant propeller efficiency (power = a sigma V^3 +
  b / (sigma V), every row within 1.3 points), which gives 138.1 KTAS at
  72%, her best power at full throttle there (165 hp, leaned). 120 KCAS is
  139.6 KTAS: past her. Her engine is sound at height (72% leaned at full
  throttle against the book's 66% at 20 in recommended lean); her model
  needs 3 to 5% more power than her book at every height from 4,000 to
  10,000 ft (trimmed at 100, 110 and 120 KCAS: 1.06, 1.03, 1.04 at 4,000
  ft; 1.03 to 1.05 at 8,000 and 10,000), so nothing grows with height -
  it is the 1.3 kt her 6,000 ft cruise figure already shows. Excluded.
- **The J-3 Cub - the derivation left out her carburettor.** Measured: at
  10,000 ft solo she trims to 61.2 KCAS. Redone (Gagg and Ferrar on her
  power, fitted to 87 mph and 450 ft/min at 1,220 lb as before), the
  derivation assumed best-power mixture at height; she has no mixture
  lever, and her float carburettor runs 18% richer at 10,000 ft than at sea
  level (tools/piston_mixture.py, FAA-H-8083-32 page 2-3), 0.103 fuel/air
  against 0.090, 94% of the power. With it, she holds at most 61.2 to 61.5
  KCAS at 922 lb (Oswald 0.6 to 0.8), 57.3 to 58.7 at 1,092 and 54.3 to
  55.6 at 1,220: her model's 61.2 is the derivation's. Every Cub point at
  10,000 ft left out of the walk is past her. Her power lapse is JSBSim's:
  the brake power is the indicated, proportional to the charge's density,
  less a friction that does not lapse (FMEP, 18,400 Pa per m/s of piston
  speed plus 46,500), about 2% steeper than Gagg and Ferrar's.
- **The S.23 - her drag due to lift, open.** Not her engines: at 10,000
  ft she makes 715 hp an engine at 2,547 rpm at 141 KCAS, more than the
  derivation's 672 (815 hp at 4,500 ft, Gagg and Ferrar above); at 5,500 ft
  807 at 2,623 rpm where Flight gives 830 all out. (Her supercharger's
  multiplier comes from the take-off boost at sea level, so she reaches
  only +0.8 lb at 5,500 ft where the book flies +2 1/2; the power comes out
  right through the engine's calibration, and is left.) Her drag is the
  DATCOM table times 1.9 at every angle of attack (make_short_s23.py), the
  drag due to lift with it: fitted to her trimmed drag from 110 to 160
  KCAS it is a zero-lift drag of 0.027 and an Oswald efficiency of 0.33,
  where Raymer's estimate for her straight wing (aspect ratio 8.66) is
  0.79. Tried in the build's data, the table's part above its least times
  1.4 and the rest times 2.1: 200 mph at 5,500 ft, 145 KCAS at 40,500 lb
  at 10,000 ft (the derivation's 145 to 146.5), 130 kt at 45,000 lb trims -
  and the sea-level climb in fine pitch is 1,115 ft/min against 950 +-10%.
  Times 2.3 and 1.55 the climb is 991 and the top speed 185 mph. Her
  climb is flown at 100 kt in fine pitch, where her airscrews are 0.86
  efficient; what holds both figures is the next step, not done in this
  time box.

**Verification.**
- `the_j3_cub_flown_solo_at_10000_ft_holds_her_derived_top_speed_within_a_knot`:
  trimmed at 60.2 KCAS (throttle 0.943, 39.7 hp), not at 62.5. Seen red
  with her carburettor's richening taken out of the build's data (trimmed
  at 62.5) and doubled (not at 60.2).
- `the_cessna_182s_at_10000_ft_needs_her_books_power_for_her_books_cruise_speed`:
  trimmed at 132.9 KTAS she needs 157.1 hp, the book's 151.8 +3.5%, within
  5%. Seen red with her zero-lift drag 0.030 -> 0.033 in the build's data
  (not trimmed there).
- The walk's left-out points are the same; their reasons now say which are
  past the book and which is open (the S.23's two).

### A pilot works the speedbrakes from the stick, the quadrant and the keyboard; the HUD shows them; protocol version 6, 2026-10-07 — tail done, one found

**What was missing.** `Controls::speedbrake` was flown, sent and predicted
like every other control, but nothing a pilot held moved it: no binding, no
key. So by hand the B-2A could not open her drag rudders, and the bomber
lessons' speed bands stayed at `vref+20` for her pilot.

**What changed.**
- **Every input path moves the lever.** `platform::Control::speedbrake`,
  named `speedbrake` in `assets/input/bindings.txt`: the throttle
  quadrant's last axis (axis 7, `lever 1`: pulled back opens them, as an
  airliner's does - it was a second elevator before), and buttons 13 and 14
  on both a stick and a quadrant, a quarter of travel a press (they trimmed
  the pitch before; each device's other trim buttons and hat still do). The
  keyboard's semicolon and apostrophe work it as a lever, in and out, at the
  throttle's rate, and it stays where it is left. Every axis, button and hat
  is still bound (`every_axis_button_and_hat_of_a_virtual_stick_and_throttle_moves_the_controls`).
- **Which aircraft have speedbrakes is data**: the catalogue's new
  `speedbrakes` command (`sim/catalogue.hpp`), read into
  `Aircraft::speedbrakes()`. Eight say it: the 737-300, 747-400, 787-8,
  A320, A380, B-2A, F-22A and Learjet 35A. JSBSim binds
  `fcs/speedbrake-cmd-norm` for every model, so the model alone cannot say.
- **The HUD shows the lever** - `SPEEDBRAKE 0.50`, after the gear - only
  where there are speedbrakes, for the aircraft flown (`Flight::controls_shown`)
  and the one ridden along in. The trace carries `speedbrake` (-1 where none),
  and the frame tests' judge (`hud_judge.hpp`) holds the line to it: the
  A320's jet frame now shows and is judged on it.
- **Protocol version 6**: a state update's watched controls carry the
  speedbrake lever after the gear, `-32768` where there are none - 16 bytes
  where there were 14; a full packet is 1,138 bytes, 1,168 sealed, inside
  the 1,232. `TRANSPORT.md`, the client written from it (`0x06`, seven
  controls read), the gearstick refusal (`474c4453060401`) and the version
  test say 6; the collision ground is unchanged. The server fills it from
  the model (`controls_of`), the client blends it between updates as it
  does the flaps, and `glideslope_cli --track` writes it as a sixth control,
  which `interpolation_check` judges with the other five.
- **The bomber lessons' bands come back.** Approach and landing:
  `vref-8` to `vref+12` (a light aircraft's), from `vref+20`; the circuit's
  final: `vref-10` to `vref+15` (the airliners' and fighters'), from
  `vref+20`. Each lesson now tells the pilot to open them half way. The AI
  flies both inside them (`an_instructor_demonstrates_an_approach_and_hands_it_over`,
  `..._a_circuit_...`).

**Verification** (linux-release, locally):
- `every_aircraft_with_speedbrakes_slows_on_its_lever_held_level_and_every_other_ignores_it`:
  all sixteen flown level ten seconds at 5,000 ft by a test pilot, lever
  stowed and out. The eight are 3.1 (747-400) to 30.5 (B-2A) knots slower -
  737 17.6, 787 13.9, A320 12.6, A380 14.4, F-22A 24.3, Learjet 4.7 - against
  a bound of 2; the other eight fly bit-for-bit the same flight. Held level
  because spoilers dump lift: left alone the Learjet gained energy with them
  out. **The F-15C's lever does nothing**: her model's surface moves but
  draws no drag (her checklist said so), so she is not flagged, and a tail
  is added.
- `a_pilot_flies_the_b2a_approach_lesson_by_hand_inside_its_band_only_with_her_speedbrakes_out`:
  a test pilot's hands, the throttle worked for the speed at no more than a
  hand's pace - half out, 124.0 to 132.5 kt against vref 124.0, nothing
  said; stowed, up to 150.5 kt and both speed faults said.
- `handing_the_aircraft_between_pilot_and_ai_steps_nothing_in_any_phase`: a
  B-2A phase, her pilot running the lever out while the AI has her - back,
  the lever catches up at a hand's pace in 1.01 s. The step it measures now
  covers every control (`as_list`), not nine.
- `a_client_working_the_speedbrakes_is_predicted_as_the_server_flies_them_in_every_aircraft_with_them`:
  the eight at 200 ms, the lever run out and in, corrected by no more than a
  quarter over the same flight stowed (worst: F-22A 0.137 m either way).
  From the review: each side's flight model is read back at every step
  (`fcs/speedbrake-cmd-norm`) and must hold the lever sent - zero steps off
  in all eight; a client flying it stowed while sending it out is caught,
  in-test, at every step it was out. The correction alone shows that only
  in the 737 (0.094 m against 0.037), which is held as an assertion.
- From the review too: README's Getting started lists the flying keys and
  the speedbrake's buttons and quadrant lever; the client's `--help` lists
  no controls at all, a new tail.
- `a_state_packet_carries_the_controls_of_the_aircraft_its_client_watches`
  (half out, stowed and none read back), the HUD line checks
  (`hud_lines_check`, three new cases; `hud_horizon_check`, gear and lever
  in four pairings), the lever and binding tests, the `TRANSPORT.md`
  agreement tests, the client from the document, the gearstick refusal, and
  the window client's HUD, ride-along and take-over tests: green.
- **Seen to fail**, one build with five deliberate bugs, all seven red: the
  lever not applied to the model (the every-aircraft test, and the
  hand-flown approach's "half out, the lesson had nothing to say: 2
  things"); the HUD line dropped (both HUD checks); the keyboard's lever
  dropped; the lever left out of the controller's catch-up ("back to the
  pilot, no control moves faster than a hand: 1.000000"); and the watched
  lever read back as 0. Reverted, all green.
- A test-label fix found on the way: `control_name` in `test_lesson.cpp`
  named the controls out of `as_list`'s order (the speedbrake last); it is
  now in order and held to `control_count`.

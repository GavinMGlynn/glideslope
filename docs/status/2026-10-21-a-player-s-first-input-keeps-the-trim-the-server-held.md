### A player's first input keeps the trim the server held, 2026-10-21

**What is still not true first.** The F-22A reads no yaw trim and the
Mosquito neither roll nor yaw trim; both are untrimmable and held on nought
there, so nothing is lost, but a trim sent them does nothing. The
untrimmable three (F-22A, Mosquito, S.23) and the C182 are held as #222 left
them. Neither client has a control for roll or yaw trim: a player keeps
what she was held on, and cannot move it. The windowed client's change is
compiled and run only by its learnt-landing test; the figures below are the
command-line client's, which begins from the same function.

**What changed.**

- **The aileron and rudder her trim found are held as her roll and yaw
  trim**, the stick and pedals centred (`sim::held_until_moved`; the project
  owner, 2026-10-22). Two controls are added, `Controls::roll_trim` and
  `yaw_trim`, flown as JSBSim's `fcs/roll-trim-cmd-norm` and
  `fcs/yaw-trim-cmd-norm`, which the models sum with the aileron and rudder
  commands one for one (JSBSim's FGFCS ties both properties; e.g. c172p.xml's
  "Roll Trim Sum"). So the stick and pedals centred after her first input
  keep her flying as trimmed.
- **Protocol 15, `0F`**, in docs/TRANSPORT.md byte for byte: an input frame
  carries twenty-one controls, the roll and yaw trim last; a watched
  aircraft's controls carry her pitch, roll and yaw trim after the
  speedbrake lever. A full state update is 1,153 bytes, 1,183 sealed.
- **Both clients begin a player's inputs from every control the server held
  her on** (`frontend::keep_held`), the three trims among them. Her
  prediction begins on the same (`held_as_told`, #232). Before, her client
  sent its own pitch trim, aileron and rudder - nought - from its first
  frame, and her trim was gone as she joined.
- `frontend::levers_of` builds what `WATCH` says from her model, shared by
  the server and the tests. A hand-over's blend moves the two trims as it
  moves every other control.
- **A player started on final is trimmed laterally too**
  (`--players-on-final`). Trimmed in pitch alone, and her client keeping that
  trim, the windowed client's C172P on final banked 5.9 degrees in 1.3 s
  and was 5 degrees off the runway's heading at 2.8 s, refused the learnt
  landing's gate (client_learnt_landing.cmake failed on it).
- `glideslope_cli connect --predict --from-held [--nudge]` joins as the
  windowed client does and sends what the server held her on; `--nudge`
  moves the stick back and right a fortieth for her first half second, then
  centres the stick and pedals. `--until-held` now says her pitch's range
  and her most bank.

**Verification.**

- `a_players_f35b_and_c172p_flown_from_their_held_trim_with_a_small_first_stick_movement_hold_height_and_attitude_and_predict_alike`
  (tests/cmake/server_first_input.cmake): a real server, through a relay of
  100 ms each way, four runs counted - each aircraft untouched, and nudged
  then centred. linux-debug, thirty seconds each, about 707 updates
  compared: F-35B untouched 1,524.0 to 1,526.2 m, pitch 6.12 to 6.17, bank
  0.08; nudged 1,524.0 to 1,534.1 m (33 ft), pitch 6.03 to 6.58, bank 2.58
  (her flight controls hold the attitude a stick movement leaves her at).
  C172P untouched 1,524.0 m throughout; nudged 1,523.3 to 1,525.7 m, pitch
  0.05 to 0.93, bank 0.82. Prediction median 0.000 m every run, worst up
  to 1.378 m (bound 3 m; a correction while joining, moving run to run).
  Seen red twice: with the client keeping its own pitch trim, aileron and
  rudder, as before, the F-35B untouched was a wreck; with the aileron and
  rudder held as the stick and pedals rather than as trims, the C172P nudged
  then centred banked 36.8 degrees and fell 90 m.
- `every_aircraft_held_on_her_roll_and_yaw_trim_flies_as_on_the_aileron_and_rudder_her_trim_found`
  (tests/unit/test_catalogue.cpp): all 16 aircraft; each trim tried against
  its control for two seconds, and thirty seconds held on the trims against
  the same on the stick and pedals - 0.000 ft and 0.000 degrees apart for
  every one. Held on a roll trim: the PA-28 0.087, the C182 0.049, the
  C172P 0.032, the Cub 0.012. Named, reading not both: the F-22A (no yaw
  trim) and the Mosquito (neither), asserted to be exactly those two.
- `every_aircraft_a_players_first_input_sends_is_what_the_server_held_her_on`:
  all 16, her levers sent through a state update and read back, the
  windowed client's airborne controls put where they say: all twenty-one
  controls the server's to the bit (fixed gear and absent speedbrakes
  excepted). Seen red with the client keeping its own trims: every trimmed
  aircraft named.
- Protocol, input, state packet, document and gearstick tests moved to
  `0F`; #222's untouched tests, the STOVL-at-once test and the on-final
  learnt landing tests pass.
- `glideslope_cli selftest`: hash 831a4e7954233b0a in linux-debug, as on
  this branch's base (#232). No x86-64-v2 run: the nearest figure to its bound,
  the nudged F-35B's bank at 2.58 of 5 degrees, is what the nudge asks of her.

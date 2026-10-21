### A player's first input keeps the trim the server held, 2026-10-21

**What is still not true first.** The held aileron and rudder are stick
positions, not trims: the aeroplanes have no aileron or rudder trim to keep
them in. Her client keeps them until a control of hers moves them - a
keyboard's arrow pressed and let go, or a stick's axis read when it is first
seen - and then they are her stick's, centred at nought. The pitch trim is
kept until she trims. The untrimmable three (F-22A, Mosquito, S.23) and the
C182 are held as #222 left them. The windowed client's change is compiled
and run only by its learnt-landing test; the figures below are the
command-line client's, which begins from the same function.

**What changed.**

- **The server tells a watched aircraft's pitch trim** (protocol 15, `0F`,
  in docs/TRANSPORT.md byte for byte): one more 16-bit control after the
  speedbrake lever. A full state update is 1,149 bytes, 1,179 sealed.
- **Both clients begin a player's inputs from every control the server held
  her on** (`frontend::keep_held`): her pitch trim, aileron, rudder and
  elevator now, besides the throttle, flaps, gear and speedbrake lever kept
  before. Her prediction begins on the same (`held_as_told`, #232). Before,
  her client sent its own pitch trim, aileron and rudder - nought - from its
  first frame, so her trim was dropped as she joined.
- `frontend::levers_of` builds what `WATCH` says from her model, shared by
  the server and the test.
- **A player started on final is trimmed laterally too**
  (`--players-on-final`). Trimmed in pitch alone, and her client now keeping
  that trim, the windowed client's C172P on final banked 5.9 degrees in
  1.3 s and was 5 degrees off the runway's heading at 2.8 s, refused the
  learnt landing's gate (client_learnt_landing.cmake failed on it).
- `glideslope_cli connect --predict --from-held [--nudge]` joins as the
  windowed client does and sends what the server held her on, with
  `--nudge` the stick back a fortieth for her first half second; and
  `--until-held` now says her pitch's range and her most bank.

**Verification.**

- `a_players_f35b_and_c172p_flown_from_their_held_trim_with_a_small_first_stick_movement_hold_height_and_attitude_and_predict_alike`
  (tests/cmake/server_first_input.cmake): a real server, through a relay of
  100 ms each way, four runs counted - each aircraft untouched and nudged.
  linux-debug, thirty seconds each, 707 or 708 updates compared:
  F-35B untouched 1,524.0 to 1,526.2 m, pitch 6.12 to 6.17, bank 0.08;
  nudged 1,524.0 to 1,537.5 m (44 ft; her flight controls hold the half
  degree a stick movement leaves her at), pitch 6.12 to 6.58. C172P
  untouched 1,524.0 m throughout, pitch 0.34; nudged 1,523.4 to 1,525.7 m,
  pitch 0.07 to 0.92, bank 0.06. Prediction median 0.000 m every run, worst
  0.000 to 1.379 m (bound 3 m) - the worst is a correction while joining,
  and moves run to run. A nudge of a twentieth rose the F-35B 25 m, too
  near the 30.48 m bound, so it is a fortieth. Seen red with the client
  keeping its own pitch trim, aileron and rudder, as before: the F-35B
  untouched was a wreck.
- `every_aircraft_a_players_first_input_sends_is_what_the_server_held_her_on`
  (tests/unit/test_catalogue.cpp): all 16 aircraft started as the server
  starts a player's, her levers sent through a state update and read back,
  the windowed client's airborne controls put where they say: all nineteen
  controls the server's to the bit (fixed gear and absent speedbrakes, which
  her model ignores, excepted), 16 of 16, and some held on a pitch trim
  (the Learjet on -0.95, the 747 on +0.42). Seen red with the fix reverted:
  every trimmed aircraft named.
- Protocol, state packet, document and gearstick tests moved to `0F`;
  `a_client_selecting_stovl_on_its_first_input_...`, the on-final learnt
  landing tests and #222's untouched tests pass.
- `glideslope_cli selftest`: hash 831a4e7954233b0a in linux-debug, as on
  this branch's base (#232): the selftest flies no server. No x86-64-v2 run:
  no figure is within half of its bound.

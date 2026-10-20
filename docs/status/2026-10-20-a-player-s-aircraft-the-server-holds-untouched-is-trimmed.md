### A player's aircraft the server holds untouched is trimmed, 2026-10-20

**What is still not true first.** Three aircraft cannot be trimmed - the
F-22A, whose trim does not converge, and the Mosquito and the S.23, whose
constant-speed propellers leave the throttle unable to trim her speed
("udot doesn't appear to be trimmable") - and are held as before, on six
tenths of the throttle: untouched for thirty seconds the F-22A falls 500 ft,
the Mosquito 1,500 and the S.23 climbs 340. And a player's first input drops
the trim: her client keeps the server's throttle, flaps, gear and speedbrake
lever (`WATCH`), but sends its own pitch trim, aileron and rudder. Both are
tails in COMPLETION_PLAN.md, Phase 9.

**What changed.** Until a player's first input the server held her aircraft
at six tenths of the throttle with every other control at its default - the
stick neutral, no trim, and the gear lever *down* (`Controls::gear` defaults
to 1) on an aircraft started with it up. An F-35B whose client was slow to
send fell from 1,524 m to 26 m in thirty seconds.

- The server now starts a player's aircraft (`Fleet::give`) trimmed level
  (`InitialConditions::trim`), and laterally too: the new
  `InitialConditions::trim_lateral` asks JSBSim's full trim, which finds the
  aileron and rudder against her engine's torque, and falls back to the
  longitudinal trim where it cannot. Trimmed in pitch alone, a C172P rolled
  off to 30 degrees of bank and lost 280 ft in thirty seconds; the C182 lost
  600 and the PA-28 390. `Aircraft::trimmed_laterally()` says which.
- It holds `sim::held_until_moved`: what the trim found (`trimmed_controls`,
  with the aileron and rudder where the trim was lateral), or six tenths of
  the throttle where it could not trim her, and the gear lever where she was
  started with it either way. On final (`--players-on-final`) she was
  already trimmed; she is held the same way, with her gear down as started.
- **A failed trim now leaves her as she was.** JSBSim's `RunIC` sets the
  motion again but not the flight-control system's filters and integrators,
  which kept wherever the trim's search left them: an F-22A whose trim had
  failed fell 3,000 ft in thirty seconds, where started untrimmed she falls
  500. `Aircraft::initialize` now resets the FCS (`FGFCS::InitModel`, its
  flap and speedbrake commands set again) before setting her up again, and
  she falls the 500 exactly. The on-final B-2 trim that fails takes this
  path too.
- The client's prediction: a client adopts the server's motion and its
  levers when it joins, and predicts only from its own first input; the
  server's held controls before that are what the server flew, which it
  puts the prediction right by. Its levers (`WATCH`) now tell the gear up
  where it was down before. No message changed.
- `glideslope_cli connect --until-held S` stays until its own aircraft has
  been heard for S seconds of the session's clock and says (on `--heard`)
  how high she was first heard and how low and high she went.

**Verification.**

- `a_players_f35b_and_c172p_left_untouched_for_thirty_seconds_hold_their_start_height_within_100_ft`
  (tests/cmake/server_untouched.cmake): a real server, two clients asking
  for an F-35B and a C172P and sending no input at all, each staying until
  thirty seconds of the session's clock. linux-debug: the F-35B 1,524.0 m
  first heard, 1,524.0 to 1,526.1 m (7 ft); the C172P 1,828.8 m, 1,828.8 to
  1,828.9 m. Seen red with the server's fix reverted: the F-35B fell to
  25.6 m.
- `every_aircraft_a_player_is_given_and_leaves_untouched_holds_her_start_height`
  (tests/unit/test_catalogue.cpp): every one of the catalogue's 16 aircraft,
  started at 3,000 ft at her catalogue airspeed as the server starts a
  player's and held thirty seconds on `held_until_moved`; 13 trimmed and
  within 100 ft (the C182 the most, 95 ft up; the rest within 7), the three
  above left out by name and checked still untrimmable, and the count
  asserted. Seen red with the aileron and rudder left out of what is held:
  the C172P, C182 and PA-28 failed, named.
- `glideslope_cli selftest`: hash ef497a75d22b8798 in linux-debug; the
  selftest trims nothing, and nothing it flies changed.

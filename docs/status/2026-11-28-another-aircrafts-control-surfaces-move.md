### Another aircraft's control surfaces move, 2026-11-28

Phase 12c, "Another aircraft on a server is drawn with its control surfaces
still".

**What is still not true, first.**
- **The B-2's rudder is still not drawn**, on a server or off it: the
  reason is the livery item's (2026-11-27).
- **A surface is drawn to a half degree**, which is what the wire carries;
  between two updates it is moved in a straight line, as the attitude is.
- **The velocity in a state update is coarser than it was**: sixteenths of
  a metre a second, where it was a float (below, and why).

## What changed

- **Protocol version 21 (`15`).** Each aircraft in a state update carries
  four more bytes: its left aileron, right aileron, elevator and rudder,
  each the JSBSim property's value (`fcs/left-aileron-pos-rad`,
  `fcs/right-aileron-pos-rad`, `fcs/elevator-pos-rad`,
  `fcs/rudder-pos-rad`) as a signed byte of half degrees, held at the ends.
  Those four are every property the surfaces files name; a model without
  one is sent nought for it (the C182 writes no right aileron, and her file
  draws it from the left).
- **Why the velocity moved too.** Twenty aircraft with four bytes more did
  not fit a datagram: 1,269 bytes sealed, of 1,232. An aircraft's velocity
  in a state update is now three `i16`s of sixteenths of a metre a second
  (at most 3 cm/s off, ±2,048 m/s), not three `f32`s, which gives back six
  bytes an aircraft: a full packet is 1,119 bytes, 1,149 sealed. Where
  the wire's velocity is read, and what the sixteenths do to each:
  - other aircraft carried on between updates, and a ridden aircraft's
    ground speed;
  - the client's own aircraft's line, read in three places: the copilot's
    situation, its airspeed and vertical speed
    (frontend/players_copilot.cpp) - at most about 0.1 kt and 11 fpm off,
    and copilot playback unaffected, since a recording is matched on its
    words, not its numbers (`but_numbers`); the learnt landing's "at rest"
    check, under 0.1 m/s (client/main.cpp, cli/main.cpp); and
    `--when-hovering`, under 0.5 m/s (cli/main.cpp) - each a speed near
    nought, where a sixteenth of a metre a second moves the threshold by
    at most that much.
  Reconciliation is unchanged: a client's prediction is put right by its
  own motion (`OwnMotion`), whose floats are as they were. The alternatives looked at - the
  angles as `i16`s, or packing the surfaces into six bits each - either
  moved more of the wire or drew to a degree.
- docs/TRANSPORT.md says all of it byte for byte; the client written from
  the document (tests/doc_client) reads the new layout.
- **The server** writes them (net::surfaces_on_the_wire); **the window
  client** moves them between updates as it moves the attitude
  (net::Interpolated, `RemoteState::surfaces_rad`), and draws each surface of
  another aircraft's model by the carried deflection its property is
  (client::carried_angles). A surfaces file naming a property a state update
  does not carry is refused as the model is read.
- Test flags for the window client: `--ride-along-person` rides along in
  the first aircraft a person flies; `--shot-ridden PREFIX` draws the shot's
  frame three times more with no HUD - the aircraft ridden in as the updates
  have it, with its surfaces still, and not drawn.

## Verification

- `the_client_with_the_window_draws_another_players_ailerons_over_as_she_rolls`
  (tests/cmake/client_sees_others_surfaces.cmake, port 24794,
  tests/tools/ridden_surfaces_check.cpp): a server for two players; a
  second player (`glideslope_cli connect --fly`) holds her Cessna's stick
  full left; the window client rides along behind her
  (`--ride-along-person --view behind`). At the shot it must say it drew her
  left aileron below -5 degrees and her right above +5, and the frame drawn
  as the updates have her must differ from the same frame with her surfaces
  still by ten pixels at least. Locally: left -20.00 and right +20.00
  degrees, her elevators -5.50; 1,705 pixels of her, 205 moved by her
  surfaces.
  - Seen red: the server writing every aircraft's surfaces as nought
    (`out.surfaces = {}` before the packet takes it) - "her ailerons were not
    drawn over the way the stick holds them, left +0.00"; and the check on
    that run's frames, "its surfaces were drawn still: 0 pixels moved by
    them". Reverted; green again, 205 pixels.
- `every_aircrafts_every_surface_reaches_another_client_where_its_flight_model_has_it`:
  each of the 16 aircraft, stick and pedals full one way then the other;
  what the server writes of it goes through a state update written and
  read and into the interpolation, and each surface its file names is
  turned by the deflection its property is carried as. Within a quarter of
  a degree of what was flown, and over by more than that. Counted against
  the catalogue as the livery item's test is (the B-2's rudder named left
  out there): 164 of 164.
  - Seen red: `surfaces_on_the_wire` writing nought - "0 of 164 surfaces
    drawn where flown". Reverted.
- `a_surfaces_deflection_goes_on_the_wire_to_the_nearest_half_degree`: all
  256 bytes read back as themselves; 2,401 deflections within a quarter of a
  degree, held at the ends.
- `a_velocity_goes_on_the_wire_to_the_nearest_sixteenth_of_a_metre_a_second`:
  all 65,536 values; 33,601 velocities within a thirty-second of a metre a
  second, held at the ends.
- `a_state_update_carries_the_ailerons_elevator_and_rudder_by_their_properties`,
  `another_aircrafts_surfaces_move_between_two_updates_and_hold_past_the_last`.
- The state packet's own tests, moved to the new layout: the round trip at
  every count from 0 to 20; every one-bit change read exactly or refused; the
  NaN walk, 75 cases, of which the reader is given 66 - an aircraft's three
  velocities cannot hold one on the wire, named; a full packet, 1,119 bytes
  and 1,149 sealed, which TRANSPORT.md says and a test holds it to. The
  version tests (`15`), the gearstick refusal (`474c4453150401`), and the
  client written from the document, which completes a session.
- Built on Windows (windows-debug, MSVC) with tools/windows_build.sh.

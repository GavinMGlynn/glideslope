### Two server tests wait for what they are waiting for, 2026-09-24 — tails done

Both failed on CI's slow runners, on pull requests that had not touched what
they test.

**`a_clients_inputs_fly_its_aircraft_on_the_server`.**
- **Before:** the client flew for six seconds and then said how many of its
  inputs the server had applied. A debug server on a loaded runner was sixty
  behind at that moment, against the thirty the test allowed.
- **Now:** the client's time up, it stops making new inputs. It keeps resending
  the last and listening until the server's updates say it has applied it. It
  gives up after a minute, or after five seconds of silence from a server that
  has gone or dropped it. The server runs `--until-empty`. The test requires
  the server's newest applied input to be the last one sent, where it used to
  allow thirty short.
- **Seen to fail:** a server that recorded only odd-numbered inputs as applied
  went red, with 177 of 178.

**`ai_aircraft_keep_flying_with_nobody_connected_and_a_late_client_finds_them_mid_flight`.**
- **Before:** the late client waited five seconds from launch. A debug server
  on a Windows runner spent all five building its terrain, and the client found
  a clock of 0.067 s.
- **Now:** the server writes a file once its aircraft are built and flying
  (`--ready-file`, a new test flag). The client counts its five seconds from
  when that file appears (`--after-ready`).
- **Seen to work and to fail:** with a six-second sleep put into the server's
  start, the test passed. With the client made to ignore the file as well, it
  failed at a clock of 0.000 s.

`--fly` clients elsewhere now wait for their last input too. The window test's
client, dropped by the button, stops on the server's silence (8 s, where it had
been as long as 68 s).

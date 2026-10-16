### Four machines in one sky, 2026-09-25 — item done, and Phase 6 with it

**Flown.** `tools/four_machines.sh` was run from WSL. The server ran on
Windows, with room for four players and one AI Cessna. Four clients joined
it:

- from Windows, over 127.0.0.1: the client with the window on Direct3D 12,
  and a predicting `glideslope_cli`;
- from Linux, across WSL's network to the Windows host: the client with the
  window on Vulkan, and a predicting `glideslope_cli`.

A fifth asked from Linux once the four were in. The script checks every
figure below, and fails if any is wrong.

| client | given | drew | worst correction | worst prediction error |
|---|---|---|---|---|
| with the window, Windows | a Cessna | the other four | 2.8 m, none snapped | - |
| with the window, Linux | a Cessna | the other four | 5.1 m, none snapped | - |
| `glideslope_cli`, Windows | a Cessna | - | - | 3.3 m |
| `glideslope_cli`, Linux | a Cessna | - | - | 2.3 m |
| the fifth | refused, reason 5 (`SERVER_FULL`) | | | |

The server flew all five aircraft, and each client was told all five were
Cessnas. The run takes about a minute. It is not in CI, because it is two
operating systems on one network.

**What the review found**, all fixed before the merge:
- **The server's word did not win.** The client took the aircraft the
  server gave it, and then `--aircraft`, which defaults to the Cessna, wrote
  over it. Every test passed, because the server gives players Cessnas. The
  test now asks for an F-15C and must fly the server's Cessna. With the old
  order restored it goes red.
- A mesh was kept for every aircraft ever seen; one that leaves takes its
  mesh with it now.
- The server compares an aircraft's id as well as its model before
  introducing it again. It sends what must arrive before the update that
  names the aircraft.
- The network check counts different aircraft introduced, not lines.

**And what CI found.** On Rocky 9, the network check at 100 ms drew no frame
past the newest update, and failed for never having tested the guessing.
Whether any happened was left to random loss, and the new session clock
leaves fewer to chance. The relay now makes the situation: `--gap 200
--every 4` drops everything from the server for a fifth of a second every four
seconds. That is twice the 100 ms the others are drawn behind, so every run
carries aircraft across it: 102 and 170 frames here. The test also requires
the relay to have made its gaps. Its parts run in CI one machine at a time:
the network checks, and the client with the window on a server.

**What works now.**
- **`AIRCRAFT` travels.** It is keyed by the aircraft's number, not a slot's,
  because an AI aircraft has no slot. The server introduces every aircraft to
  each client as it is admitted, and to every client when one appears, over
  the reliable layer. Both the session library and `glideslope_cli`
  acknowledge it and keep a roster. The predicting client flies its own
  aircraft as the model the server names. `--predict` takes no model now.
- **The client with the window joins, and is given an aircraft.**
  - It connects before it builds its flight, and waits until the server has
    said which aircraft is its own and what aeroplane that is. The flight is
    built there and set to the server's motion.
  - From then on it predicts. It sends the stick thirty times a second, flies
    exactly what it sent, and reconciles on each newer update
    (`client/online.hpp`).
  - It draws every other aircraft with its own model, 100 ms behind the
    session clock. Each is placed at its centre of gravity, which is a few
    metres from where its own client draws it, and lit for how it is pointing.
  - A server with nothing to fly gives it no aircraft. After ten seconds with
    no word of one, the client says so and flies alone. Ten seconds is
    enough, because a server answers the handshake only once its aircraft are
    flying.
- **Online, a shot keeps real time and draws only its own frame.** Thousands
  of headless frames ran the software Vulkan driver out of memory. That is a
  tail. (Fixed 2026-09-26, see the log; the shot still draws only its own
  frame, which spares the session the machine's time.)

**Verified** by `the_client_with_the_window_flies_the_servers_aircraft_and_draws_the_others`.
- **The run.** A server with one AI Cessna, and the client with the window
  taking a shot ten seconds in.
- **What it must say.**
  - It was given a Cessna.
  - It drew exactly one other aircraft: the AI, under another number, 469 m
    away.
  - Its prediction made over a hundred corrections, the worst 2.3 m, none too
    large to hide.
- **Seen to fail.** With the client made to draw nobody else, the test went
  red.
- **Also changed.** `the_client_with_the_window_joins_the_server_server_txt_names`
  now also requires the client to say it was given no aircraft.

### The server's dashboard in a window, when asked for, 2026-09-24 — two items done

**`glideslope_server --window` draws its dashboard in an SDL window**; without
it the server runs in the terminal exactly as before, and `--headless` runs
with neither. The project owner asked for the window to be a choice made at
start: the server is to be hosted headless on AWS - EC2 on Rocky Linux 10, or
Fargate - so nothing about it may need a display unless asked.
`REQUIREMENTS.md` 6.6 is changed to say so.

- **One dashboard, two ways to draw it.** `src/frontend/server/dashboard.hpp`
  holds what the dashboard shows - the slots with their ping and traffic,
  what is flying and where, the last eight arrivals and departures (new; the
  terminal had none) - and `dashboard_lines()` is the text both draw. The
  terminal prints it; the window draws the same lines.
- **The window** (`window.cpp`) is SDL's renderer and its built-in 8x8 font
  at twice size - no font file, no widget library - with a red drop button
  beside every slot somebody is in. A drop lets the player go as a timeout
  does, through one `let_go()` both use: their aircraft removed or handed to
  an AI pilot as `--on-leave` says, and the slot freed. Closing the window
  stops the server.
- **No display library unless asked.** SDL is linked statically and loads a
  display library only when `--window` starts its video. On Linux only
  Wayland and X11 are tried: given neither, SDL went on to drive the console
  through Mesa, which failed deep inside and leaked rather than saying no.
- **The test flags gearstick's server has**: `--window-dump` prints every line
  the window drew next to the terminal's lines for the same facts,
  `--window-shot FILE` writes its last frame as a BMP, and `--window-press
  LABEL` presses a button the first time it is drawn, through the path a
  click takes. The dashboard's port is now the one bound, not the one asked
  for: `--port 0` showed 0.

**Verification.** `the_servers_window_shows_the_terminals_dashboard_and_its_drop_button_drops_a_player`:
a client joins and flies, the server presses `drop 0`, and the player is
dropped by the operator, slot 0 is open again, the log shows both, every line
the window drew is the terminal's in order, and the frame is 1120 by 720 with
its background in the corner - seen to fail with the window drawing one line
short, and with the drop doing nothing. `the_server_runs_with_no_display_and_refuses_a_window_there`
(Linux): with DISPLAY, WAYLAND_DISPLAY and XDG_RUNTIME_DIR taken away - without
the last, Wayland finds WSLg's compositor at its default socket - the server
runs, headless and with the terminal dashboard, and `--window` is refused
with the reason; seen to fail with a silent fall back. `the_server_links_no_display_library`
(Linux) holds the server to no X11, Wayland, GL or audio library; seen to fail
with libX11 linked. Four `--dry-run` tests refuse `--window` with `--headless`
or `--plain`, and the window's test flags without it. Leaks allocated entirely
in the display's own libraries are counted and named, as the client's tests
do; one with a frame in glideslope fails.

**What CI found in it, and was fixed the same day.** The client written from
`TRANSPORT.md` did not compile on Windows - its Winsock branch had never been
built, and windows.h's `min` and `max` macros broke `std::max` (`NOMINMAX`,
added with a note). Its "rolled to" was the roll in its last update, and an
aeroplane held over rolls on round: 46 degrees on CI after a whole turn - it
now reports the furthest it banked. On a slow runner the drop came on the
window's last frame, which the dump then showed from before it took effect:
the server now draws one last frame as it stops. macOS's runner has a small
screen and shrank the window to 1024 by 653: the server now says the size it
wrote, and the test holds the frame to that.

**Not yet proven: the server-only build.** `-DGLIDESLOPE_SERVER_ONLY=ON`,
which `deploy/Dockerfile` builds, has no SDL; there the window is
`window_none.cpp`, and `--window` is refused as "built without one". It was
compiled by hand against the project's headers and warning set, but no server-
only build has been made - nothing here or in CI builds that configuration,
and the Dockerfile has never been built (the Deployment item). Building the
image is what will prove it. *Since proven: "Deployment: the container image
built and serving", above.*

### The window client draws another player's chosen aeroplane as that aeroplane, 2026-10-07 — tail done

**What was missing.** A player could ask for an aeroplane and fly it, and
every client was told what it was, but nothing showed the client with the
window drawing it so: its shot named each other aircraft by the id it had
been told, not by the model it drew.

**What changed.** The shot's `drew aircraft N, the X` now names the model
the aircraft's mesh was made from (`OtherModel::id`), or says there is no
model, so a mesh made from the wrong aeroplane cannot be reported as the
right one. `client_on_server.cmake` takes `-DOTHER_PLAYER=ON`: a second
player, `glideslope_cli connect --aircraft pa28`, joins beside the window
client and the AI's Cessna and stays until the shot is written
(`--until-exists`, the event).

**Verification**:
`the_client_with_the_window_draws_another_players_chosen_aeroplane_as_that_aeroplane`:
the window client must say it drew two others, the PA-28 and the AI's
Cessna, neither its own number, and pass the plain test's prediction
bounds. Seen to fail: with every other aircraft's mesh made from the
Cessna's model, it drew aircraft 4 as the c172p and failed; reverted, it
passes (59 s, sanitized debug).

### The windowed client's `--loading` flown by a test, 2026-10-22 — item done

**What is still not true first.** The tests read the weight her flight
*begins* at, not what she weighs later in it: fuel burnt is the model's, as
it is for every aircraft. They draw on Vulkan alone, as the client's other
server tests do, and skip (exit 77) where it cannot draw. A take-over is
built in one direction only - from a named loading to an AI F-35B at her
model's own - because a server gives the AI no loading and refuses a take-over
of a player's aircraft, even one the AI flies (server_take_over.cmake): the
client code is the one comparison (`taken->loading != start.loading`) either
way.

**What changed.** The client with the window says what she weighs as each
flight of hers begins - `glideslope: the f35b's flight begins at 37530 lb,
loaded vertical_landing` (or `loaded as her model is`) - from her model's
`inertia/weight-lbs` once she is initialised (src/frontend/client/flight.cpp),
as glideslope_cli says what its prediction weighed. Nothing else in the
client changed.

**Verification** (linux-release, WSL, Vulkan; `tests/cmake/client_loading.cmake`).
Each case reads the figure from `figures/f35b.xml` in the data the client
reads, and first flies her alone at her model's own loading, which must not
be the figure (45,400 lb), so a loading not loaded is seen:

- `the_client_with_the_window_alone_at_her_vertical_landing_loading_flies_her_at_its_weight`:
  `--aircraft f35b --loading vertical_landing`, no server: begun at 37,530 lb.
- `the_client_with_the_window_asking_a_server_for_her_vertical_landing_loading_is_told_it_and_flies_her_at_its_weight`:
  the same asked of a server with one AI Cessna: begun at 37,530 lb, loaded
  as the server's `AIRCRAFT` says (on a server its word wins).
- `the_client_with_the_window_at_a_loading_taking_over_an_f35b_loaded_otherwise_flies_her_again_at_that_loading`:
  the same, on a server whose AI flies an F-35B (a plan written by the test,
  north from Sydney at 5,000 ft and 300 kt); the client rides along and takes
  it over six seconds in. Took over aircraft 4, the f35b; her flights began
  37,530 lb loaded vertical_landing, then 45,400 lb loaded as her model is.

**Seen red**: with the client's `aircraft_->load(...)` removed, all three
fail (45,400 lb where 37,530 was wanted); with the take-over's
`|| taken->loading != start.loading` removed, the take-over case fails with
one flight begun, not two. Both reverted; the three and
`the_client_flies_the_aircraft_it_is_given_in_the_air_or_on_the_ground_on_vulkan`
green after.

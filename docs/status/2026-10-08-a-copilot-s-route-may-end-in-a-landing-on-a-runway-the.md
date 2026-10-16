### A copilot's route may end in a landing, on a runway the server knows; protocol 10, 2026-10-08 — closes the runways tail

**What it is not, first.** **No language model has yet been seen to land
one**: the copilot is told it may (`land`, last), and the re-recorded
copilot flights below were asked the same questions as before, none of
which asks for a landing; the landing flown end to end is a route file
given to an AI aircraft (`--ai-route`), not a model's answer through a
player's client - the same checks and flying (`fly_route_on`), but not the
same way in. A glide does not land: with the engine stopped the copilot
still orbits over its field. The flight planner, which plans from the
ground, is not offered `land` (a new tail).

**What changed.**
- **Protocol 10 (`0A`)**: `COPILOT_ROUTE` ends with a landing flag and, for
  a landing, the runway's name, threshold, elevation, heading and length
  (`net::RouteLanding`); TRANSPORT.md byte for byte (1,011 bytes at its
  limits), THREATS.md says what a landing is held to; `doc_client`'s
  version moved, and the version test now writes it in hex.
- **The copilot** (`copilot/copilot.cpp`) is told `land NAME LATITUDE
  LONGITUDE ELEVATION_FT HEADING_DEG LENGTH_M`, last, copying a runway line
  it was given, only with the engine running. `change_refusal` refuses a
  landing on none of the runways it was told of (`copilot::landing_field`:
  a threshold within 100 m and a heading within 5 degrees), a glide's, one
  for an aircraft with no approach speed, and one after an orbit flown for
  ever; `read_change` refuses `land` anywhere but last, and a landing name
  not a route's word.
- **The server** checks a route's landing against the collision ground's own
  runway ends within a kilometre of its threshold (`ends_near`) by the same
  `change_refusal`, and lands it on that end's threshold, heading and
  length and the ground's elevation - not the route's numbers. The hand-over
  planner's routes carry their landing too. `--ai-route FILE` gives the
  first AI aircraft a route file as a player's copilot's route is, for a
  test that flies in simulated time; the route file reader is shared with
  `glideslope_cli --send-route` (`frontend/route_file.hpp`).

**Verification.**
`a_copilots_route_that_ends_in_a_landing_is_landed_and_leaves_the_runway`:
a server with one AI C172P over Chatswood, given a route of one waypoint and
`land YSSY_16R`, flies it, lands on YSSY 16R by the learnt landing and
leaves the runway, stopped beside it, unwrecked, in 90,000 steps (52 s,
linux-debug). **Seen to fail** with `ends_near` finding no runway: "the
server did not fly the route to its landing ... is refused the route: the
landing YSSY_16R is on none of the runways nearby"; reverted. (With that and
the copilot's field check both broken the server dereferenced no runway; it
now refuses one it cannot find, whatever the check above said.)
`a_copilots_route_landing_on_no_runway_the_server_has_is_refused`: a landing
on a made-up runway is refused, saying so.
`a_copilots_route_may_end_in_a_landing_on_a_runway_it_was_told_of_and_on_no_other`
(unit): the copilot is told the runway and how to land on it; a route ending
on it is taken, on that runway; eight refusals, each built. **Seen to fail**
with the field check removed from `change_refusal`: "`land YSSY_16R
-33.939 151.172 ...` is refused, saying "none of the runways": it was
taken"; reverted. The message tests walk the landing:
its round trip, its 32-byte name at the datagram's limit, its five numbers
against NaN and infinity (32 fields now, 27 before), and its flag and name
refusals (15 now).
**Every copilot recording re-recorded** (the copilot's instructions
changed, and a recording plays back only the words it recorded): the ten in
`tests/data/copilot/` the copilot is asked through - manly, engine,
engine_thinking, take_back, leave and take_over on a server, hand-over by
Claude and ChatGPT, the coast and the engine on the command line - each
recorded by its `..._now` test with the owner's keys (scanned: no key in
any) and played back green. Two needed more: **take_back**'s second answer
was `keep` again, written in as a route as before, its `note` saying so;
and **ChatGPT's hand-over** answered an orbit of Bondi round which she
already flew, so `server_hand_over_model.cmake`'s "came no nearer its
waypoint" now also takes an aircraft flying the circle: both half-minute
lines within a quarter of the orbit's radius of it and ten degrees or more
further round it - the server's line says an orbit's radius and the bearing
of her from its centre. **Seen to fail** with that bearing held at nought,
as an aircraft held still would be: "the aircraft came no nearer its
waypoint"; reverted. **Every recording was scanned for a key** (`sk-`,
`x-api-key`, `Bearer`, and the first characters of each key in
~/.config/glideslope): none holds one; a recording writes no header. The
first try with the new instructions had Claude, its engine stopped, answer
a glide ending in `land`, refused three times; the instructions now say a
glide never ends in `land`, and the second recording did not.

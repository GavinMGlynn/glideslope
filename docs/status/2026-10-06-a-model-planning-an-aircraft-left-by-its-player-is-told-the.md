### A model planning an aircraft left by its player is told the plan it flies, 2026-10-06 — tail done

**Left by a player who goes** (`--on-leave ai`), an aircraft flies the
server's plan file until the server's model answers - and the model was told
"no route is flown: the autopilot holds what the aircraft is doing". The
server's situation for a hand-over (`situation_of`) now gives the route the
aircraft's navigator is flying: its waypoints still to fly, from the one it
is flying to (`sim::Navigator::still_to_fly`), as heights stacked for it.
An aircraft left by a take-over holds its course and has none, as before.

**Told the plan, Claude keeps it.** Asked live three times
(claude-haiku-4-5-20251001), it answered keep each time, where told no
route it had given one. So the server now says what a keep keeps:
"aircraft N's model answered keep: it flies on to THE_HEADS, 1 of 4" - or
"it holds its course" with no plan.

**Why the recording went from two exchanges to one.** The old one held a
refusal and a retry: told no route was flown, Claude first answered a glide
to Sydney airport with the engine running, which `change_refusal` refused
("a glide is only for an engine that has stopped"), and its second answer,
a route to orbit the airport, was flown. Told the plan, it answered keep at
once, which needs no checking against a route and is taken on the first
answer - so one exchange.

**The recording is made again**: `tests/data/copilot/leave-server-anthropic.jsonl`,
asked of Claude by `..._asking_anthropic_now` with GLIDESLOPE_LIVE_MODEL=1,
scanned for keys (none). `cmake/server_hand_over_planner.cmake`, case leave,
now requires the recording's question - which the server's matched, but for
its numbers - to hold "The route being flown" with the plan file's last
waypoint, AIRPORT, and not "No route is flown"; and, the answer being keep,
that the aircraft flies on to one of the plan file's four waypoints. A route
answered instead would still be checked as before. Verified:
`an_aircraft_left_by_a_player_who_goes_is_planned_in_the_air_by_the_servers_claude_as_recorded`
green, and red with the route taken out of the situation again ("exchange 1
was recorded for another request"); the take-over and no-key cases green
and unchanged.

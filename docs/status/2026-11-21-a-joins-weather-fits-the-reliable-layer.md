### A join's weather fits the reliable layer: past 256 on the wire, messages wait their turn, 2026-11-21

**What it does not do, first.** The reliable layer is a fixed window, not
congestion control: it does not shrink when datagrams are lost, and a sender
still refuses past `net::most_waiting` (4,096 queued), which the server
still says on its standard error. No multi-process test joins a real server
with 32 stations each mid-blend; the join is exercised by the unit test
below, over the loopback to a real `net::ClientSession`, with the bodies
`frontend::weather_said_on_joining` makes.

**What was wrong.** `net::Reliable::send` refused once 256 messages were
unacknowledged (`most_in_flight`). A client joining 32 stations each
blending from a few reports is told more than that - four weathers a slot,
each a `WEATHER` and a `WEATHER_ALOFT`, is 256 before the runways - so the
server's queue refused the rest, said so on its standard error, and the
joiner flew other air.

**The fix** (`src/net/reliable.hpp`, `.cpp`).
- `send` numbers and keeps up to `most_waiting` (4,096) messages.
- `to_send` puts on the wire only the first `most_in_flight` (256) not yet
  acknowledged; the next go out as acknowledgements let the first go. So a
  receiver, which holds 256 out of order (`most_held_back`), never has one
  of them dropped - a `static_assert` holds `most_in_flight <=
  most_held_back`.
- `held_to_send()` says how many are waiting off the wire.
- No byte of any datagram changed, nor the version. `docs/TRANSPORT.md`
  ("Reliable messages") describes the window; `docs/THREATS.md` the new
  bound on the sender's queue (about 4.8 MiB a peer at worst).
- The stand-in server of the unit tests (`tests/unit/stand_in_server.hpp`)
  now hears its client's acknowledgements (`hear_reliable`), as the server
  does; without them a sender holding more than 256 never sends the rest.

**Verification** (linux-debug):
- `a_client_joining_32_stations_each_mid_blend_over_several_reports_flies_the_servers_air`
  (test_same_air.cpp): 32 stations (`world::most_stations`) found at 0 s,
  each given new reports at 30, 60 and 90 s, blending over 120 s; joined at
  95 s, each slot's chain checked three deep, the join checked to be more
  than `most_in_flight` messages - 288 messages, 32 of them held off the wire at first. Sent through the reliable
  layer over the loopback to a real `net::ClientSession` and taken as the
  window client takes it; every slot told its four weathers; the joiner's
  air compared with the server's at 84 places (every station, and between
  each pair of neighbours), 3 heights and 8 times from 95 s to 400 s:
  2,016 of 2,016 points the same, to the last bit.
- `more_than_a_reliable_sender_puts_on_the_wire_at_once_arrives_whole_and_in_order`
  (test_reliable.cpp): 785 messages queued at once, the first datagram of
  each round lost the first time it carries its message, every
  acknowledgement lost for five rounds - all 785 arrive once and in order in 12 rounds, 4 datagrams lost; at most 256 on the wire unacknowledged, at most 255 held back by the receiver.
- `a_reliable_sender_refuses_once_too_many_are_waiting` now holds the
  refusal at 4,096, and that only the first 256 go on the wire.
- The window client's mid-change join test (2026-11-21) now runs through
  the same helper, acknowledgements heard: 150 of 150 points the same.

**Seen red.** With `src/net/reliable.cpp` put back as it was on `main`, both new tests were built and run and failed: the reliable one at "message 256 queued", the 32-station join at "every message queued: 256 of 288". The fix was then put back, and both pass.

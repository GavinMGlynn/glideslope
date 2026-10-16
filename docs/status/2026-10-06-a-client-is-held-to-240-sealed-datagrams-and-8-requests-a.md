### A client is held to 240 sealed datagrams and 8 requests a second, 2026-10-06 — closes a tail (REQUIREMENTS 6.2)

**What is still not limited first.** Nothing limits how often an address may
send *before* it has a session - initiations to a server with a free slot,
and garbage - beyond the 32 initiations a second a full server reads
(`THREATS.md`); and an input sequence far ahead of the server's is not
refused. Those stay where `THREATS.md` names them.

**What it does now.** Each session on the server has two token buckets
(`net::Budget`, `net/budget.hpp`), each with a second's worth at once:
- **240 sealed datagrams a second** (`session_datagrams_per_second`), taken
  after the datagram opens - a forger writing the client's address on
  garbage spends nothing of it - and before anything inside is acted on: one
  past it is dropped unread (no input, no `PONG`, no acknowledgement). A
  `LEAVING` is never dropped. Eight times the clients' 30 inputs a second.
- **8 requests a second** (`session_requests_per_second`): each reliable
  message a client sends is acknowledged as before, and past the budget
  ignored - no swap, take-over, route or watch.
- The server says, as it lets a session go that went past either, "ADDRESS was
  held to its rates: D of N sealed datagrams dropped past 240 a second, R of
  M requests ignored past 8 a second".
- `glideslope_cli connect --flood` (with `--fly`): once the server has applied
  one of its inputs, it sends 1,000 sealed `PING`s at 1,000 a second, then 50
  `WATCH` requests at once, knocks with one more token every quarter of a
  second until that is answered, and leaves when its requests are all
  acknowledged, saying on standard error how many pings were answered and
  over how long they went.
- `docs/TRANSPORT.md` ("Rates", and "What is not here yet"), `docs/THREATS.md`
  (the swap, take-over, route and input sections, and the list of what is
  owed) and REQUIREMENTS section 9 state the rates. No byte on the wire
  changed, so no version bump.

**Verification.** `a_client_sending_faster_than_its_stated_rates_is_held_to_them`
(`tests/cmake/server_rate_limits.cmake`, port 24802): `glideslope_cli connect
--fly --flood` | server `--until-empty`. It reads the two rates out of
`net/budget.hpp`, and holds that the client had at most a second's worth plus
the rate times the time it sent them of its pings answered (458 of 1,000 over
999 ms here, against 481), that it sent faster than that, and that the
server said it held the session to those rates, dropping some datagrams and
taking at most 9 of the 50 requests (8 here). 4.3 s (Linux debug). **Seen to
fail two ways**: with the datagram budget never taken, "1000 of 1000 pings
sent over 1000 ms were answered, more than the 481 its rate of 240 a second
allows"; with the request budget never taken, "the server took 50 of 50
requests sent at once, more than 9". Both reverted. The other 35 tests that
run a server and the command-line client pass with the limits in.

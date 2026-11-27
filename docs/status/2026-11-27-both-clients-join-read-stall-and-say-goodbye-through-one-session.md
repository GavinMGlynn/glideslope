### Both clients join, read, stall and say goodbye through one session, 2026-11-27

**What changed.** The command-line client (`glideslope_cli connect`) used
to have its own copy of the client's half of a session. It did its own
handshake, read its own updates and its own reliable stream, knocked, stalled
(`--stall-once-rolled`) and said goodbye. Only joining again, knocking and
believing a refusal were shared (2026-10-08). Now it has no copy. It runs on
`net::ClientSession`, the session the window client uses:

- **The handshake** is `ClientSession::join`, which takes the address, the
  server's key, this client's key pair and what it asks (the payload, how
  long to wait, how often to resend). It says how it failed: no socket,
  cannot send, refused (with the reason), the answer did not open, or no
  answer. `connect` now wraps it, and the window client still calls
  `connect`. `--first-from-elsewhere` is a `before_sending` hook that is
  handed the initiation before it first goes out.
- **Reading** is `read_one(now)`: one datagram, handled the way `poll` would
  handle it. A session told `keep_in_order()` hands what arrived back to the
  caller in the order it came, through `take_arrived()`: each state update,
  each delivered reliable message and each `PONG` token. The command-line
  client keeps reading one datagram per pass, which its prediction figures
  (read-dry, put right, the gap breakdown, slip counting) depend on. `poll`
  is now `read_one` until the socket is dry, then `tend` (knocks, proving
  pings, acknowledgements).
- **Knocks, acknowledgements and the reliable stream** belong to the session
  (`tend`, `send_message`, `messages_in_flight`). Joining again is the
  session's own as well: the CLI waits on `standing()` and reads
  `went_back()`, `joined_again()` and `stale_while_joining_again()`. It
  prints the same lines as before, which the tests read.
- **The stall** is the session's `stall_until_let_go`.
- **The goodbye** is the session's `leave()`. `end_without_goodbye()` covers
  `--no-goodbye`, a stay of no seconds and `--again-when-let-go`. A client
  that is finishing calls `leaving_soon()`, so it believes no refusal, as
  before.

The test-only flags that forge, replay or listen raw (`--forge-leaving`,
`--again-from-elsewhere`, `--again-when-let-go`) use the session's test
tools: `sealed`, `send_from_here`, `initiation`, `first_answer`, `server`
and `socket_for_a_test`. Nothing changed on the wire.

**Verification.**
`both_clients_join_read_stall_and_say_goodbye_through_one_session` checks
three things:
- Every one of the clients' 30 first-party sources (`src/frontend/cli`,
  `src/frontend/client` and the shared frontend code, comments stripped)
  is free of all 10 pieces a session is made of: `Initiator`, `Sealer`,
  `Unsealer`, `Reliable`, `Rejoin`, `lets_go`, `refusal_from`,
  `sealed_knock`, `read_state` and `is_leaving`.
- The window client joins by `ClientSession::connect` and the command-line
  client by `ClientSession::join`.
- Every test named for joining again, going back, being let go or goodbye
  runs one of the two clients. There are 16, out of every test in
  `tests/CMakeLists.txt`, and the count is asserted. Left out by name: this
  test, and `the_server_refuses_lost_goodbyes_without_a_drop`, which tests a
  flag refused before anything connects.

The 16:
- `glideslope_cli`:
  - `a_copy_of_an_initiation_arriving_after_its_session_was_let_go_makes_no_second_player`
  - `a_client_stalled_past_the_timeout_joins_again_by_itself`
  - `a_client_refused_by_a_forger_while_its_session_is_quiet_goes_back_to_it`
  - `a_client_let_go_with_its_last_updates_held_joins_again_without_going_back`
  - `a_client_slower_to_prepare_than_the_servers_timeout_is_not_let_go_and_seeds_its_law_as_the_server_does`
  - `a_prediction_given_a_nan_is_let_go_of_and_its_client_flies_on_from_the_servers_state`
  - `a_player_started_again_on_a_full_server_flies_again_at_once`
  - `a_player_the_operator_drops_is_let_go_from_every_address_their_key_is_at`
  - `a_client_that_says_it_is_leaving_is_let_go_at_once`
  - `a_goodbye_from_another_address_or_session_lets_nobody_go`
- the window client:
  - `the_client_with_the_window_stalled_past_the_timeout_joins_again_by_itself`
  - `the_client_with_the_window_dropped_by_the_operator_says_so_and_does_not_join_again`
  - `the_client_with_the_window_dropped_with_its_goodbyes_lost_is_refused_joining_again_and_says_so`
  - `the_client_with_the_window_joins_its_server_started_again_and_flies_the_aircraft_it_gives`
  - `the_client_with_the_window_refused_by_a_forger_while_its_session_is_quiet_goes_back_to_it`
  - `the_client_with_the_window_away_past_the_servers_timeout_building_its_flight_or_in_a_frame_is_not_let_go`

**Seen to fail.** The source check was run against the command-line client
as it was before this change. It failed, naming all 10 pieces in
`src/frontend/cli/main.cpp`. The count check failed at 17 counted against 16
found, and listed the 16. With the session's goodbye sending no copies, tests
from both clients failed: the command-line client's
`a_client_that_says_it_is_leaving_is_let_go_at_once` ("a client was let go
for its silence, after the two minute timeout, so its goodbye was not
heard"), and the window client's
`the_client_with_the_window_joins_the_server_server_txt_names` ("let go for
its silence"). The bug was reverted.

Before the full suite ran, the 171 tests matching the session's names
(joining again, going back, being let go, stalls, goodbyes, initiations,
forgers, layouts, refusals, the flood and stale updates) passed on
linux-debug.

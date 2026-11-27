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
  prints the lines the tests read as before. One line is gone: "the session
  still answers; stalling again", which the old stall printed when a datagram
  opened while it waited, and no test read. The session's stall throws away
  what it hears without opening it.
- **The stall** is the session's `stall_until_let_go`.
- **The window client's half is unchanged**, except that a session kept in
  order keeps nothing for `told()`, `take_air()`, `take_refusals()`,
  `take_learnt_refusals()` or `take_instructions()`. The caller hears each
  message itself, and those lists would otherwise grow without end.
- **The goodbye** is the session's `leave()`. `end_without_goodbye()` covers
  `--no-goodbye`, a stay of no seconds and `--again-when-let-go`. A client
  that is finishing calls `leaving_soon()`, so it believes no refusal, as
  before. **The command-line client's error exits now say goodbye**: the
  session's destructor calls `leave()`. These are the exits after the
  session was made, such as a server gone quiet while it waited to send.
  Before, they went silent and the server's timeout let them go.

The test-only flags that forge, replay or listen raw (`--forge-leaving`,
`--again-from-elsewhere`, `--again-when-let-go`) use the session's test
tools: `sealed`, `send_from_here`, `initiation`, `first_answer`, `server`
and `socket_for_a_test`.

**What changed on the wire**, all of it the command-line client's, now
doing what the window client already did:
- Until anything opens under a session, a sealed `PING` goes every quarter
  of a second (`tend`). Before, it sent its inputs, or a `PING` after a
  second of quiet.
- Its error exits after the session was made say goodbye, as above.
- An input packet with nothing in it is not sent.
The messages themselves are unchanged.

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
- Every test named for joining again, going back, being let go
  (`let_go`, `lets_go`, `lets_none_go`...), leaving, being dropped or
  forged, or goodbye runs one of the two clients. There are 22, out of the
  242 tests whose names are written out in `tests/CMakeLists.txt`, and the
  count is asserted. Two are left out by name, and the list is asserted to be
  exactly those, so a test that runs no client cannot drop out unnoticed:
  this test, which runs no client, and
  `the_server_refuses_lost_goodbyes_without_a_drop`, a flag refused before
  anything connects. The names built in loops were read by hand and none is
  of these kinds. The unit tests of `net::ClientSession` and `net::Rejoin`
  are the session itself, and are not counted.
- `a_join_that_comes_to_nothing_says_how` (unit) checks each way a
  handshake can fail against a stand-in server: held back, refused
  (`WRONG_LAYOUT`), an answer that does not open, and no answer.

The 22:
- `glideslope_cli`:
  - `a_copy_of_an_initiation_arriving_after_its_session_was_let_go_makes_no_second_player`
  - `a_client_stalled_past_the_timeout_joins_again_by_itself`
  - `a_client_refused_by_a_forger_while_its_session_is_quiet_goes_back_to_it`
  - `a_client_let_go_with_its_last_updates_held_joins_again_without_going_back`
  - `a_client_slower_to_prepare_than_the_servers_timeout_is_not_let_go_and_seeds_its_law_as_the_server_does`
  - `a_prediction_given_a_nan_is_let_go_of_and_its_client_flies_on_from_the_servers_state`
  - `a_player_started_again_on_a_full_server_flies_again_at_once`
  - `a_player_the_operator_drops_is_let_go_from_every_address_their_key_is_at`
  - `a_player_the_operator_drops_is_told_and_kept_out_for_the_rest_of_the_run`
  - `the_servers_window_shows_the_terminals_dashboard_and_its_drop_button_drops_a_player`
  - `a_server_lets_go_a_client_it_has_not_heard_from_for_its_timeout`
  - `a_server_behind_real_time_hears_every_client_and_lets_none_go_that_is_joining`
  - `a_player_leaving_has_their_aircraft_removed_or_flown_on_by_an_ai_as_the_session_says`
  - `a_client_that_says_it_is_leaving_is_let_go_at_once`
  - `a_goodbye_from_another_address_or_session_lets_nobody_go`
- the window client:
  - `the_client_with_the_window_stalled_past_the_timeout_joins_again_by_itself`
  - `the_client_with_the_window_dropped_by_the_operator_says_so_and_does_not_join_again`
  - `the_client_with_the_window_dropped_with_its_goodbyes_lost_is_refused_joining_again_and_says_so`
  - `the_client_with_the_window_joins_its_server_started_again_and_flies_the_aircraft_it_gives`
  - `the_client_with_the_window_refused_by_a_forger_while_its_session_is_quiet_goes_back_to_it`
  - `the_client_with_the_window_away_past_the_servers_timeout_building_its_flight_or_in_a_frame_is_not_let_go`
  - `the_client_with_the_window_lets_go_of_a_prediction_given_a_nan_and_flies_on_from_the_servers_state`

**Seen to fail.** The source check was run against the command-line client
as it was before this change. It failed, naming all 10 pieces in
`src/frontend/cli/main.cpp`. The count check failed when the stated count
did not match, and listed what it found. The left-out check failed in two
ways: with `the_server_refuses_lost_goodbyes_without_a_drop` taken off the
list ("runs neither client"), and with a name added that no test has
("named as left out but not found"). With the session's goodbye sending no copies, tests
from both clients failed: the command-line client's
`a_client_that_says_it_is_leaving_is_let_go_at_once` ("a client was let go
for its silence, after the two minute timeout, so its goodbye was not
heard"), and the window client's
`the_client_with_the_window_joins_the_server_server_txt_names` ("let go for
its silence"). The bug was reverted.

On linux-debug, the 324 tests matching the session's names passed: joining
again, going back, being let go, leaving, stalls, goodbyes, drops,
initiations, forgers, layouts, refusals, the flood and stale updates. A full
local run was stopped at 127 of 2227 tests, none failed, to free the
machine. The PR's full CI run covers the whole suite.

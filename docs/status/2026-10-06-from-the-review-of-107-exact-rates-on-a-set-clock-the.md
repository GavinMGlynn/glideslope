### From the review of #107: exact rates on a set clock, the payload fuzzed and refused through a server, the catalogue read once, the engine named, a ground test that waited on a time, 2026-10-06 — fixes

**What is still open first**: as below each entry; nothing new.

- **The rates are pinned on a clock the test sets**, not by the socket test:
  `a_session_is_held_to_its_stated_rates_on_a_clock_the_test_sets` (unit)
  holds both budgets to a second's worth at once, half that half a second
  later, no more than a second's worth after a long quiet, and none refused
  at the rate for a minute. **Seen to fail** with the bucket's cap taken
  out ("half a second later, half a second's worth"); reverted. The socket
  test now holds only what a slow machine cannot change - some pings
  unanswered, some datagrams dropped, 8 or 9 of 50 requests taken - since
  its old bound on the client's own sending time could flake when the server
  read late.
- **The asked-aircraft reader is fuzzed** (`test_fuzz.cpp`: sixteen parsers,
  twenty-one seeds, one an initiation's payload), and **an oversized payload
  goes through a real server's handshake** in
  `a_player_who_asks_for_an_aeroplane_when_joining_flies_it_and_every_client_is_told_so`:
  a fourth client (`glideslope_cli connect --asked-payload HEX`, a test
  flag, the payload unchecked) sends a length of 33; the server says "asked
  for an aeroplane in a payload that does not read: it flies c172p", and
  every client is told of four Cessnas.
- **The catalogue is read once**, when the fleet is made (`Fleet::catalogue_`),
  not from the disk at each admission; `THREATS.md` says a stranger's choice
  of aeroplane sets what its admission costs.
- **The state update names the engine stopped** (version `04`, still): a
  byte after the condition, the first stopped engine's number when the
  condition is `02` and `FF` exactly when it is not; anything else makes the
  update unreadable. 52 bytes an aircraft, a full update 1,136 (1,166 with
  envelope and seal). `sim::Prediction::hear_engine_stopped` stops the engine
  named, starts again one it stopped for an earlier word that names another
  or none, and ignores an engine it does not have. Pinned by
  `a_state_packet_carries_a_wreck_and_refuses_a_condition_it_does_not_know`
  (every engine byte under every condition: 768 walked) and the prediction
  unit test, now on the Mosquito's two engines (the second named, then the
  first). The document's client reads the byte.
- `tests/doc_client/doc_client.cpp`'s note of the version says `04`.
- **The ground test waited on a time.** `a_client_told_the_servers_collision_ground_refuses_other_ground_and_leaves`
  failed on macOS CI with "the exit codes were 1;1;0": the client on the
  same ground stayed five seconds and a slow server had said nothing - no
  ground, no knock answered, so exit 1. **Reproduced** with the server held
  to three seconds a step (`--test-step-ms 3000`): exit codes 1;1;0 once,
  and once the client leaving untold. Now that client stays until it has
  been told the ground and answered a knock (`--until-told-ground`), and no
  stay of `glideslope_cli connect` is over before it has answered one of
  the server's knocks, a minute past its time the most. With the server
  held so, the test passes. On failure it prints both clients' own words.
  **That made a second race likelier, and CI's macOS debug found it**
  (2026-10-07): leaving as soon as it was told, the client on the same ground
  emptied the server, which stopped (`--until-empty`) before the client on
  other ground's initiation was read - "no answer from 127.0.0.1:24725" after
  a minute. **Reproduced** by starting that client two seconds late. Now the
  client on other ground writes `--done` and the one on the same ground stays
  until it has been told, answered a knock *and* that file has appeared
  (`--until-told-ground` with `--until-exists` waits on both); the late start
  passes.

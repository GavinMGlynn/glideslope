### `LAW_STATE`: a client given an F-35B mid-flight predicts her from the server's law, 2026-10-22

**What is still not true first.**

- **No take-over or take-back of an F-35B in STOVL mode is tested through a
  server, because neither can be built yet.**
  - The server takes over only an AI's own aircraft. An AI's F-35B at her
    model's own loading is too heavy to hover, so she never flies her law,
    and a plan file names no loading.
  - A player's F-35B handed to the AI is flown on the plan, converted back,
    and her law let go before a take-back.
  - The server's code sends the message on a take-over or a take-back too
    (the client flies her as a person again), but nothing has seen it do
    so.
  - Taken back in STOVL mode with the law let go, the server's new law and
    the prediction's are each seeded from their own side: 0.858 degrees of
    pitch apart, seen while building the take-back test. A plan tail.
- **The windowed client is compiled, not run.** It waits for the message
  and begins on it, but no test runs it.
- **While it waits, the windowed client draws nothing new.** It keeps the
  flight it had until the message comes, rather than drawing her from the
  updates.
- **A wing-borne autopilot is made afresh.** The message carries only its
  pitch integral, the one number of its own measured to matter (2026-10-16).
- **Edge case:** a law begun by the player's own input between being given
  her and the first update is not told. The client sees no law in that
  update and makes its own, as before.

**The message** (protocol `10`, `LAW_STATE`, docs/TRANSPORT.md byte for
byte). The server sends it reliably, once, to a client it gives an aircraft
whose STOVL law already flies her: joining, joining again on its key, taking
her over or taking her back from the AI. It goes in the pass that sends the
first state update in which that client flies her as a person, and only if
that update shows a height asked.

It is self-contained (the design of Gaffer on Games' snapshot plus input
replay, and Unreal's network prediction: a full state tagged with its step,
and the client re-simulates its own inputs from there). It carries:

- the server's clock and the client's newest input applied;
- her own motion, as the state update carries it;
- the law's stage, flags, integrals and the bank held;
- the controls and lift system's commands it last gave her, and where the
  lift system stands;
- the lever and selector latches;
- in the hover, the hover law.

The size is 371 bytes, or 427 hovering.

A client given her with a height asked in its own motion does not predict
her until the message arrives. It then puts her prediction to that step and
flies its own inputs since forward from there, as `glideslope_cli`'s
`start` does from an update. So it never pairs the message with a state
update that may be lost or already used. THREATS.md names the message: the
server's alone, dropped if a client sends it, and refused when malformed.

**The code.**

- `net::LawState`, `write_law_state` and `read`, in net/state.cpp. Its own
  motion shares `write_own_motion` and `read_own_motion` with the state
  update.
- `Told` keeps the newest one.
- `frontend/law_told.hpp` turns it to and from `sim::Controller::LawState`.
  That type now carries the selector held at a take-back or take-over.
- The server: `Fleet::law_of`, and each client's `flying_as_given`.
- `glideslope_cli` waits for it and begins on it (`law_told`,
  `start_on_law`), and counts its errors afresh from then.
- The windowed client: `Joined::law`, `OwnWord::law`, `Online::fresh_law`
  and `Flight::adopt(motion, law)`.
- `glideslope_cli` gains `--stovl-level` and `--stovl-bank-at N` for the
  test.

**The tests.**

- `a_client_joining_an_f35b_mid_bank_predicts_her_from_the_servers_law_state`
  (tests/cmake/server_stovl_join.cmake, `join`). Client A flies her at her
  vertical-landing loading in STOVL mode and banks her jet-borne 70 s after
  selecting, leaving 20 degrees held. It leaves without a word at 95 s.
  Client B joins on A's key through a 100 ms relay and is told her law:
  stage 2, bank held 20.000 degrees. Over 750 updates, in two runs: her
  pitch within 0.017 and 0.024 degrees, the median 6 and 4 mm, the 95th
  percentile 8 and 6 mm, the worst 0.509 and 0.007 m. The bounds are server_stovl_vertical_landing.cmake's: half a
  degree, 60 mm, 3.4 m, 5.6 m.
- **A leaves on an event, not at 95 s** (`--leave-once-bank-held`): once
  her law holds the bank and the server has applied the input that
  centred the stick. Left at 95 s, the Windows runners had not finished
  the roll, and B was told 0 and 14 degrees, the server's then - timing,
  not a stale message. B must now be told exactly the stage and bank the
  server says it held at the step of the message, and the server's must
  be A's 20.000. Seen red with the server saying one degree more than it
  sent. Locally A leaves at 79.7 s; pitch within 0.032 degrees, median
  16 mm. The script writes its addresses in its commands, where
  test_ports.cmake reads them.
- **Seen red without the message**: the server not sending it and the
  client not waiting for it, so the client predicts from a law made
  afresh. Joining mid-bank: her pitch 2.784 degrees off, the median 246 mm,
  the 95th percentile 347 mm. Restored: green.
- `a_law_states_every_number_is_read_exactly_or_refused` (test_state.cpp):
  jet-borne and hovering read back equal, at 371 and 427 bytes. It builds
  121 refusals: every one the document names, and a NaN and an infinity in
  each of 55 numbers.
- The message tests now walk thirteen kinds: 169 pairs, 16,384 loss
  patterns, every truncation and byte change, the document and THREATS.md.
  Protocol version `10` is pinned in test_protocol, test_runway_ground, the
  doc client and server_gearstick.cmake.

**Verification** (linux-release): the multi-process join test green; the
message, state, protocol, transport, threats, doc-client, fuzz and gearstick
tests green; the in-process join test green. The windowed client builds but
is not run.

## Rebased onto main, 2026-10-23

Main's client prepares a STOVL landing's ground before joining, so B's first
sealed datagram became an input of its own: the scripted pilot's selector
off, which ended the law and the 20 degrees of bank the server held before it
told B her law (seen: "stage 2, bank held 0.000", the join test red). A
`--stovl-level` client now sends nothing until its prediction has begun
(`ready_to_send`), as `--stovl-at-once` and `--from-held` already did; the
join test is green again.

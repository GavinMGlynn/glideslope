### A client of another control layout is refused at the handshake, 2026-11-27

**What is still missing, first.** Only the count of controls in an input
frame is said. A build that keeps the count and moves or changes the meaning
of a control is not told apart, and no other message's layout is said at the
handshake: a build of another day may still read today's state updates as the
wrong length and say nothing more useful (docs/TRANSPORT.md, "What the
transport does not claim").

**What now works.** The initiation's payload begins with one byte: how many
controls the client sends in each input frame. That is
`net::controls_on_the_wire`, the constant the frame's own layout is built
from and which a test already ties to `sim::Controls::control_count`, so the
count is never written down by hand a second time (`net::write_joining`,
`net::read_joining`). The aeroplane and loading asked for follow it as
before. A server reading another count, or an empty payload that says none,
answers the initiation with a `REFUSAL` of the new reason `08`,
`WRONG_LAYOUT`, instead of a handshake answer. It does not admit the client,
and it says on its standard output what it was sent and what it reads:
`refused 3b8e1f0c from 127.0.0.1:50123: it sends 20 controls in an input
frame, and this server reads 21`. Before this, the server let such a client in
and dropped every input it sent as the wrong length.

The clients say the refusal by name (`net::refusal_name`):
`glideslope_cli connect` prints `refused, reason 8, WRONG_LAYOUT`, where it
printed only the number before, and so does its line for a refusal while
joining again. The client with the window prints `glideslope: HOST refused
this client, WRONG_LAYOUT` where it printed "cannot reach". `ClientSession::connect` now reports
the server's reason through an optional out-parameter. `net::Rejoin` ends on
`WRONG_LAYOUT`, as it does on `SERVER_FULL`, so a server started again on
another build is not knocked on for a minute.

**The version moved to `14` (20)**, because the payload's shape changed. A
peer still on `13` is refused at the envelope as `WRONG_VERSION`. The
collision ground is unchanged: rules 2, on the strips of 2026-10-01. The
client written from the document (`tests/doc_client`) sends its count of 21
and knows `WRONG_LAYOUT` by name. `glideslope_cli connect --controls N`, or
`--controls none`, is the test flag that makes a client of another layout.

**Verification.**

- `a_client_of_another_control_layout_is_refused_at_the_handshake_by_name`
  (tests/cmake/server_layout_refused.cmake, port 24727). One server receives
  four clients, one after another. The clients saying 20 controls, 22
  controls and none each hear `refused, reason 8, WRONG_LAYOUT`, and the
  server says for each what it was sent and that it reads 21. The client
  saying 21 is admitted, and it is the only admission.
- `a_clients_count_of_controls_goes_first_in_its_payload_and_is_read_back`.
  Every count of 256, with an aeroplane asked for after it and without (512
  payloads, asserted), is read back as written. An empty payload is read as
  saying no count.
- `the_transport_document_and_the_code_agree_byte_for_byte` now walks nine
  refusals, `WRONG_LAYOUT` among them. It checks each against the code's name
  for it, and checks every byte of 256 read as its own reason or as
  `UNKNOWN` (9 and 247, asserted).

**Seen red.** I removed the server's check by making its condition
`false && (...)`, kept the aeroplane read from the payload, and rebuilt the
server. The ctest test then failed in 4.3 s: "the client saying 20 controls
was not refused WRONG_LAYOUT". The server's output showed all four clients
admitted, to slots 0, 1, 2 and 0. With the check put back, the test passes in
2.3 s.

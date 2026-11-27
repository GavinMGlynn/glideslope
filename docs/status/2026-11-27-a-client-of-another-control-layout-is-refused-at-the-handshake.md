### A client of another control layout is refused at the handshake, 2026-11-27

**What is still missing, first.** The fingerprint covers the controls'
names and their order, and that each goes on the wire as an `i16`. It does
not cover a control's range or sense: one changed under the same name gets
through. No other message's layout is said at the handshake, so a build of
another day may still read today's state updates as the wrong length and say
nothing more useful (docs/TRANSPORT.md, "What the transport does not
claim").

**What now works.** The controls are now one table, `sim::control_fields`
in sim/aircraft.hpp, holding each control's name and an accessor into
`Controls`, in the wire's order. `as_list` and `from_list` walk that table,
and so does the prediction's name for a control that is not finite.
`sim::control_layout` is made from the same table at compile time: a 32-bit
FNV-1a over each name, then `:i16` and a newline. Its value is `03281B95`.

The initiation's payload now begins with that fingerprint, as a `u32`. The
aeroplane and loading asked for follow it, as before. To `src/net/` the
fingerprint is opaque (`net::write_joining`, `net::read_joining`): the
network includes nothing from sim, and `ClientSession::connect` takes the
fingerprint from its caller.

A server whose fingerprint is different, or that gets a payload shorter than
four bytes, answers with a `REFUSAL` of the new reason `08`, `WRONG_LAYOUT`,
instead of a handshake answer. It does not admit the client, and it says on
standard output what it was sent and what its own fingerprint is:
`refused 3b8e1f0c from 127.0.0.1:50123: its controls' layout is 6F1D0E22,
and this server's is 03281B95`. Before this change, a client with a different
count was let in and every input it sent was dropped as the wrong length.
A client with the same count in a different order would have been flown on
the wrong controls.

The clients now say the reason by name (`net::refusal_name`):

- `glideslope_cli connect` prints `refused, reason 8, WRONG_LAYOUT`, where
  it printed only the number before, and so does its line for a refusal
  while joining again.
- The client with the window prints `glideslope: HOST refused this client,
  WRONG_LAYOUT` where it printed "cannot reach".
- When the window client is refused while joining again, it now names the
  reason (`ClientSession::refused_by`). Before, it said "it is full" for any
  refusal.
- `net::Rejoin` ends on `WRONG_LAYOUT`, as it does on `SERVER_FULL`.

**The version moved to `14` (20)**, because the payload's shape changed. A
peer still on `13` is refused at the envelope as `WRONG_VERSION`. The
collision ground is unchanged: rules 2, on the strips of 2026-10-01. The
client written from the document (`tests/doc_client`) sends `03281B95` and
knows `WRONG_LAYOUT` by name.

The test flag is `glideslope_cli connect --layout WHICH`. WHICH is one of:
`none`; eight hex digits; `swap:I:J`; `without:I`; or `with:NAME`. Anything
else is refused with exit 2 and a message, before anything is sent.

**Verification.**

- `a_client_of_another_control_layout_is_refused_at_the_handshake_by_name`
  (tests/cmake/server_layout_refused.cmake, port 24727). One server takes
  five clients, one after another. Four of them each hear `refused, reason
  8, WRONG_LAYOUT`, and the server gives for each the fingerprint it was
  sent, or none, beside its own:
  - aileron and rudder swapped, the same count of 21;
  - yaw trim left out;
  - one control more;
  - no layout at all.

  This build's client is admitted, and it is the only admission. Twelve
  unreadable `--layout` values are each refused with exit 2 and a message.
- `swapping_leaving_out_or_renaming_a_control_changes_the_layouts_fingerprint`.
  Every one of the 210 swaps of two controls, the 21 omissions and the 21
  renamings gives a fingerprint other than `03281B95`. All three counts are
  asserted.
- `a_clients_layout_goes_first_in_its_payload_and_is_read_back`. Every value
  of each of the fingerprint's four bytes, with and without an aeroplane
  after it (2,048 payloads), is read back as written. Each of the four
  lengths short of a fingerprint is read as saying no layout.
- `the_transport_document_and_the_code_agree_about_the_inputs` now holds the
  document to the fingerprint's value, its bytes, and the table's names in
  order. `the_transport_document_and_the_code_agree_byte_for_byte` walks nine
  refusals, `WRONG_LAYOUT` among them. It checks each against the code's
  name for it, and checks every byte of 256 read as its own reason or as
  `UNKNOWN` (9 and 247, asserted).

**Seen red, twice.**

1. The earlier count-based version of the test was run with the server's
   check made `false && (...)`. Every wrong client was admitted, to slots 0,
   1, 2 and 0.
2. For the reorder case, the fingerprint comparison was cut so that only a
   missing layout was refused (`if (!joining)`). The test then failed on its
   first client in 4.4 s: "the client of layout swap:1:2 was not refused
   WRONG_LAYOUT". The server showed all five clients admitted. With the
   comparison put back, the test passes in 2.5 s.

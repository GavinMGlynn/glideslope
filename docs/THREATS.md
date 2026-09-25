# Threats

What is defended, from whom and how; what is deliberately not defended, and
why; and the order the work is done in. Every message the server accepts is
named below with its defence, or with why it needs none.

## What the server presently accepts

**Said first, because everything else here is conditional on it.**
`glideslope_server` binds a UDP port and reads what arrives. It parses the
envelope; it answers a `HANDSHAKE_INITIATION` with a `HANDSHAKE_RESPONSE` and
admits the key to a slot; it opens `SEALED` bodies under the keys that
handshake agreed and refuses one that fails its tag or falls outside the
replay window; and it lets go a peer it has not heard from for `--timeout`
seconds, giving the slot back. Anything else - anything malformed, any kind it
does not expect, a sealed body it cannot open - is dropped or refused, and
never reaches the simulation, because **nothing a client sends drives an
aircraft yet**.

So the defences described here are in one of three states, and every section
below says which:

| state | means |
| --- | --- |
| built | the code exists, a named test holds it, and the server runs it |
| unwired | the code exists and nothing calls it outside its test |
| not built | there is nothing, and the section says what would be needed |

The envelope reader, all seven message readers, the handshake, the sealing,
the replay window and the per-peer connection table are **built** and wired:
`src/frontend/server/main.cpp` runs all of them on every datagram. The seven
message readers are **unwired** - they are exercised by
`tests/unit/test_messages.cpp` and by the fuzzing seeds, and the server does
not yet carry a message inside a sealed body. Rate limiting, the cookie reply
under load, and every input range and rate check are **not built**.

## What is deliberately not defended

- **Cheating is not a threat this project has.** It is not a scored or
  competitive game: there is no rollback, no deterministic simulation and no
  server verification of a result (`REQUIREMENTS.md` 6.1). Nothing here
  defends a scoreboard, because there is none.
- **False state is defended by the shape of the thing, not by a check.**
  Clients send inputs, never state (`REQUIREMENTS.md` 6.2), so a client cannot
  claim to be somewhere it is not. **That is not the same as inputs being
  trustworthy**, and input range and rate checks are owed - see "Inputs"
  below.
- **Who holds a key is not this transport's business.** It authenticates a
  key, not a person (`docs/TRANSPORT.md`). One person holding four keys is
  four players as far as anything here can tell, and nothing stops them taking
  every slot on a server. An allowlist of keys per server would; none is
  planned.
- **Traffic analysis is not defended.** The envelope is in the clear, so
  anyone watching sees that two machines are speaking glideslope, which
  version and which kind of datagram. Only a `SEALED` body will be secret.
- **Nothing is defended against the machine the client runs on.** A player
  can read their own client's memory and see every other aircraft's position.
  With at most four players there is no interest management to hide anything
  behind (`REQUIREMENTS.md` 6.3).
- **Denial of service by bandwidth is not defended and cannot be here.** A
  flood large enough to fill the link is the network operator's problem. What
  is defended - or owed - is the far smaller flood that costs the server more
  than it costs the sender.

## Every message the server accepts

### The four datagram types

Every datagram begins with the same six bytes: four of magic (`GLDS`), one of
version (`01`), one of type. `read_envelope` in `src/net/protocol.cpp` checks
them in that order and returns the reason it refused.

**The magic is checked before the version**, so a gearstick client is told it
is another protocol rather than being told its version is wrong, which would
be true and useless. A datagram shorter than the six-byte envelope is
`TOO_SHORT`; a type outside the four is `UNKNOWN_TYPE`. Held by
`an_envelope_that_is_wrong_is_refused_with_the_reason_it_is_wrong`,
`every_truncation_of_a_datagram_is_refused_without_running_off_the_end` and
`no_single_byte_changed_anywhere_in_a_datagram_can_break_the_reader`. **Built
and wired**: `take()` in `src/frontend/server/main.cpp` calls `read_envelope`
on every datagram before it does anything else, and sends the reason it
refused back as a `REFUSAL`. **That reply is real now**, and it is the
amplifier this document describes below: seven bytes out for one byte in, to
whatever address the datagram claimed to come from.

**`HANDSHAKE_INITIATION` (`01`) is the one datagram a stranger may legitimately
send, and it is the least defended thing in this document.** It is **built and
wired**: `Responder::answer` in `src/net/handshake.cpp` reads it, the server
answers with a `HANDSHAKE_RESPONSE` and admits the key to a slot, and
`a_client_and_a_server_complete_a_session_over_a_socket` and the end-to-end
`a_server_lets_go_a_client_it_has_not_heard_from_for_its_timeout` hold it
through a real socket and a real server.

**The suite is `Noise_IK_25519_ChaChaPoly_BLAKE2b`, and not the `..._BLAKE2s`
that `REQUIREMENTS.md` 6.7 names.** libsodium, which that section chooses for
the primitives, provides BLAKE2b and no BLAKE2s at all; BLAKE2b is a hash the
Noise specification itself defines, so this is a real suite rather than an
invention, and the alternative was carrying hand-written cryptography in a
project that has none. The header of `src/net/handshake.hpp` says so, and says
the project owner has not ruled on the departure.

**What it costs is no longer hypothetical.** The server does an X25519
operation on a datagram from an address it has never heard of, before it knows
whose key is talking: that is the cost an attacker can impose for the price of
one small datagram, and it is being paid on every port this program binds. IK's
first message carries the initiator's static key encrypted to the server's, so
the server does learn which key is talking before it commits a slot to it, and
`a_stranger_on_the_port_cannot_complete_a_session` holds that a sender using
the wrong server key gets no session at all.

**One thing bounds that cost, and it is not a rate limit.** `take()` refuses
with `SERVER_FULL` before it constructs a `Responder`, when the session is
full and the address is not one it already has a connection for - so a full
server does no asymmetric work for a stranger. A server with a free slot does.
**A cookie reply and a per-address handshake budget are still not built**, and
nothing limits how many initiations one address may send per second. Either
would do: a stateless cookie the sender must echo before the server does any
asymmetric work, or a simple budget of handshakes per address per second.

**A second initiation at a live address is not a second session. Built, and
it was not always so.** `connections[who] = std::move(c)` in `take()` used to
be unconditional, so anyone who replayed a captured `HANDSHAKE_INITIATION`
with the victim's source address gave that address fresh keys, and every
sealed datagram the victim sent from then on failed to open - a player put off
a server for the price of one datagram, needing no forgery, because anyone may
make a valid `IK` initiation to a public key. The same mechanism bit an honest
client: `connect_to` resends its initiation every 250 ms until it is answered,
so two answers in flight left the client holding the keys from the first and
the server those from the second.

Now an address that already has a session gets no other. The initiation that
made the session is kept beside it, and **the same bytes get the same answer
back** - which is what an honest client whose answer was lost needs - while
**different bytes are dropped in silence**, leaving the live session alone.
The address is usable again once the timeout sweep has let the old session go.
Held by `a_repeated_handshake_initiation_does_not_take_a_live_session_away`,
which runs a real client and a real server at once: the client completes a
session, sends its initiation again with `--again`, and must still be answering
the server's pings three seconds later.
`a_replayed_initiation_makes_a_session_the_replayer_cannot_read` still holds
that the replayer learns nothing.

**And a slot goes back only when nobody is left on that key.** The timeout
sweep used to release from `Slots` whatever key the connection held, so a key
displaced by a later handshake at the same address stayed in the session for
the life of the process, and four `connect` runs from one address filled a
four-player server permanently. With one session per address nothing is
displaced, and the sweep now releases a key only when no other connection is
on it - two addresses may share a key, and the first to go quiet must not take
the slot from the second.

**What is still not defended: how many keys one person may mint.** A slot
belongs to a key, and one person with four keys is four players. That is named
under "What is deliberately not defended" and no allowlist is planned.

**What the handshake does not claim** is written in its own header and repeated
here because this is the document that says what is defended: it is not
reviewed cryptography. What is proved, in `tests/unit/test_handshake.cpp`, is
that both ends write the Noise community's `cacophony` known-answer vector for
`Noise_IK_25519_ChaChaPoly_BLAKE2b` byte for byte - the handshake and the
transport keys it splits into - so it matches the Noise specification as an
independent implementation reads it; and that two honest ends agree and
nothing else does - a wrong key, a changed byte, a replayed message and a
truncated one all fail.

**`HANDSHAKE_RESPONSE` (`02`) is one a server should never accept at all**,
because a server never initiates, and **that rule is now in code**: `take()`
returns without a word on one, in the same arm as `REFUSAL`. Built and wired.
No test points at that arm on its own; what holds it is that it is a bare
`return` and the server has nothing else it could do with such a datagram.

**The client's side of it is the undefended one.** `connect_to` in
`src/frontend/cli/main.cpp` reads a `HANDSHAKE_RESPONSE` from whatever address
it arrives from - it takes the sender's address and never compares it with the
address it sent to - and if `Initiator::finish` cannot open it, it prints "the
answer did not open" and gives up. So one forged datagram of type `02`, from
anywhere, arriving before the real answer, ends a connection attempt. The
forgery gains nothing else, because it cannot produce an answer that opens.
**What would defend it: read on until the answer that opens arrives or the five
seconds run out, rather than give up on the first that does not.** That is not
built.

**`SEALED` (`03`) is where every protocol message will live**, and its defence
is the whole of the authenticated encryption: a body that does not
authenticate under the handshake's keys is dropped in silence, with no reply of
any kind, so that a forged datagram is neither an amplifier nor an oracle.
**Built and wired, and that silence is real**: the `sealed` arm of `take()`
returns without sending anything when `Unsealer::open` answers nothing.

**libsodium is a dependency.** `vcpkg.json` lists it, `cmake/CesiumNative.cmake`
names it as this project's own package rather than one of Cesium Native's, and
the top-level `CMakeLists.txt` links `unofficial-sodium::sodium` into
`glideslope_net`. The cipher is ChaCha20-Poly1305 under the handshake's keys,
one key per direction, so a datagram cannot be reflected back at its sender.

**The sequence number and the replay window `REQUIREMENTS.md` 6.7 requires are
built**, in `src/net/sealing.cpp`. Eight bytes of sequence number ride in the
clear in front of every sealed body; they are the cipher's nonce and the tag's
additional data, so changing one only stops the body opening. The opener keeps
the highest number it has opened and a window of `replay_window` (64) behind
it, and answers `replayed` for a number it has already opened, `too_old` for
one further back than the window, and `not_ours` for one that does not open -
and it checks the window before running the cipher, so a replay is cheaper to
refuse than to open. A body that does not open never moves the window.
`a_sealed_body_that_arrives_twice_opens_once`,
`every_order_of_arrival_opens_every_sealed_body_exactly_once`,
`a_sealed_body_older_than_the_window_is_refused_and_the_limit_is_stated` and
`every_change_to_a_sealed_body_is_refused` hold it, and the limit is stated
rather than implied: a datagram delayed by more than 64 is indistinguishable
from a replay and is refused.

**Inside the seal, the first byte says which of five kinds the body is**
(`src/net/inside.hpp`): `reliable`, `inputs`, `state`, `ping` and `pong`. **Two
of the five are built.** A `ping` is answered with a `pong` carrying the same
token, which is how the dashboard's round trip is measured and how a client
stays alive against `--timeout`; a `pong` is believed only when its token is
the one outstanding. `inputs` go from each client and are applied to its own
aircraft only; `state` goes from the server; `reliable` carries the messages
both ways, and the server acts on a client's `CONTROLLER_SWAP` and `WATCH`
alone (above). A first byte the server does not know
is ignored rather than refused, deliberately, so that a later version's client
is not dropped for speaking one.

**A sealed body is matched to a session by its source address**, not by its
key, so the replay window is per address entry rather than per key - see "An
unauthenticated datagram from anywhere" below.

**`REFUSAL` (`04`) is a datagram the server sends and must never act on.** It
is sent in the clear because there may be no session to seal it with, which
means anyone can forge one to anyone. The rule that follows is that a refusal
is advice to a person, never a command to a program: a client must not tear a
session down because an unauthenticated byte told it to, and a server must
never answer a refusal with a refusal, which is how two spoofed endpoints are
made to shout at each other for ever.

**The server's half of that rule is written in code; the client's half is half
written.** `take()` returns without a word on a `REFUSAL`, so a server neither
acts on one nor answers one - built and wired. In the client, `stay()` ignores
every datagram that is not a `SEALED` one that opens, so once a session exists
a refusal moves nothing; but `connect_to`, before the session exists, prints
the reason and returns 1 on any `REFUSAL` that reaches its socket from any
address. **A connection attempt is ended by one unauthenticated byte**, which
is the thing the rule forbids.

**Six of the seven reasons are actually sent, and two of those leak.** The
seven - `UNKNOWN`, `NOT_THIS_PROTOCOL`, `WRONG_VERSION`, `UNKNOWN_TYPE`,
`TOO_SHORT`, `SERVER_FULL`, `BAD_HANDSHAKE` - are defined and held against
`docs/TRANSPORT.md` by
`the_transport_document_and_the_code_agree_byte_for_byte`.
The server sends the four the envelope reader gives it and both of the
handshake's; `UNKNOWN` is sent by nothing. `SERVER_FULL` and `BAD_HANDSHAKE`
each leak something to an unauthenticated sender: that the session is full, and
that a handshake was attempted and failed. **This document says both should be
sent only after the far end has authenticated, or not at all, and the server as
built sends both before.** That is a disagreement between this document's
advice and the code, named here rather than quietly dropped; nothing in
`docs/COMPLETION_PLAN.md` carries it yet.

### The eight reliable messages, and which way each goes

**Six of the eight are only ever sent by the server, which is most of the
answer to "what can a client say".** The server owns the lobby, the session
clock, the weather, what each aircraft is and which terrain dataset everyone
agrees on (`REQUIREMENTS.md` 6.3); a client sends none of them and a server
that is handed one has been handed something with no meaning.

| message | who sends it | would a server accept it |
| --- | --- | --- |
| `LOBBY` | server | no |
| `SESSION` | server | no |
| `WEATHER` | server | no |
| `AIRCRAFT` | server | no |
| `TERRAIN_DATASET` | server | no |
| `CONTROLLER_SWAP` | server, and a client asking | **yes** |
| `WEATHER_ALOFT` | server | no |
| `WATCH` | a client | **yes** |

**So `CONTROLLER_SWAP` and `WATCH` are the two with a client-to-server
threat surface**, and the readers for the other six matter in the opposite
direction: they are what defends a client against a server that is hostile,
broken or a different version. That direction is not hypothetical - a client
is told a server's host, port and key by whoever ran the server: on a command
line (`glideslope_cli connect HOST:PORT KEY`, `glideslope --server HOST PORT
--server-key KEY`) or in the one-line `server.txt` of `REQUIREMENTS.md` 6.6.
The two a client acts on are `AIRCRAFT`, which names the model it loads -
a catalogue id it looks up, never a path it opens - and `CONTROLLER_SWAP`.

**The server drops the other six** (2026-09-25). It reads every reliable
message a client sends, acts on a `CONTROLLER_SWAP` or a `WATCH` and on nothing
else: anything else is acknowledged, so that the client stops repeating it,
and let go. The server's dashboard also builds a `LOBBY` from `Slots` to draw
its own table, and that one is never written to the wire.

Common to all eight, in `src/net/messages.cpp`, and all built:

- **A body is read as the kind its first byte says and never as another.**
  `after_kind` refuses at once if the kind byte is not the one being asked
  for. All sixty-four pairs are tried by `no_message_reads_as_a_kind_it_is_not`,
  and fifty-six of them are refused.
- **A message with anything trailing is refused**, so nothing can be hidden
  behind one: every reader ends on `r.done()`, which is true only when nothing
  went wrong and nothing is left unread.
  `every_message_with_anything_trailing_is_refused` appends one byte to each
  of the eight.
- **A message that has been cut short is refused.** The `Reader` marks itself
  broken on the first read it cannot satisfy, answers zero from then on and
  never runs off the end of its buffer; `ok()` at the end is the only thing
  believed. `every_truncation_of_every_message_is_refused` walks every prefix
  of all seven, more than four hundred of them.
- **A count above its limit breaks the reader rather than being trimmed**, in
  `count()`: a sender asking for more than this protocol allows is not one to
  guess at. A reader that trusted a count could be told to hold four billion
  slots by six bytes.
- **A text field longer than the caller will accept breaks the reader rather
  than being cut**, in `Reader::text`.
- **A number that is not a number is refused, in every floating-point field of
  every message.** A NaN or an infinity is eight bytes like any other, and a
  NaN position would spread through the floating origin and the terrain query
  rather than stopping where it arrived. The refusal sits in one wrapping
  reader that every `f64` of every message passes through, so a field added
  later is checked without anyone remembering to.
  `every_floating_point_field_of_every_message_refuses_a_nan_and_an_infinity`
  walks all nineteen such fields - the other three kinds carry none, and the
  test names them - putting both infinities and four NaNs in each, and five
  extreme but real numbers that must still read.
- **`out` is untouched unless the read succeeded**, so a caller cannot act on
  half a message it has been told not to trust.
- **Every single-byte change to every message either reads or is refused, and
  never anything else** - the whole space, each byte against each of the 255
  other values it could hold, in
  `every_single_byte_change_to_every_message_is_read_or_refused`.

The limits themselves, from `src/net/messages.hpp`, each of which
`the_transport_document_and_the_code_agree_about_the_messages` holds against
`docs/TRANSPORT.md`:

| limit | value |
| --- | --- |
| `most_slots` | 4 |
| `most_levels` | 24 |
| `most_near_ground` | 4 |
| `most_microbursts` | 16 |
| `most_metar_bytes` | 256 |
| `most_name_bytes` | 64 |
| `most_time_bytes` | 32 |
| `sha256_bytes` | 32 |

**None of those is a generous guess, and that is itself a defence.** Each is
chosen so that its message fits in one datagram: `most_message_bytes` is 1218,
being 1232 less the six-byte envelope and the reliable layer's eight-byte
header, and `every_message_filled_to_its_limits_fits_in_one_datagram` fills
every one of the eight to its limits and holds it under that. A limit set
loosely is not a harmless limit here - it is a message that cannot be sent at
all, because nothing fragments.

#### `LOBBY`

Server to client. **Its weakest point is that a slot's index is checked for
range and for nothing else**: a row whose index is 4 or more is refused, but
nothing requires the indices to be distinct, to be in order, or to match the
row's own position, so a lobby of four rows all claiming slot 0 reads cleanly.
`Slots::lobby()` always writes exactly `players_allowed` rows in slot order,
so a correct server never sends such a thing. What would defend it: refusing a
row whose index is not its position, which is the only shape this project ever
sends. Until then a client must not use the index to decide which row is
which. Nothing can be made to address memory out of range by it, which is what
the range check bought.

What is checked: the slot count against `most_slots`, so at most four rows;
each index against `most_slots`; each row's controller against
`known_controller`, so a value outside `NOBODY`, `PERSON` and `AI` makes the
message unreadable rather than defaulting to something; each name against
`most_name_bytes`; and `players_allowed` against 1 to `most_slots`, because a
player count this protocol does not allow is not a lobby.

#### `SESSION`

Server to client. **Its clock must be a number now, and nothing more than
that.** `simulation_time_s` is refused if it is a NaN or an infinity and is
otherwise any finite double the wire holds - a negative clock, or one of 1e300,
reads cleanly - and `began_unix_ms` is any `u64`. What is checked: the name
against `most_name_bytes`, the clock for being finite, and nothing trailing.
What would defend the rest: requiring the clock to be not negative and within a
sane span at the point the client adopts it, which is the code that knows what
a session clock is for. Nothing adopts one yet. See "Numbers that are not
numbers" below.

#### `WEATHER`

Server to client, and since 2026-09-22 the station's report alone: the
forecast above it moved to `WEATHER_ALOFT`, because the two together did not
fit in a datagram.

**Half of the check its numbers want is built.** The split this document asked
for has been made: a NaN or an infinity is not a number at all and no field has
a use for one, so the reader refuses it and the message with it; a number that
is real but absurd belongs where the message becomes a `world::WeatherReport`,
which is the code that knows what a latitude is, **and that half is not
built**. A latitude of 1e300, a negative microburst radius and a microburst
lasting minus an hour all still read as a valid `WEATHER`. The METAR needs
neither, because the server and the client parse the same raw text with the
same code, which is why it is sent as text; the microbursts are numbers on the
wire and nothing yet reads them back into anything.

What is checked: the METAR against `most_metar_bytes` (256); the microbursts
against `most_microbursts` (16); the turbulence flag against 0 or 1 exactly;
the severity against 7; and **the severity byte held to `00` when the flag
says there is none**, so that a byte nobody reads cannot carry anything. At
its limits the whole message is 1,062 bytes of the 1,218 a datagram leaves.

#### `WEATHER_ALOFT`

Server to client, sent after a `WEATHER`. **A station with no forecast simply
has no `WEATHER_ALOFT`**, which is how its absence is said: there is no flag
byte to lie about, and a client must not treat a missing forecast as an error
worth acting on, because it is the ordinary case at most stations.

**Its numbers are checked in the same half-way as `WEATHER`'s**, and there are
more of them than in any other message - eight of the nineteen floating-point
fields the seven messages carry between them. A NaN wind is refused; a pressure
of zero and a height below the centre of the Earth still read cleanly, and
these are the numbers that would feed the wind a client's prediction flies
in.

What is checked: the forecast time against `most_time_bytes` (32); the
pressure levels against `most_levels` (24); the near-ground winds against
`most_near_ground` (4); and nothing trailing. At its limits it is 1,093 bytes,
**the largest message this protocol has**, against the 1,218 a datagram
leaves; the forecast this project actually fetches, nineteen levels and four
winds, is 877. It is the message with the least room, so it is the one to
re-measure if any of those limits is ever raised.

#### `AIRCRAFT`

Server to client. **Its worst field is the JSBSim model directory, which is a
path fragment from the network**, and nothing here checks that it is a name
rather than a traversal: `../../..` is 64 bytes or fewer and reads cleanly.
Whatever eventually turns it into a path must refuse a separator and a parent
reference, and must look the id up in the catalogue rather than joining it to
a directory. Nothing consumes the message yet, so that check has nowhere to
live.

What is checked: the slot against `most_slots`, so a slot of 4 or more is
refused; the catalogue id and the model directory each against
`most_name_bytes`; and nothing trailing.

#### `TERRAIN_DATASET`

Server to client. See "A wrong or malicious terrain dataset" below; the reader
checks the name and the version against `most_name_bytes`, requires exactly
`sha256_bytes` (32) bytes of hash, and refuses anything trailing.

#### `CONTROLLER_SWAP`

**A client may ask for its own aircraft, and nothing else.** The message names
an aircraft by number, and a reader cannot tell whose datagram it is, so the
check is the server's: it honours a swap only when the number is the one the
sender's `Connection` flies, and only to `PERSON` or `AI`. A client asking for
another player's aircraft, an AI's, or one that does not exist is
acknowledged and nothing more (`Fleet::hand`, 2026-09-25). A player's
aircraft keeps its number and slot either way, so a swap cannot move anybody
else. `at_simulation_time_s` is refused if it is a NaN or an infinity; from a
client it is ignored, and the time the server announces is its own clock's.
Nothing limits how often a client may ask: each swap is a controller change
and a reliable message to every client, so a client asking a hundred times a
second costs the server that. A rate limit is owed (below).

#### `WATCH`

**A client says which aircraft it is watching**, by number, or none. It
changes only what that client is told: the watched aircraft's controls, in its
own state updates, fourteen bytes. Any number reads; one that is not flying is
told nothing. It tells a client nothing it could not see already - where every
aircraft is - beyond the positions of one aircraft's controls, which is what
riding along is for. Watching another player's aircraft is allowed: its
controls are no more private than its position.

What is checked: the slot against `most_slots`; `to` against
`known_controller`, so the message is unreadable rather than defaulting if the
controller byte is not `NOBODY`, `PERSON` or `AI`; and nothing trailing.

**What would defend it, and what must be built with Phase 7:**

- The server ignores the slot in the message and uses the slot of the key the
  datagram authenticated as. A player may hand over their own aircraft and
  take it back (`REQUIREMENTS.md` 6.5); whether anyone may take over an AI
  traffic aircraft is a lobby rule that does not exist yet, and until it does
  the answer is no.
- The time must be finite and within a bounded window either side of the
  session clock, or the server simply ignores it and swaps on its own clock.
  The server owns the clock; a swap timed by a client is a client telling the
  server when to act.
- A swap rate limit, because a swap is cheap to ask for and a controller
  change is not free.

### Inputs and state updates

**The input packet is designed and built; nothing sends or receives one over a
socket.** `src/net/inputs.cpp` holds both ends and they are **unwired**:
neither `glideslope_server` nor either client mentions `InputSender` or
`InputReceiver`, and `inputs` is one of the three kinds inside a sealed body
that are named and not built. The state update is not written at all, in either
sense.

**The range half of `REQUIREMENTS.md` 6.2's input validation is answered by the
encoding.** Every control goes on the wire as a 16-bit fraction of -1 to 1, so
a control outside that range cannot be expressed; the receiver also refuses a
frame count of nought or more than `redundancy` (4), refuses a packet whose
newest sequence is smaller than its own frame count, and refuses anything
trailing. `an_input_receiver_refuses_anything_that_is_not_an_input_packet`
holds it.

**The rate half is not built, and neither is the rule about a sequence far
ahead.** `InputReceiver::received` takes whatever sequence arrives as the
newest it has seen, so one packet claiming sequence 4,294,967,295 would make it
ignore every real frame for the rest of the session. That is no longer a future
design to get right - it is a line of code to write: the receiver must refuse a
sequence further ahead of its own than the session's input rate could account
for, and something must bound how many packets an address may send per second.
Neither is written.

## Amplification

**The server replies now, so it amplifies now.** Two replies go to an address
that has not authenticated: a `REFUSAL` and a `HANDSHAKE_RESPONSE`.

The refusal's ratio is small and fixed: a `REFUSAL` is seven bytes, six of
envelope and one of reason, and the smallest datagram that can provoke one is a
single byte, refused as `TOO_SHORT`. Seven bytes out for one in is not much of
a weapon, but it is not nothing when the source address is forged, and **the
fix this document proposed is still not built**: say nothing at all to a
datagram too short to be one of ours, and to a datagram whose magic is not
ours. A sender that got the magic right deserves an answer; a random byte does
not. Today every random byte gets seven back.

The handshake's reply is the one that matters. A `HANDSHAKE_RESPONSE` goes to
whatever address an initiation claimed to come from, before that address has
proved it can receive anything, and it costs the server an X25519 operation as
well as the bytes. **Nothing bounds how often**, which is the same gap named
under `HANDSHAKE_INITIATION` above.

Inside a session the only reply is a `pong` to a `ping`, which is nine bytes of
plaintext for nine, from a peer that has authenticated. It is not an
amplifier.

The 1232-byte cap bounds every single reply, so no one datagram can be an
amplifier beyond it. The risk is in the number of replies, not the size of one,
which is why `SERVER_FULL` and `BAD_HANDSHAKE` should wait for a completed
handshake - **and do not** - and why the opening burst of `LOBBY`, `SESSION`,
`WEATHER`, `WEATHER_ALOFT`, `AIRCRAFT` and `TERRAIN_DATASET` must. That burst
is the amplifier worth caring about: several datagrams and, in the weather
alone, 2,155 bytes across two messages at their limits - 1,062 and 1,093,
measured by `every_message_filled_to_its_limits_fits_in_one_datagram` -
provoked by one handshake. **It is not built**, and when it is it must never be
sent to an address that has not proved it can receive at that address.

## Replay

**There is a replay defence, and it covers only what is sealed.**
`REQUIREMENTS.md` 6.7's sequence number and replay window are built and wired,
in `src/net/sealing.cpp` and in the `sealed` arm of the server's `take()`: a
sealed body carrying a number already opened, or more than 64 behind the newest
that opened, is refused before the cipher is run.

**The handshake is covered by memory, not by the cipher.** The server
remembers every initiation that has made a session, by its first 32 bytes,
the initiator's ephemeral key. It drops a copy of one in silence, from any
address, whether that session is live or gone (`Taken`, in the server's
`main.cpp`). It was built for an honest client, not an attacker. CI's
four-player test counted six players' aircraft because copies of the fourth
client's initiation, read after its session had been let go, were each taken
as a new handshake. Each made a session and an aircraft that nobody flew
(`PROJECT_STATUS.md`, 2026-09-26). Held by
`a_copy_of_an_initiation_arriving_after_its_session_was_let_go_makes_no_second_player`,
seen to fail without the check.

**What a replayed or forged initiation can do now:**

- **A captured initiation replayed** is dropped before any X25519 work, from
  any address: the one it was captured from, a spoofed one, or a new one after
  the victim's session has gone. Before, it was answered afresh from every new
  address: a session the replayer could not read, and an aircraft for the
  victim's key. That aircraft sat in the sky until `--timeout`, and it held a
  player's slot.
- **It cannot be used to lock an honest player out.** The memory is keyed by
  the ephemeral key, which an honest client makes afresh for every connection.
  An attacker who replays a player's old initiation blocks only that
  initiation, which the player will never send again.
- **An attacker may forge an initiation carrying somebody else's ephemeral
  key** before that person's own initiation arrives. Only someone who has seen
  the ephemeral key in flight can do that, and the forgery must also seal a
  static key under it. That needs the ephemeral's secret. Without it, the
  forgery fails `Responder::answer`, and a forged initiation that does not
  complete is not remembered. So only a completed handshake fills the memory.
- **Anyone may fill the memory with handshakes of their own**, each with a new
  ephemeral key. It holds the newest 65,536 (about 2 MiB) and forgets the
  oldest. Somebody who completed 65,536 handshakes could push a captured
  initiation out and replay it again, and it would be answered as before: a
  session the replayer cannot read (which
  `a_replayed_initiation_makes_a_session_the_replayer_cannot_read` holds). A
  restarted server remembers nothing. Neither is defended: that would need a
  timestamp in the initiation, as WireGuard has, and this protocol has none.
  What still bounds a flood of fresh handshakes is the rate limit that is not
  built.

A replayed `SEALED` datagram is refused by the replay window. A replayed
initiation can no longer take a live session away either; see
`HANDSHAKE_INITIATION` above.

The reliable layer throws away a message whose number is at or below the last
one delivered - `number <= delivered_` in `Reliable::received` - and
`a_reliable_message_that_arrives_twice_is_handed_up_once` holds it. **That is
duplicate suppression, not a replay defence**, and the difference matters: it
exists so that retransmission delivers exactly once, it does not reject a
replayed message carrying a number that has not been delivered yet, and it
believes whatever datagram it is handed. It is no substitute for the window the
sealing has, which is in front of it: since 2026-09-25 the server and both
clients run a `Reliable` inside the sealed body, and what reaches it has
already opened under the session's keys and passed the replay window.

The message that would hurt is `CONTROLLER_SWAP`, because it carries its own
effective time: a swap captured from one session and replayed into another
would arrive with a stale `at_simulation_time_s`, which is a second reason for
the server to use its own clock rather than the client's.

**The reliable layer no longer believes an acknowledgement it could not have
earned.** It keeps the highest number it has actually put on the wire and
ignores anything above that, so the forged `0xFFFFFFFF` that once flushed a
sender's whole queue now lets go of nothing;
`an_acknowledgement_of_a_message_that_was_never_sent_is_not_believed` holds it
and the item is ticked in `docs/COMPLETION_PLAN.md`. **What is still believed
is a forged acknowledgement of a message that was sent**, because nothing tells
`Reliable` a forged datagram from a real one. The sealing is what would: a body
that does not open reaches nothing. But that remains a convention rather than a
type - nothing stops a caller feeding `Reliable::received` raw socket bytes -
and since no `Reliable` is wired to a socket at all, the wiring that would make
it impossible has not been written.

## Numbers that are not numbers

**Every message now rejects a NaN and an infinity in every one of its
floating-point fields**, and the item is ticked in `docs/COMPLETION_PLAN.md`.
The refusal is in one wrapping reader in `src/net/messages.cpp` that every
`f64` of every message passes through - a latitude, a microburst's radius and
duration, every wind and temperature in a `WEATHER_ALOFT`, the session clock
and the time a controller swap takes effect - so a field added later is checked
without anyone remembering to, and the message is refused whole rather than
handed up with a hole in it, which is the behaviour every other rule here
already has.

**What it would have cost is why it was worth its own section.** World
positions are double precision and Earth-centred, and a NaN entering there
propagates through the floating origin and the terrain query rather than
stopping at the field it arrived in; an infinite microburst duration never
ends; a NaN session clock poisons every comparison made against it.

**What is still not checked is range**, and that split is deliberate: the
reader refuses what is not a number, and what is a number but absurd - a
latitude of 1e300, a negative radius, a pressure of zero, a session clock
before the Big Bang - belongs in the code that knows what each field is for.
**No such code exists**, because nothing consumes any of these messages yet. So
the range half is owed before the server accepts anything, not after. The input
packet cannot carry either kind of nonsense: its controls are 16-bit fractions
of -1 to 1, not doubles.

## An unauthenticated datagram from anywhere

**The socket binds every address the machine has** - `INADDR_ANY` and
`in6addr_any` in `UdpSocket::bound` - so anyone who can route a packet to the
port can send to it, and nothing filters by source. There is no
`--listen-address`.

**UDP source addresses are forgeable, and nothing here can tell a datagram that
came from an address from one that claims to.** Noise does not bind a session
to an address, which is the right answer for a player whose phone hands over
between networks - but the server as built does the opposite: its connection
table is keyed by the address's text, so **a session is identified by its
address and not by its key**. Two consequences, both read straight off
`take()`: a client that changes address loses its session, because nothing
moves a connection from one address to another; and its old session sits in the
table until the timeout sweep takes it, holding the slot. **The rule this
document asked for - a session identified by its key, its address updated only
from a datagram that has already authenticated - is not built**, and the table
it would live in exists now, so there is no longer any excuse of nowhere to put
it. What the address keying no longer costs is a live session: an initiation at
an address that has one is answered from what is already there, or dropped.

An unauthenticated datagram costs the server one `recvfrom`, an envelope read,
and then whatever its type asks for: a `REFUSAL` written and sent, or - if it
says `01` and a slot is free - an X25519 operation and a handshake answer.
**There is no rate limit of any kind**, and the loop sleeps for two
milliseconds only when nothing was waiting, so a sustained flood keeps a core
busy for as long as it lasts. That is the honest cost, and it is no longer the
floor this document once described: **the server does asymmetric work for a
stranger on every initiation it can find a slot for.**

## Resource exhaustion through the reliable layer

**The queues are bounded; the number of peers is what is not.**

`Reliable::send` refuses once `most_in_flight` (256) messages are waiting to
be acknowledged, which bounds the sender's own queue - that is the local
caller's problem, not an attacker's, and
`a_reliable_sender_refuses_once_too_many_are_waiting` holds it.

The receiving side is the one an attacker touches. A peer that sends messages
2 upwards while withholding message 1 makes the receiver hold them:
`Reliable::received` keeps at most `most_held_back` (256) out-of-order bodies
and drops the rest, because an endpoint stuck behind one missing message has
no reason to hold an unbounded number behind it. Each body is bounded by
`most_message_bytes`, so at most 1218 bytes, which puts the worst case at
roughly 305 KiB per peer - 256 times 1218 bytes - before the map's own
overhead.

**With at most four players, that is about 1.2 MiB, and the four is the real
defence.** It comes from `Slots`: `admit` returns nothing once `full()`, and
`players_allowed` is held to 1 to `most_slots` twice over, by `wrong_with()`
in the server's argument parsing and again by `std::clamp` in the `Slots`
constructor. A `Reliable` must therefore be created only for a peer that has
completed a handshake and been given a slot - never one per address that has
sent a datagram. **The server's `Connection` follows half of that rule.** It is
made in the `handshake_initiation` arm only after `Responder::answer` succeeds
and `Slots::admit` returns a slot, so a datagram that is not a completed
initiation makes no entry. **But it is made per address, not per key.**
`Slots::admit` hands a key already in the session the slot it already has, and
the server gives each entry an aircraft of its own. So one key's handshakes
from many addresses make one entry per address, each with its own pair of
cipher states, each costing an X25519 operation. They go only when `--timeout`
sweeps them. One *captured* initiation no longer does this: it is taken once,
and its copies are dropped from every address (see "Replay"). But anyone who
holds a key's secret can make a fresh initiation for each address. The session
being full stops it, because a fresh address is then refused `SERVER_FULL`
before the crypto; a session with a slot free does not. That is the unbounded
number of peers this section's first line names, and it is a path now rather
than a warning: bounded only by the flood rate times `--timeout`. **No
`Reliable` follows the rule either, because no `Reliable` exists in the server
at all.** Getting that wrong when it is wired is how a bounded per-peer cost
becomes an unbounded one.

**`--timeout` is wired; retransmission still gives up on nothing.** A client
the server has not heard from for longer than `--timeout` (default 10 seconds)
is let go each time round the loop and its slot released, and the server knocks
on every connection once a second so that a client which is there answers and
stays. `a_server_lets_go_a_client_it_has_not_heard_from_for_its_timeout` runs a
real server and a real client to hold it. The reliable layer is the part that
gives
up on nothing - an unacknowledged message is retransmitted every 0.25 seconds
for ever - and it is unwired, so nothing retransmits anything today. Three
smaller gaps, none exploitable today: message numbers are `u32` with no wrap
handling, which a session would have to send four billion reliable messages to
reach; and `Reliable` has no notion of a peer, so everything above is
per-instance, not per-address. **The timeout sweep no longer strands a slot**:
one session per address means no key is displaced, and a key is released only
when no other connection is on it.

## A client claiming a slot, or a player count

**A slot cannot be claimed, because it is not handed out.** `Slots::admit`
works out a player's slot as the rank of their key among the keys present, so
connecting the same three people in any of the six orders gives each the same
slot - `slots_are_the_same_whatever_order_the_players_connect_in` and
`a_players_slot_is_their_keys_rank_among_those_in_the_session` hold it. There
is no message in which a client asks for a slot. **A client names an aircraft
in `CONTROLLER_SWAP`**, and the server honours it only for the aircraft the
sender's `Connection` flies (above).

A player count cannot be claimed either: `players_allowed` is the server's
`--players`, refused outside 1 to 4 before the server starts
(`a_session_is_never_fewer_than_one_player_or_more_than_four`) and clamped
again in the `Slots` constructor. The fifth player is refused because
`admit` returns nothing when the session is full, which is what the
`SERVER_FULL` reason exists for -
`a_session_is_full_at_its_player_count_and_the_next_is_refused` holds the rule,
and the server does now send that refusal - in the clear, to an address that
has not authenticated, which is the leak named under `REFUSAL` above.

In the other direction, the `LOBBY` reader refusing a `players_allowed`
outside 1 to `most_slots` is what stops a hostile server telling a client it
is in a session of 200.

**What is not defended: one person, four keys, every slot.** Identity is a key,
and nothing counts keys per person, per address or per anything. A private
server's answer is not to give the key out; a public server would need an
allowlist, and there is no plan for one. **It costs four addresses now, not
one.** An address with a session gets no second one, so four keys from one
address take one slot and wait out the timeout for the next; four slots wants
four sockets, which is four processes or one program written to do it. That
raises the price and does not defend it.

## A wrong or malicious terrain dataset

**The hash in `TERRAIN_DATASET` defends agreement, not safety, and the check
it is for is not built.** Its purpose is that the server and every client's
prediction agree where the ground is: collision terrain is always the open DEM
at a pinned version and hash, never the visual mesh, so a client predicting
against a different dataset would drift from the server for a reason no
measurement would explain. A client that uses the wrong dataset is the failure
this message exists to prevent, and a 32-byte SHA-256 checked before the
dataset is used is what prevents it. Nothing consumes the message yet, so
nothing checks the hash yet.

**A matching hash proves the dataset is the one the server meant. It does not
prove the dataset is harmless.** The bytes still go through a GeoTIFF decoder,
and that decoder is the attack surface - a larger one than anything in
`src/net/`, because it is a real file format. "Every network parser fuzzed
under sanitizers" is done: eleven parsers against eighteen seeds, each seed
whole, cut to every length and changed a byte at a time, under the sanitizers
this build already compiles with. **Three readers that take bytes off the wire
are not among the eleven**: `Responder::answer`, `Unsealer::open` and the
terrain decoder. The first two are walked by their own tests against every
single-byte change and every truncation, which is most of what the corpus would
do to them. The terrain decoder has nothing of the kind, and it is the one that
most needs it.

**And the name is not a name yet.** `TerrainDataset::name` and `version` are
checked for length (`most_name_bytes`) and nothing else, so a name containing
a path separator or a parent reference reads cleanly. Whatever turns either
into a file path, a cache key or a URL must treat it as hostile: look it up in
a list of datasets this build knows, rather than joining it to a directory.

## Where the weather is asked

**`GLIDESLOPE_WEATHER_SERVICE` redirects every weather request, in every
build, and is held to HTTPS or the loopback.** It exists so a test can point
the weather at a port nothing listens on, or at a stub on the loopback. It is
read by the shipped programs too, so whoever can set a player's environment
can choose where their weather comes from - but only an `https://` host, whose
certificate is checked as the real services' are, or `http://127.0.0.1` /
`http://localhost`, with a port or without and nothing after
(`world::weather_service_allowed`, and
`the_weather_may_be_asked_elsewhere_only_over_https_or_of_the_loopback`).
Anything else stops the flight with the variable named, rather than sending
the request over plain HTTP to another machine, where it could be read and
its answer changed. What it cannot defend: someone who sets the environment
can name an HTTPS host of their own, whose weather is theirs to make up. That
is no more than they could do by running the program some other way, and the
weather is not scored.

## What writing this document found

Writing it found six disagreements between the code and the documents, which is
the point of writing it. **All six are now fixed**, and are described above as
the code now stands:

- a `WEATHER` at its limits was 5,419 bytes against a 1,218-byte budget, so a
  full one could never have been sent at all; it is now two messages that fit,
  and a test fills every message to its limits and holds it to the datagram;
- the turbulence severity byte was free to carry anything when the flag
  before it said there was none, and is now held to `00`;
- a slot index was a raw byte with no range check in `LOBBY`, `AIRCRAFT` and
  `CONTROLLER_SWAP`, and 4 or more is now refused in `LOBBY`; `AIRCRAFT` and
  `CONTROLLER_SWAP` have since named an aircraft's number instead, any byte,
  checked by the server against what the sender flies;
- the server's receive buffer was 1500 bytes where a datagram is 1232, and is
  now `platform::largest_datagram`;
- the reliable layer believed any acknowledgement it was told, and now ignores
  one above the highest number it has actually put on the wire;
- no message rejected a NaN or an infinity in an `f64`, and all nineteen
  floating-point fields now do.

The last two were items at the bottom of `docs/COMPLETION_PLAN.md` and are
ticked there.

A seventh, found re-reading after the fix, was that three places still said
there were six reliable messages where there are seven - the six the plan item
names are six *things*, and the weather is two of them. That prose was
corrected the same day.

**Reading this document again against the server that now exists** found four
more, described above where each belongs:

- **a `HANDSHAKE_INITIATION` at an address that already had a session replaced
  that session's keys**, so a replayed initiation was a denial of service
  against an established peer, and an honest client's own retransmission could
  leave the two ends holding different keys. **Fixed the day it was found**: an
  address with a session gets the same answer for the same initiation and
  nothing at all for a different one, held by
  `a_repeated_handshake_initiation_does_not_take_a_live_session_away`, which
  was watched failing against the code as it was;
- **the key a replaced connection held was never released from `Slots`**, so
  four handshakes from one address filled a four-player server and left it full
  until the process restarted. **Fixed with the above**, and the sweep now
  releases a key only when no connection is left on it;
- **the connection table is keyed by address rather than by key**, so one
  captured initiation replayed from many spoofed addresses makes an entry and
  an X25519 operation apiece, for as long as the session has a slot free.
  Recorded nowhere;
- **`SERVER_FULL` and `BAD_HANDSHAKE` are sent to senders that have not
  authenticated**, which is what this document says they must not be. Recorded
  nowhere.

**Nothing else in this document is known to disagree with the code today.**
That is a statement about a document that changes no code, so it is only as
good as the next reading of it.

## The order of the work

Each of these belongs to the Phase 6 transport item or to a Phase 7 item in
`docs/COMPLETION_PLAN.md`, except the four this document found and named
above, which belong to no item yet. The order is the order in which each
defence becomes possible, not a wish list.

**Done**, and described above with what holds each:

1. **The handshake and the sealing**, with libsodium. Both are built, wired and
   run on every datagram. **The cost of an unauthenticated
   `HANDSHAKE_INITIATION` was not designed at the same time**, which this
   document said it must be; item 7 below is the debt that left.
2. **The peer table**, so that a `Connection` exists only for a key that has
   completed a handshake and been given a slot by `Slots`. Done for
   `Connection`; **no `Reliable` is created for anybody yet**, so the half of
   this rule that names `Reliable` is untested by anything real.
3. **`--timeout` wired**, so a silent peer is let go and its slot returned.
4. **The replay window**, per `REQUIREMENTS.md` 6.7, in the sealing and below
   where the reliable layer will be.
5. **A NaN or an infinity refused in every floating-point field.**

**Left**, in the order the work should take them:

6. **A session identified by its key rather than its address**, so that a
   client that changes address keeps its session instead of leaving one behind
   to time out. Taking a live session away and stranding a slot were the sharp
   ends of this and are fixed; what is left is the smaller, honest case.
7. **A cookie reply or a per-address handshake budget**, and no refusal sent to
   an address that has not authenticated.
8. **Direction, on every message.** Done 2026-09-25: the server acts on a
   client's `CONTROLLER_SWAP` and `WATCH` and on nothing else.
9. **A rate limit on `CONTROLLER_SWAP` and `WATCH`.** The sender's own
   aircraft is checked and the server's clock used (2026-09-25); how often a
   client may ask is not limited.
10. **Input ranges and rates.** The range is the wire format's already; what is
    owed is the rate limit and refusing a sequence far ahead of the server's.
11. **The terrain dataset hash checked before use**, and the dataset name
    looked up rather than joined to a path.
12. **Fuzzing the three readers the corpus misses** - the handshake, the
    sealing and the terrain decoder - under sanitizers, in CI.

The truthful summary of this project's network security is no longer the
sentence this document used to end on. It is this: **the server authenticates
every peer it admits, seals both directions and refuses a replay - and one
unauthenticated datagram can still take an admitted peer's session away and
keep their slot.** Nothing a client sends reaches an aircraft, which is the
only reason that is not yet a way to spoil somebody's flight.

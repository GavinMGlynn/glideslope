# Threats

What is defended, from whom and how; what is deliberately not defended, and
why; and the order the work is done in. Every message the server accepts is
named below with its defence, or with why it needs none.

## The server accepts nothing yet

**Said first, because everything else here is conditional on it.** There is no
handshake and there is no sealing, so today `glideslope_server` binds a UDP
port, counts the datagrams that arrive and drops every one of them unread. It
does not parse an envelope, does not decrypt anything, does not send a refusal
and does not admit anybody. `docs/TRANSPORT.md`, "What is not here yet", says
the same thing, and the server prints it on its own dashboard: "nobody can
connect yet: the handshake is not built."

So **no defence described here is a defence that is presently doing anything
to a live attacker.** Each one is in one of three states, and every section
below says which:

| state | means |
| --- | --- |
| built | the code exists and a named test holds it |
| unwired | the code exists and nothing calls it outside its test |
| not built | there is nothing, and the section says what would be needed |

The envelope reader and all seven message readers are **built**. The envelope
reader is **unwired**: `read_envelope` is called by `tests/unit/test_protocol.cpp`
and by nothing else in `src/`. The handshake, the sealing, the replay window,
the per-peer connection table, rate limiting and every input check are **not
built**.

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
`TOO_SHORT`; a type outside the four is `UNKNOWN_TYPE`. Built, and held by
`an_envelope_that_is_wrong_is_refused_with_the_reason_it_is_wrong`,
`every_truncation_of_a_datagram_is_refused_without_running_off_the_end` and
`no_single_byte_changed_anywhere_in_a_datagram_can_break_the_reader`. Unwired:
the server never calls it.

**`HANDSHAKE_INITIATION` (`01`) is the one datagram a stranger may legitimately
send, and it is the least defended thing in this document.** It is not built.
When it is, `Noise_IK_25519_ChaChaPoly_BLAKE2s` obliges the server to do an
X25519 operation on a datagram from an address it has never heard of, before
it knows whose key is talking: that is the cost an attacker can impose for the
price of one small datagram. IK's first message carries the initiator's static
key encrypted to the server's, so the server does learn which key is talking
before it commits a slot or a `Reliable` queue to it - **but that is a
property of the pattern named in `REQUIREMENTS.md` 6.7, not of any code here,
and it is worth nothing until it is built and tested.** Nothing in this
project yet specifies a cookie reply or a per-address rate limit, which is
what would bound the cost. Either would do: a stateless cookie the sender must
echo before the server does any asymmetric work, or a simple budget of
handshakes per address per second. This is the first thing to design when the
handshake is written, not the last.

**`HANDSHAKE_RESPONSE` (`02`) is one a server should never accept at all**,
because a server never initiates. `known_type` accepts it as a known type and
`read_envelope` hands it back, so the check that puts it in its place - a
server ignores it in silence - does not exist, because there is nothing yet to
put it in place of.

**`SEALED` (`03`) is where every protocol message will live**, and its defence
is the whole of the authenticated encryption: a body that does not
authenticate under the handshake's keys is dropped in silence, with no reply
of any kind, so that a forged datagram is neither an amplifier nor an oracle.
Above that, `REQUIREMENTS.md` 6.7 requires a sequence number and a replay
window per message. **None of it is built. Nothing seals anything today**, and
libsodium is not yet a dependency.

**`REFUSAL` (`04`) is a datagram the server sends and must never act on.** It
is sent in the clear because there may be no session to seal it with, which
means anyone can forge one to anyone. The rule that follows is that a refusal
is advice to a person, never a command to a program: a client must not tear a
session down because an unauthenticated byte told it to, and a server must
never answer a refusal with a refusal, which is how two spoofed endpoints are
made to shout at each other for ever. **Neither rule is written in code**,
because nothing sends or receives one yet. The seven reasons - `UNKNOWN`,
`NOT_THIS_PROTOCOL`, `WRONG_VERSION`, `UNKNOWN_TYPE`, `TOO_SHORT`,
`SERVER_FULL`, `BAD_HANDSHAKE` - are defined and tested against the document,
and `SERVER_FULL` and `BAD_HANDSHAKE` are both reasons that leak something to
an unauthenticated sender: that the session is full, and that a handshake was
attempted and failed. Both should be sent only after the far end has
authenticated, or not at all.

### The seven reliable messages, and which way each goes

**Six of the seven are only ever sent by the server, which is most of the
answer to "what can a client say".** The server owns the lobby, the session
clock, the weather, which aeroplane each slot is flying and which terrain
dataset everyone agrees on (`REQUIREMENTS.md` 6.3); a client sends none of
them and a server that is handed one has been handed something with no
meaning.

| message | who sends it | would a server accept it |
| --- | --- | --- |
| `LOBBY` | server | no |
| `SESSION` | server | no |
| `WEATHER` | server | no |
| `AIRCRAFT` | server | no |
| `TERRAIN_DATASET` | server | no |
| `CONTROLLER_SWAP` | server, and a client asking | **yes** |
| `WEATHER_ALOFT` | server | no |

**So `CONTROLLER_SWAP` is the only one of the seven with a client-to-server
threat surface**, and the readers for the other six matter in the opposite
direction: they are what defends a client against a server that is hostile,
broken or a different version. That direction is not hypothetical - a client
is told a server's host, port and key by whoever gave it a `server.txt`.

The check that the server drops the other six is **not built**, for the same
reason as everything else: there is no peer whose messages could be dropped.

Common to all seven, in `src/net/messages.cpp`, and all built:

- **A body is read as the kind its first byte says and never as another.**
  `after_kind` refuses at once if the kind byte is not the one being asked
  for. All forty-nine pairs are tried by `no_message_reads_as_a_kind_it_is_not`,
  and forty-two of them are refused.
- **A message with anything trailing is refused**, so nothing can be hidden
  behind one: every reader ends on `r.done()`, which is true only when nothing
  went wrong and nothing is left unread.
  `every_message_with_anything_trailing_is_refused` appends one byte to each
  of the seven.
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
every one of the seven to its limits and holds it under that. A limit set
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

Server to client. **Its numbers are not checked.** `simulation_time_s` is any
double the wire holds, NaN and infinity included, and `began_unix_ms` is any
`u64`; a NaN session clock would poison every time comparison that used it.
What is checked: the name against `most_name_bytes`, and nothing trailing.
What would defend the rest: requiring the clock to be finite and not negative
at the point the client adopts it. This is one face of an open plan item - see
"Numbers that are not numbers" below.

#### `WEATHER`

Server to client, and since 2026-09-22 the station's report alone: the
forecast above it moved to `WEATHER_ALOFT`, because the two together did not
fit in a datagram.

**Every floating-point field in it is unchecked.** A latitude of 1e300, a
negative microburst radius, a NaN wind or a microburst lasting minus an hour
all read as a valid `WEATHER`. These numbers go into the flight model, and the
check splits in two: a NaN or an infinity belongs in the reader, because it is
not a number at all and no field has a use for it, while a number that is real
but absurd belongs where the message becomes a `world::WeatherReport`, which
is the code that knows what a latitude is. The METAR needs neither, because
the server and the client parse the same raw text with the same code, which is
why it is sent as text; the microbursts are numbers on the wire and nothing
yet reads them back into anything.

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

**Its numbers are unchecked in the same way as `WEATHER`'s**, and there are
more of them than in any other message: a pressure of zero, a NaN wind or a
height below the centre of the Earth all read cleanly, and these feed the wind
a client's prediction flies in.

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

**This is the one message a server would accept from a client, and the reader
cannot defend the thing that matters about it.** `slot` is now refused at 4
and above, which stops it addressing anything out of range, but **a slot in
range is still unrelated to who sent it**: a client asking to swap slot 3
while sitting in slot 0 writes a message that reads perfectly, and no reader
can tell otherwise, because a reader does not know whose datagram it is.
That check belongs at the server, above the reader, and there is no server to
put it in yet. `at_simulation_time_s` is any double, NaN, infinity, a time
long past and a time in the next century included.

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

**They are not designed yet**, so they are named here only so that nothing
mistakes their absence for a decision. `REQUIREMENTS.md` 6.2 requires input
validation - ranges and rate limits - on a stream that is sent redundantly and
tagged with sequence numbers. When those messages are written, every control
axis needs a stated range and every input frame a stated rate, and a sequence
number far ahead of the server's must be discarded rather than believed.

## Amplification

**Today the server replies to nothing, so it amplifies nothing.** That is the
only reason there is no amplification vector here.

When refusals are sent, the worst ratio is small and fixed: a `REFUSAL` is
seven bytes, six of envelope and one of reason, and the smallest datagram that
can provoke one is a single byte, refused as `TOO_SHORT`. Seven bytes out for
one in is not much of a weapon, but it is not nothing when the source address
is forged, and the fix is cheap: **say nothing at all to a datagram too short
to be one of ours, and to a datagram whose magic is not ours.** A sender that
got the magic right deserves an answer; a random byte does not.

The 1232-byte cap bounds every single reply, so no one datagram can be an
amplifier beyond it. The risk is in the number of replies, not the size of
one, which is why `SERVER_FULL`, `BAD_HANDSHAKE` and the whole opening burst
of `LOBBY`, `SESSION`, `WEATHER`, `WEATHER_ALOFT`, `AIRCRAFT` and
`TERRAIN_DATASET` must wait for a completed handshake. That burst is the
amplifier worth caring about: it is several datagrams and, in the weather
alone, up to 2,155 bytes across two messages, provoked by one handshake, and
it must never be sent to an address that has not proved it can receive at that
address.

## Replay

**There is no replay defence.** `REQUIREMENTS.md` 6.7 requires a sequence
number and a replay window per message; neither exists, because nothing seals
anything.

The reliable layer throws away a message whose number is at or below the last
one delivered - `number <= delivered_` in `Reliable::received` - and
`a_reliable_message_that_arrives_twice_is_handed_up_once` holds it. **That is
duplicate suppression, not a replay defence**, and the difference matters: it
exists so that retransmission delivers exactly once, it does not reject a
replayed message carrying a number that has not been delivered yet, and it
believes whatever datagram it is handed. It is no substitute for the window
the sealing owes.

The message that would hurt is `CONTROLLER_SWAP`, because it carries its own
effective time: a swap captured from one session and replayed into another
would arrive with a stale `at_simulation_time_s`, which is a second reason for
the server to use its own clock rather than the client's.

**The reliable layer also believes acknowledgements**, and this one is open
work rather than a future concern: it is recorded as an item in
`docs/COMPLETION_PLAN.md`. A datagram whose acknowledgement field is
`0xFFFFFFFF` flushes every unacknowledged message out of the sender's queue,
and the far end never learns they were lost. The header says the layer is fed
by the transport above it, and that is the defence - nothing must reach
`Reliable::received` that has not authenticated - but it is a convention, not
a type: nothing stops a caller feeding it raw socket bytes. When the sealing
exists, the wiring must make that impossible rather than merely unlikely.

## Numbers that are not numbers

**No message rejects a NaN or an infinity in any of its floating-point
fields**, and this is recorded as an open item in `docs/COMPLETION_PLAN.md`
rather than left as an observation. Every `f64` is read as whatever bits
arrive: a latitude, a microburst's radius and duration, every wind and
temperature in a `WEATHER_ALOFT`, the session clock and the time a controller
swap takes effect.

**What it would cost is worse than a wrong number**, which is why it is worth
its own section. World positions are double precision and Earth-centred, and a
NaN entering there propagates through the floating origin and the terrain
query rather than stopping at the field it arrived in; an infinite microburst
duration never ends; a NaN session clock poisons every comparison made against
it. The reader is the right place for the check, because it is the one place
every one of these fields passes through, and because a message refused whole
is the behaviour every other rule here already has.

Only one of these fields is one a client could set - `CONTROLLER_SWAP`'s
effective time - and that stays true only until the input messages exist, when
every control axis becomes one too. So this is owed before the server accepts
anything, not after.

## An unauthenticated datagram from anywhere

**The socket binds every address the machine has** - `INADDR_ANY` and
`in6addr_any` in `UdpSocket::bound` - so anyone who can route a packet to the
port can send to it, and nothing filters by source. There is no
`--listen-address`.

**UDP source addresses are forgeable, and nothing here can tell a datagram
that came from an address from one that claims to.** Noise does not bind a
session to an address, which is the right answer for a player whose phone
hands over between networks, but it means the rule has to be stated: a session
is identified by its key, and a session's address may only be updated from a
datagram that has already authenticated. There is no session table yet, so the
rule has nowhere to live.

Today an unauthenticated datagram costs the server one `recvfrom`, one counter
increment and one pass round the loop. **There is no rate limit of any kind**,
and the loop sleeps for two milliseconds only when nothing was waiting, so a
sustained flood keeps a core busy for as long as it lasts. That is the honest
cost, and it is the floor: it gets worse the moment the server does asymmetric
work for a stranger, which is what the handshake is.

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
sent a datagram. That is the wiring rule that does not exist yet, and getting
it wrong is how a bounded per-peer cost becomes an unbounded one.

**Nothing gives up.** A message that is never acknowledged is retransmitted
every 0.25 seconds for ever; the server's `--timeout` (default 10 seconds) is
meant to let a silent client go and drives nothing yet, which the server says
on its own dashboard. Two smaller gaps, neither exploitable today: message
numbers are `u32` with no wrap handling, which a session would have to send
four billion reliable messages to reach; and `Reliable` has no notion of a
peer, so everything above is per-instance, not per-address.

## A client claiming a slot, or a player count

**A slot cannot be claimed, because it is not handed out.** `Slots::admit`
works out a player's slot as the rank of their key among the keys present, so
connecting the same three people in any of the six orders gives each the same
slot - `slots_are_the_same_whatever_order_the_players_connect_in` and
`a_players_slot_is_their_keys_rank_among_those_in_the_session` hold it. There
is no message in which a client asks for a slot. **The one place a client
names a slot is `CONTROLLER_SWAP`, and that byte is checked for range and not
for ownership**, which is why it is the sharp edge of this document: a slot of
4 or more is refused by the reader, and a slot of 0 to 3 that is not the
sender's own can only be caught by a server that knows whose datagram it is.

A player count cannot be claimed either: `players_allowed` is the server's
`--players`, refused outside 1 to 4 before the server starts
(`a_session_is_never_fewer_than_one_player_or_more_than_four`) and clamped
again in the `Slots` constructor. The fifth player is refused because
`admit` returns nothing when the session is full, which is what the
`SERVER_FULL` reason exists for -
`a_session_is_full_at_its_player_count_and_the_next_is_refused` holds the
rule, and nothing sends the refusal yet.

In the other direction, the `LOBBY` reader refusing a `players_allowed`
outside 1 to `most_slots` is what stops a hostile server telling a client it
is in a session of 200.

**What is not defended: one person, four keys, every slot.** Identity is a
key, and nothing counts keys per person, per address or per anything. A
private server's answer is not to give the key out; a public server would need
an allowlist, and there is no plan for one.

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
under sanitizers" is a Phase 6 item and is not done; the terrain decoder
belongs in that corpus, not only the message readers.

**And the name is not a name yet.** `TerrainDataset::name` and `version` are
checked for length (`most_name_bytes`) and nothing else, so a name containing
a path separator or a parent reference reads cleanly. Whatever turns either
into a file path, a cache key or a URL must treat it as hostile: look it up in
a list of datasets this build knows, rather than joining it to a directory.

## What writing this document found

Writing it found six disagreements between the code and the documents, which
is the point of writing it. **Four were fixed on 2026-09-22** and are
described above as the code now stands:

- a `WEATHER` at its limits was 5,419 bytes against a 1,218-byte budget, so a
  full one could never have been sent at all; it is now two messages that fit,
  and a test fills every message to its limits and holds it to the datagram;
- the turbulence severity byte was free to carry anything when the flag
  before it said there was none, and is now held to `00`;
- a slot index was a raw byte with no range check in `LOBBY`, `AIRCRAFT` and
  `CONTROLLER_SWAP`, and 4 or more is now refused;
- the server's receive buffer was 1500 bytes where a datagram is 1232, and is
  now `platform::largest_datagram`.

**Two were not fixed**, and are items at the bottom of
`docs/COMPLETION_PLAN.md` rather than observations here: the reliable layer
believing a forged acknowledgement, and no message rejecting a NaN or an
infinity in an `f64`. Both are described above as live and undefended.

A seventh, found re-reading after the fix, was that three places still said
there were six reliable messages where there are seven - the six the plan
item names are six *things*, and the weather is two of them. That prose was
corrected the same day.

**Nothing in this document is known to disagree with the code today.** That
is a statement about a document that changes no code, so it is only as good
as the next reading of it.

## The order of the work

Each of these is a Phase 6 or Phase 7 item in `docs/COMPLETION_PLAN.md`; the
order is the order in which each defence becomes possible, not a wish list.

1. **The handshake and the sealing**, with libsodium. Until a datagram can be
   authenticated, every rule below has nothing to attach itself to. Design the
   cost of an unauthenticated `HANDSHAKE_INITIATION` at the same time - a
   cookie or a per-address budget - rather than after.
2. **The peer table**, so that a `Reliable` exists only for a key that has
   completed a handshake and been given a slot by `Slots`, and so that a
   `SEALED` body that does not authenticate never reaches `Reliable::received`.
3. **`--timeout` wired**, so a silent peer is let go rather than retransmitted
   at for ever.
4. **The replay window**, per `REQUIREMENTS.md` 6.7, above the sealing and
   below the reliable layer.
5. **Direction, on every message.** A server drops `LOBBY`, `SESSION`,
   `WEATHER`, `WEATHER_ALOFT`, `AIRCRAFT` and `TERRAIN_DATASET` from a client;
   a server drops `HANDSHAKE_RESPONSE`; nobody answers a `REFUSAL` with a
   `REFUSAL`.
6. **A NaN or an infinity refused in every floating-point field**, which is
   an open item in the plan already and wants doing before anything is
   accepted, not after.
7. **`CONTROLLER_SWAP` checked at the server**: the sender's own slot, a
   finite time the server may override with its own clock, and a rate limit.
   This lands with Phase 7.
8. **Input ranges and rates**, when the input messages are designed.
9. **The terrain dataset hash checked before use**, and the dataset name
   looked up rather than joined to a path.
10. **Fuzzing**, on every network parser and on the terrain decoder, under
    sanitizers, in CI.

Until at least the first two are done, the truthful summary of this project's
network security is the sentence at the top: **the server accepts nothing, so
there is nothing to attack and nothing defended.**

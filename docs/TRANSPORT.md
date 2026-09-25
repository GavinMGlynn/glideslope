# Transport

How a glideslope client and a glideslope server speak to each other, byte for
byte, so that a third party could write a working client from this document
alone.

**This describes what exists.** The envelope, the handshake, the sealing,
inputs, state updates and the keepalive are built and tested, and a client
written from this document alone - `tests/doc_client/doc_client.cpp`, by
somebody who read it and the Noise specification and nothing else of this
project - completes a session with the server in `ctest`. The
section "What is not here yet" at the end says exactly what is missing - the
reliable messages are defined here and do not yet travel - so that nothing in
this document is mistaken for something a client could talk to today.

## What the transport does not claim

Said first, because a transport's limits matter more than its features.

- **It does not deliver.** Datagrams are UDP. One may be lost, may arrive
  twice, and may arrive out of order. Inputs and state updates are sent
  expecting that: the latest wins, and losing one costs a frame. Anything that
  must arrive - the lobby, the session, the weather, an aircraft definition,
  the terrain dataset, a controller swap - needs the reliable layer above,
  which is a separate thing and is not described here.
- **It does not order.** Sequence numbers say what came before what; they do
  not hold a datagram back until its predecessor arrives.
- **It does not verify what anyone says about a flight.** There is no
  rollback, no deterministic replay and no server-side proof of a result. The
  server owns every aircraft, and that is the whole of the authority model.
- **It does not hide who is talking.** The envelope is in the clear, so
  anyone watching sees that two machines are speaking glideslope, which
  version, and which kind of datagram. Only the body of a `SEALED` datagram is
  secret.
- **It does not negotiate.** One suite, one version. A datagram of another
  version is refused rather than downgraded to.
- **It does not authenticate a person.** It authenticates a key. Who holds
  that key is the lobby's business.

## The envelope

Every datagram begins with the same 6 bytes.

| offset | size | field | value |
| --- | --- | --- | --- |
| 0 | 4 | magic | `47 4C 44 53`, the ASCII `GLDS` |
| 4 | 1 | version | `01` |
| 5 | 1 | type | see below |

The body follows immediately, and what it is depends on the type.

**The magic is this project's own.** gearstick uses its own, and the two
differ, so a gearstick client and a glideslope server refuse each other at the
first four bytes rather than somewhere deeper and less clearly.

### Types

| value | name | body |
| --- | --- | --- |
| `01` | `HANDSHAKE_INITIATION` | the Noise handshake's first message |
| `02` | `HANDSHAKE_RESPONSE` | the Noise handshake's second message |
| `03` | `SEALED` | ciphertext |
| `04` | `REFUSAL` | one byte, a reason |

A type this version does not know is refused.

### Refusals

A `REFUSAL` is sent in the clear, because there may be no session to seal it
with. Its body is one byte:

| value | name | means |
| --- | --- | --- |
| `00` | `UNKNOWN` | a reason this version does not know |
| `01` | `NOT_THIS_PROTOCOL` | the magic was someone else's |
| `02` | `WRONG_VERSION` | the magic was right and the version was not |
| `03` | `UNKNOWN_TYPE` | a type this version does not know |
| `04` | `TOO_SHORT` | fewer bytes than the envelope needs |
| `05` | `SERVER_FULL` | every slot is taken |
| `06` | `BAD_HANDSHAKE` | the handshake did not complete |

**The magic is checked before the version.** A datagram from another protocol
is told that it is another protocol, rather than being told its version is
wrong - which would be true but useless. **The length is checked before
either**: fewer than 6 bytes is `TOO_SHORT`, whatever they are, and an empty
datagram is not answered at all.

A `REFUSAL` is always 7 bytes - the envelope, with this version, `01`, and
type `04`, then the reason - whatever the datagram it answers said its version
was. The server sends one:

- to a datagram whose envelope it cannot read, with the reason above;
- **instead of a handshake answer**: `SERVER_FULL` when every slot is taken,
  and `BAD_HANDSHAKE` when the initiation does not complete;
- to a `SEALED` datagram from an address that has no session: `BAD_HANDSHAKE`.

It sends nothing back to a `HANDSHAKE_RESPONSE` or a `REFUSAL`, which a server
is never sent, nor to a `SEALED` datagram that does not open under its
address's session. **A refusal is not sealed, so anybody can forge one.** A
client should believe one only while it is waiting for the answer to its
handshake, which is the only time the server has a reason to send it one.

## Starting a session

**A session is an address.** The server knows which session a `SEALED`
datagram belongs to by the address and port it came from and nothing else -
nothing on the wire names a session - so a client sends everything from one
socket. A client whose address changes is a stranger to the server, and must
handshake again once its old session has been let go.

**The handshake is two datagrams**, with no framing of their own: the body of
each is exactly one Noise message (see "Sealing" for the suite).

| datagram | body | size with an empty payload |
| --- | --- | --- |
| `HANDSHAKE_INITIATION`, client to server | Noise message one: the client's ephemeral key (32), its static key sealed (32 + 16), the payload sealed (n + 16) | 96, so a 102-byte datagram |
| `HANDSHAKE_RESPONSE`, server to client | Noise message two: the server's ephemeral key (32), the payload sealed (n + 16) | 48, so a 54-byte datagram |

**The payloads are empty.** The server ignores whatever the initiation's
payload holds and sends an empty one back. **The answer is the admission**:
a client that gets a `HANDSHAKE_RESPONSE` has a slot, and one that gets
`SERVER_FULL` has not. The client is not told which slot; the lobby that
would say so is one of the reliable messages, which do not travel yet.

**A lost handshake is sent again, unchanged.** If no answer comes, the client
sends the same initiation, byte for byte - this project's client every quarter
of a second, until it gives up after however long it was told to wait; the
interval is the client's to choose. **A `HANDSHAKE_RESPONSE` that does not
complete the handshake ends this project's clients' attempt**: both
`glideslope_cli connect` and the client with the window give up on it, and
connect no further. That is a weakness, not a rule of the protocol: a
response is sent in the clear, so anybody who can put a datagram at the
client's address can end its attempt with a forged one. A client may instead
drop such a response and go on waiting for the real one, and the server
neither knows nor cares which it does. The server answers a repeat of the
initiation it has already taken from that address with the same answer and
changes nothing. A *different* initiation from an address that already has a
session is dropped without a word, so that nobody can take a live player's
session with one datagram; the address can start again once the server has
let the old session go.

**An initiation is taken once from each address.** The server remembers
every initiation that has made a session by its first 32 bytes, the client's
ephemeral key, together with the address it came from, and keeps remembering
it after that session has gone. A copy of it arriving then **from the same
address** is dropped without a word: no session, no answer, no refusal. **From
any other address** it is answered as any initiation is, with a new session
and an answer, and that is deliberate: a copy dropped from every address would
let anybody who saw an initiation inject a copy from a spoofed address to
arrive first, and keep its real sender out.

A client sending the same initiation again must have it answered while the
session it made is live, which is what resending until answered does. A
client whose session has gone and that wants another makes a new initiation,
with a new ephemeral key. This project's clients mint a new one for every
connection.

**What it does not claim.**

- **A client whose address changes between resends**, a NAT rebinding its
  port, is two addresses to the server. Each gets a session, and each session
  an aircraft for the same key. The one the client does not use goes quiet
  and is let go after `--timeout`. This is how things were before any of
  this, and it is not defended.
- **The memory is finite.** The server remembers the newest 16,384
  initiations it has taken, about 3 MiB, and forgets them all when it
  restarts. A copy of one older than that, or from before a restart, is
  answered as a new one would be.
- **Nothing can refuse an old one the server has forgotten.** The initiation
  carries no timestamp, as WireGuard's does, that would let it.

**A session ends when the server stops hearing from it.** There is no
goodbye. The server lets a session go when no datagram that opens under it
has arrived for its `--timeout` (10 seconds unless it was told otherwise),
and its slot is free for somebody else; any sealed datagram that opens counts,
so a client sending inputs, or only answering the server's `PING`s, is kept.

## How large a datagram is

**At most 1232 bytes, envelope and all.** IPv6 obliges every path to carry
1280 bytes; 40 of those are its header and 8 more are UDP's, which leaves
1232. Nothing this sends is ever fragmented, and a sender that offers more
than 1232 bytes is refused rather than having them broken up for it.

A datagram that arrives longer than the receiver's buffer is dropped, not
cut: half a datagram is not a datagram.

## How values are written

Inside a body, values are written one after another with no padding and no
alignment.

**Byte order is little-endian**, least significant byte first, for every
integer. Every integer is fixed-width. The machines this runs on are all
little-endian, so nothing is swapped in practice; saying so is what lets
someone else write a client.

| written as | size | notes |
| --- | --- | --- |
| `u8` | 1 | |
| `u16` | 2 | |
| `u32` | 4 | |
| `u64` | 8 | |
| `i16` | 2 | two's complement |
| `i32` | 4 | two's complement |
| `f64` | 8 | its IEEE-754 bits, written as a `u64`; never a NaN or an infinity |
| `f32` | 4 | its IEEE-754 bits, written as a `u32`; never a NaN or an infinity |
| text | 2 + n | a `u16` length, then that many bytes, not terminated |
| bytes | n | a length agreed by the message, then that many bytes |

**No floating-point number is written as a floating-point number.** A double
goes on the wire as its IEEE-754 bits in a `u64`, which has one
representation rather than a compiler's choice of one. So `1.0` is
`00 00 00 00 00 00 F0 3F`.

**`f32` is for a velocity or an angle and never for a position.** A float has
24 bits of mantissa, which at Earth's radius is half-metre steps - so every
world position on this wire is an `f64`, and the only things written as `f32`
are the ones a float holds far better than anything can measure them.

**A number that is not one is not a value this protocol carries.** The eight
bytes of an `f64` can say NaN or infinity as easily as they can say a
latitude, and nothing downstream would notice: a NaN position spreads through
the floating origin and the terrain query, and an infinite duration never
ends. So no message field may hold one, and a reader refuses the whole
message that carries one. Every finite double is allowed, including the
largest, the smallest subnormal and both zeros.

Text is bytes, not characters: the length is in bytes, and it is not
terminated. A reader is entitled to a limit and to refuse anything longer.

### What a reader must do

**Everything that comes off the wire is hostile until it has been read.** A
reader must never read past the end of the datagram it was given, whatever
the lengths inside it say. This project's reader marks itself broken on the
first read it cannot satisfy, answers zero from then on, and is asked once at
the end whether any of it was real; a client may do it another way, but it
must not trust a length it has not checked.

## Reliable messages

Seven things must each arrive, exactly once, in the order they were sent: the
lobby, the session, the weather, an aircraft's definition, the terrain
dataset, a controller swap and which aircraft a client is watching. They go
as **eight kinds of message**, because the weather is two of them. They ride the reliable layer below,
which numbers them and repeats them until they are acknowledged.

**An acknowledgement of a message that was never sent is not believed.** That
layer ignores an acknowledgement above the highest number it has actually put
on the wire, because an endpoint cannot have received what was never sent;
one datagram claiming `FFFFFFFF` would otherwise empty the send queue and
nothing would send those messages again. A real client never hits this, since
it only ever acknowledges what arrived. Telling a *forged* acknowledgement of
a message that was sent from a real one is the sealing's job, and the sealing
is below.

**The weather is two of them**, because it does not fit in one datagram. The
forecast above a station is nineteen pressure levels as this project fetches
it, which is 760 bytes on its own, and a single message carrying that, a
METAR and one microburst comes to 1,235 bytes - more than the 1,218 a
datagram leaves once the envelope and the reliable header are in front of it.
So the report goes as `WEATHER` and the forecast as `WEATHER_ALOFT`.

**Nothing fragments.** A body larger than 1,218 bytes cannot be sent at all:
the socket refuses it and the reliable layer would repeat it for ever. Every
limit below is chosen so that its message fits, and a test fills each message
to its limits and holds it to that.

**Every message begins with one byte saying which kind it is**, and the rest
is that kind's fields in the order given here.

| value | name |
| --- | --- |
| `01` | `LOBBY` |
| `02` | `SESSION` |
| `03` | `WEATHER` |
| `04` | `AIRCRAFT` |
| `05` | `TERRAIN_DATASET` |
| `06` | `CONTROLLER_SWAP` |
| `07` | `WEATHER_ALOFT` |
| `08` | `WATCH` |

A kind this version does not know is not a message, and is refused rather
than skipped.

### Who is flying: `CONTROLLER`

Several messages carry one byte saying who is flying an aircraft.

| value | name | means |
| --- | --- | --- |
| `00` | `NOBODY` | the slot is open |
| `01` | `PERSON` | a person's input |
| `02` | `AI` | an AI pilot |

A value this version does not know makes the message it is in unreadable.

### `LOBBY`

Every slot the server has, and who is in it. It is sent whole rather than as
changes, because it is small and a whole one cannot be misapplied to a state
the receiver did not have.

| written as | field |
| --- | --- |
| `u8` | `01`, the kind |
| `u8` | players allowed, 1 to 4 |
| `u8` | how many slots follow, at most 4 |
| | then, per slot: |
| `u8` | the slot's index |
| `u8` | a `CONTROLLER` |
| text | the player's name, or the AI pilot's; empty for nobody, at most 64 bytes |

### `SESSION`

| written as | field |
| --- | --- |
| `u8` | `02`, the kind |
| `u64` | the session's id |
| text | its name, at most 64 bytes |
| `u64` | when it began, milliseconds since the Unix epoch, UTC |
| `f64` | the simulation's clock, seconds since it began |

### `WEATHER`

**The weather is sent as what it was made from, not as what it became.** A
METAR is a line of text with twenty optional fields in it; re-encoding them
would be twenty chances for two ends to disagree. The raw report goes on the
wire and the receiver parses it with the same rules the sender did.

| written as | field |
| --- | --- |
| `u8` | `03`, the kind |
| text | the raw METAR, at most 256 bytes |
| `f64` | the station's latitude, degrees |
| `f64` | its longitude, degrees |
| `f64` | its elevation, metres above mean sea level |
| `u8` | `01` if the turbulence severity is given, `00` to let the METAR's gusts decide |
| `u8` | the severity, 0 to 7, and `00` when not given |
| `u64` | the air seed: the same report and seed give the same air on every machine |
| `u8` | how many microbursts follow, at most 16 |
| | then, per microburst: |
| `f64` | its latitude, degrees |
| `f64` | its longitude, degrees |
| `f64` | its radius, metres |
| `f64` | its downdraught well above the outflow, metres a second |
| `f64` | when it begins, on the simulation's clock |
| `f64` | how long it lasts, seconds |

At its limits this is 1,062 bytes.

### `WEATHER_ALOFT`

The forecast above the station, sent after a `WEATHER`. A station with no
forecast simply has no `WEATHER_ALOFT`, which is how its absence is said.
The forecast has no raw text of its own, so its levels are numbers.

| written as | field |
| --- | --- |
| `u8` | `07`, the kind |
| text | the forecast's time, `2026-09-22T00:00`, UTC, at most 32 bytes |
| `u8` | how many pressure levels follow, at most 24 |
| | then, per level: |
| `f64` | its pressure, hectopascals |
| `f64` | its height, metres above mean sea level |
| `f64` | the air's velocity towards north, metres a second |
| `f64` | towards east |
| `f64` | its temperature, degrees Celsius |
| `u8` | how many near-ground winds follow, at most 4 |
| | then, per wind: |
| `f64` | the height above the ground, metres |
| `f64` | the air's velocity towards north, metres a second |
| `f64` | towards east |

At its limits this is 1,093 bytes, the largest message there is. The forecast
this project actually fetches - nineteen levels and four winds - is 877.

### `AIRCRAFT`

Which aeroplane an aircraft is, by the server's number for it - the number
state updates carry. Not a slot's index: an AI aircraft has no slot, and a
client must know what every aircraft is to draw it. The server sends one for
every aircraft to each client as it is admitted, and to every client when an
aircraft appears; a number taken out of the sky and later given to another
aircraft is introduced again.

| written as | field |
| --- | --- |
| `u8` | `04`, the kind |
| `u8` | the aircraft's number, as a state update gives it |
| text | the catalogue's id, `c172p`, at most 64 bytes |
| text | the JSBSim model's directory, at most 64 bytes |

### `TERRAIN_DATASET`

The ground both ends must agree on. The collision terrain decides where the
ground is, and a client predicting against a different dataset would drift
from the server for a reason no measurement would explain.

| written as | field |
| --- | --- |
| `u8` | `05`, the kind |
| text | the dataset's name, at most 64 bytes |
| text | its version, at most 64 bytes |
| bytes | its pinned SHA-256, exactly 32 bytes |

### `CONTROLLER_SWAP`

An aircraft handed between a person and an AI pilot, by the server's number
for it. **It travels both ways.** A client asks for its own aircraft to be
handed to the AI pilot (`02`) or back to it (`01`), with the time written as
nought; the server honours it only for that client's own aircraft, and says
so to every client, with the time it took effect. A request for another's
aircraft, or to `NOBODY`, is acknowledged and nothing more.

While the AI flies an aircraft, the server applies none of its client's
inputs, and a state update gives its controller as `AI`. A client whose
aircraft has been handed over stops predicting it and draws it from the
updates like any other; given it back, it predicts again from the next
update carrying its motion, and until the server has applied an input sent
since, it knows nothing of how it is being flown. The server brings the
controls from the AI's to the pilot's at the pace of a hand - full travel in a
second - rather than jumping them, so for that second the pilot's inputs are
not yet all it flies.

| written as | field |
| --- | --- |
| `u8` | `06`, the kind |
| `u8` | the aircraft's number, as a state update gives it |
| `u8` | the `CONTROLLER` it is going to |
| `f64` | when it takes effect, on the simulation's clock |

### `WATCH`

**Which aircraft a client is riding along in**, sent by a client: the
server's number for it, or `FF` for none. It changes only what that client is
told - its own state updates carry the watched aircraft's controls (below).
Any number reads; one that is not flying is told nothing. A client may watch
any aircraft, its own and other players' among them.

| written as | field |
| --- | --- |
| `u8` | `08`, the kind |
| `u8` | the aircraft's number, as a state update gives it, or `FF` for none |

### What a reader must refuse

A count larger than the limit above is refused rather than trimmed: a sender
asking for more than this protocol allows is not one to guess at. A message
with anything left over after its last field is refused, so that nothing can
be hidden behind one. A message that has been cut short is refused. A slot
index of 4 or more in a `LOBBY` is refused, because a session has at most four. The
turbulence severity byte must be `00` when the flag before it says there is
none, because a byte nobody reads is a byte that can carry anything. **An
`f64` holding a NaN or an infinity is refused**, in any field of any message,
exactly as a bad count is. None of these may be read as a different kind.

## Inputs

**A client sends its inputs, never its state.** They are not sent reliably,
they are sent repeatedly: a lost input frame is worth nothing a moment later,
so retransmitting one until it is acknowledged would deliver it too late to
use and cost a round trip to find out. Every packet instead carries the last
**4** frames.

**The bound, stated.** A frame rides in the packet of its own number and the
three after it, so it survives losing any three of those four. Lose all four
and it is gone: there is nothing left carrying it. The frames at the very end
of a stream have less cover, because the packets that would have carried them
have not been sent yet.

| written as | field |
| --- | --- |
| `u8` | `02`, the kind (see "What is inside a sealed body") |
| `u32` | the newest frame's sequence number, from 1 |
| `u8` | how many frames follow, 1 to 4 |
| | then, per frame, oldest first: |
| `i16` x17 | the seventeen controls |

The frames are consecutive, so only the newest sequence is sent: the oldest
is the newest less one fewer than the count.

**Every control is a 16-bit fraction of -1 to 1**, written as a `u16` holding
its two's-complement bits. `-32767` is -1 and `32767` is 1, both exact, so a
control held hard over arrives hard over. The step is about 3e-5, far finer
than any stick, and a quarter of what a double would cost.

**A value is rounded to the nearest step**: the `i16` is `v x 32767` rounded
to the nearest whole number, halves away from nought, and read back as that
over 32767.

**A client must fly what it sent, not what its stick said.** The client
predicts its own aircraft by running the flight model on its own inputs; if it
flew the stick's exact number while the server flew the rounded one, the two
would diverge for a reason no measurement could explain. So the client rounds
first and flies the rounded value.

The seventeen controls, in order, **every one of them sent in every frame**:
a frame is the whole cockpit, and the server flies exactly what it says. A
control a client does not move must be sent at the value that leaves the
aeroplane alone - which for several is not nought.

| # | control | range used | means | leave it at |
| --- | --- | --- | --- | --- |
| 0 | elevator | -1 to 1 | +1 nose up | 0 |
| 1 | aileron | -1 to 1 | +1 rolls right (right wing down) | 0 |
| 2 | rudder | -1 to 1 | +1 yaws the nose left, as JSBSim's own command does | 0 |
| 3 | throttle | 0 to 1 | 1 full power | as wanted |
| 4 | mixture | 0 to 1 | 1 full rich; 0 cuts the engine | 1 |
| 5 | flaps | 0 to 1 | 1 fully down | 0 |
| 6 | left brake | 0 to 1 | 1 full | 0 |
| 7 | right brake | 0 to 1 | 1 full | 0 |
| 8 | pitch trim | -1 to 1 | +1 nose up | 0 |
| 9 | propeller | 0 to 1 | 1 highest rpm | 1 |
| 10 | gear | 0 or 1 | 1 down, 0 up | 1 |
| 11 | supercharger | 0 or 1 | 1 automatic, 0 held in low gear | 1 |
| 12 | speedbrake | 0 to 1 | 1 fully out | 0 |
| 13 | throttle offset, port engine | -1 to 1 | added to the throttle for that engine | 0 |
| 14 | throttle offset, starboard engine | -1 to 1 | the same | 0 |
| 15 | cooling flaps, port engine | 0 to 1 | 1 open | 0 |
| 16 | cooling flaps, starboard engine | 0 to 1 | the same | 0 |

An aeroplane without a control ignores it. **Nothing is clamped on the way in**
but each engine's throttle-plus-offset, which is held to 0 to 1: a value outside
a control's range reaches the flight model as it is, so a client must not send
one. **Frames of all zeros are not "no input"**: they cut the mixture and raise
the gear. The wire itself does not know what the controls are - the flight
model hands them over as a flat list and takes them back the same way, which is
what keeps the list above and the simulation from drifting apart when a control
is added; a test holds the count to seventeen.

### What is inside a sealed body

**Everything that is not a handshake or a refusal travels inside a `SEALED`
datagram**, and there is more than one kind of thing to send. So the plaintext
inside the seal begins with one byte saying which kind it is. That byte is
inside the seal, not in the envelope, because nothing outside a session needs
to know which of these a datagram is.

The kind byte is the first byte of the plaintext and belongs to what
follows it: the input packet's and the state update's tables below begin with
it, and there is no second one.

| value | name | what follows |
| --- | --- | --- |
| `01` | `RELIABLE` | the reliable layer's datagram, carrying one message |
| `02` | `INPUTS` | an input packet |
| `03` | `STATE` | a state update from the server |
| `04` | `PING` | `u64`, a token |
| `05` | `PONG` | `u64`, the token from the `PING` it answers |

**A client refuses nothing.** Refusals are the server's, sent to strangers;
a client drops what it cannot read, in silence.

A kind this version does not know is **ignored, not refused**: a client of a
later version may send one, and dropping its session for it would make every
future addition a breaking change. A kind it does know but cannot read - a
`PING` that is not nine bytes - is ignored the same way.

**`PING` and `PONG` are the keepalive and the round trip.** The server knocks
on each connection once a second; the other end sends the same token straight
back; the server takes the time between as the round trip and draws it on its
dashboard. Only the token the server has outstanding counts, so an old or
invented one tells it nothing. **A client that answers is also a client the
server does not let go** when `--timeout` comes round, which is why the
knocking is the server's job: the server is the one deciding who has gone.

**`RELIABLE` is numbered here and nothing sends it yet.** The numbering is
fixed before anything uses it so that it cannot move later. A client sends
`INPUTS`, `PING` and `PONG`; the server sends `STATE`, `PING` and `PONG`, and
ignores a `STATE` or a `RELIABLE` from a client.

### State updates

**What the server sends back, and why the server is authoritative.** A client
sends inputs and predicts its own aircraft from them; the server flies every
aircraft for real and says, 25 times a second, where they all are. The
client reconciles its prediction against its own aircraft's line and
interpolates everybody else's.

It rides inside a `SEALED` datagram as the kind `STATE` (`03`) - the kind
byte is the table's first row, not a second byte in front of it - and **it is
not reliable and must not be**: a state update is worth nothing once a newer one
exists, so repeating a lost one would deliver stale positions late. Each is
sent once; the sealing's replay window throws away an old one that arrives out
of order.

| written as | field |
| --- | --- |
| `u8` | `03`, the kind |
| `f64` | the simulation's clock, seconds since the session began |
| `u32` | the newest input sequence from this client the server has applied |
| `u8` | `your_aircraft`: the number, as below, of this client's own aircraft, or `FF` for none - its number, not its place in the list |
| `u8` | how many aircraft follow, at most 20 |

Then, for each aircraft:

| written as | field |
| --- | --- |
| `u8` | the server's number for this aircraft, steady for as long as it flies |
| `u8` | who is flying it, a `CONTROLLER` |
| `u8` | whether it is flying or a wreck: `00` flying, `01` wrecked |
| `f64` | its position, Earth-centred and Earth-fixed, metres, x |
| `f64` | the same, y |
| `f64` | the same, z |
| `f32` | its velocity in the same frame, metres a second, x |
| `f32` | the same, y |
| `f32` | the same, z |
| `f32` | its heading, degrees: true, 0 to 360, clockwise from north |
| `f32` | its pitch, degrees: positive nose up |
| `f32` | its roll, degrees: positive right wing down |

After the last aircraft, **this client's own aircraft's motion**, so that its
prediction can be put right (see "Predicting your own aircraft" below):

| written as | field |
| --- | --- |
| `u8` | `01` if the motion follows, `00` if this client has no aircraft |
| `f64` | its position, Earth-centred and Earth-fixed, metres, x |
| `f64` | the same, y |
| `f64` | the same, z |
| `f32` x4 | its attitude, a unit quaternion from north-east-down to the body, scalar first |
| `f32` x3 | its velocity relative to the Earth along its own axes - forward, right, down - metres a second |
| `f32` x3 | its rotation rates about those axes - roll, pitch, yaw - radians a second |

A flag other than `00` or `01`, or a NaN or infinity in any of the thirteen
numbers, makes the packet unreadable.

**Last, the controls of the aircraft this client is watching** (`WATCH`),
after a flag of their own. Each is a 16-bit fraction, as inputs are, of -1 to
1 or 0 to 1; there is no room in a full packet for every aircraft's, so a
client is told of the one it watches.

| written as | field |
| --- | --- |
| `u8` | `01` if the controls follow, `00` if this client watches nothing |
| `u8` | the watched aircraft's number |
| `i16` | the aileron, right positive, -1 to 1 |
| `i16` | the elevator, back positive, -1 to 1 |
| `i16` | the rudder, right positive, -1 to 1 |
| `i16` | the first engine's throttle, 0 to 1 |
| `i16` | the flaps, 0 to 1 |
| `i16` | the gear, down at 1 - or `-32768` where it does not retract |

A flag other than `00` or `01`, or `-32768` in any control but the gear,
makes the packet unreadable.

**Two of the fields are meant for one client and not for the others**, which
is why a state update is sealed to each connection separately rather than
built once and sent to all: the input sequence and the index of this client's
own aircraft. A client cannot reconcile its prediction without knowing which
line is its own.

**Positions are Earth-centred, Earth-fixed and double precision**, because the
whole world is in play: there is no session origin for an aircraft to be near,
and two aircraft in one session may be on opposite sides of the planet. The
number for an aircraft is not a slot - an AI aircraft has no slot - and it is
the server's to hand out.

**20 aircraft is the most one can hold**, which is the four players
`--players` allows and the sixteen AI aircraft `--ai` allows. A packet that
full, with the client's own motion and a watched aircraft's controls, is
1,114 bytes, and 1,144 with the
envelope and the sealing in front of it, inside the 1,232 a datagram holds; a test fills one to its limits and
holds it to that.

**A server with nothing to fly sends no state updates at all.** Until the
server is flying aircraft - AI aircraft, or a flight plan the players' aircraft
are given - a client is admitted, knocked on and kept, and never told where
anything is. A client is given an aircraft when it is admitted if the server
has one to give, and `FF` otherwise.

**A wreck** is an aircraft that has crashed - into another, or into the ground
harder than its gear takes. The server decides it and says it here, and the
aircraft stays where it hit, a wreck, for a few seconds; then it flies again
from where it started, under the same number, `00` again.

**A reader refuses**: a kind that is not `03`, fewer bytes than the fields
need, any byte left over at the end, more than 20 aircraft, a controller or a
condition this version does not know, and any NaN or infinity in any of the ten numbers. It
does not refuse a `your_aircraft` that names no aircraft in the packet: a
client that cannot find itself has no aircraft yet, which is what `FF` says.

### The session's clock

**Every update is stamped with the simulation's clock, and a client must not
assume that clock keeps time with its own.** A server that cannot keep real
time - a slow machine, a loaded one - runs its clock slower, and a client that
took the fastest update it ever heard and counted on from there in real time
would get further ahead of the server every second, drawing other aircraft
where nothing it had heard put them. Fit the rate as well as the offset, over
the last few seconds of updates; the update that says the clock is furthest
on, at that rate, is the one that waited least in the network.

### Predicting your own aircraft

**A client flies its own aircraft ahead of the server**, so that its controls
answer at once, and puts it right when the server's word arrives - a round
trip late. The server's word is the motion at the end of the state update,
and the newest input sequence it had applied. The client sets its own flight
model's position, attitude, velocity and rates to that motion, and flies it
forward again through every input it has sent since that sequence. How far
that moves the aircraft is the correction; a small one is hidden by blending
it in, and one larger than about a wingspan is not.

**The first update is already a trip old**, so a client should keep the
inputs it sends before it has heard one, and fly those the server has not
yet applied forward from it. Until the server has applied an input of the
client's, though, the client is joining rather than predicting: the server
flew the aircraft for that trip on no input of the client's, and the client
cannot know how.

**What the server does not say** is how far into its newest input it had
flown when it took the motion. It flies an input from when that input arrives,
so an input that jitter makes late is flown late, and a client that assumes
every input lasted as long as it did on its own screen is wrong by up to the
aeroplane's speed times the jitter and an input's length: at 50 m/s, 60 ms of
jitter and inputs thirty a second, 5 m.

**An update can arrive after a newer one** - the network reorders them - and
a client must not put its aircraft back to it: it has already been put right
by the newer one, and the inputs the older one would have it fly again are
gone.

Nothing else about the aircraft is sent - its engines, its controls' positions,
its fuel: the client, flying the same inputs, already has them, and sending
them would make every update many times larger for nothing.

## Sealing

**A `SEALED` datagram's body is ciphertext** under the keys the handshake
agreed. Each end seals under the key it sends with and opens under the key it
receives with, so the two directions never share a key and a datagram cannot
be reflected back at the end that sent it.

| written as | field |
| --- | --- |
| `u64` | the sequence number, from 0, one per direction |
| bytes | the body, ChaCha20-Poly1305 sealed, with its 16-byte tag |

The sequence number is the cipher's nonce: four bytes of nought then the
number, least significant byte first, as ChaCha20-Poly1305's twelve. It is
sent in the clear because it is not a secret, and **its eight bytes as written
are the additional data the tag covers** - those eight and nothing else, not
the envelope - so changing it only stops the body opening.

**Which key is which.** Noise's `Split()` gives two keys. The first is the
client's sending key and the server's receiving key; the second is the
server's sending key and the client's receiving key.

**A replay window of 64.** The opener keeps the highest number it has opened
and a bitmap of the 64 before it. A number it has already opened is refused,
and so is one 64 or more behind the newest: 63 behind is the oldest opened. **That is the stated limit**: a
datagram delayed by more than the window cannot be told from a replay, and is
refused rather than guessed at. Only a body that opens moves the window, so a
datagram that is not ours cannot push the window forward.

**The handshake** is `Noise_IK_25519_ChaChaPoly_BLAKE2b`, exactly as the Noise
Protocol Framework (revision 34) defines it, with an empty prologue: any
standard Noise implementation of that suite completes it, and this project's
matches the Noise community's `cacophony` known-answer vector for it byte for
byte. `REQUIREMENTS.md` 6.7 names BLAKE2s; libsodium has no BLAKE2s, BLAKE2b
is a hash the Noise specification defines, and the choice is recorded in
`src/net/handshake.hpp` and `docs/PROJECT_STATUS.md`. The initiator must
already know the responder's static public key, which the server prints at
startup.

## What is not here yet

- **Five of the eight reliable messages.** `AIRCRAFT`, `CONTROLLER_SWAP` and
  `WATCH` travel, inside `RELIABLE`. The lobby, the session, the weather and the
  terrain dataset are defined and encoded, and nothing sends them yet.
- **Any check on what a client sends.** A client's inputs reach its aircraft
  with no range check and no rate limit: a value outside -1 to 1 cannot be
  written, because the wire is a 16-bit fraction, but nothing stops a client
  sending as fast as it likes. `docs/THREATS.md` says what that costs.
- **Choosing an aeroplane.** A player flies whatever the server's flight plan
  flies, and starts where it starts. `REQUIREMENTS.md` asks for the player to
  pick, and that is a session setting nobody has written.
- **Rate limiting, and the cookie an overloaded server would demand.** A
  server does an X25519 operation for any stranger that sends it an
  initiation. `docs/THREATS.md` says what that costs and what would bound it.

What a client written from this document **can** do today: complete the
handshake with a server whose public key it was given, be admitted to a slot,
seal and open datagrams under the keys that handshake agreed, answer the
server's knocking so that it stays in its slot and the server can measure the
round trip, **read where every aircraft is 25 times a second, learn what
aeroplane each one is, and fly its own aircraft by sending inputs**, and be
let go when it stops. What it cannot do is be told anything else: it never
learns the lobby, the weather or the terrain dataset.

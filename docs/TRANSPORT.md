# Transport

How a glideslope client and a glideslope server speak to each other, byte for
byte, so that a third party could write a working client from this document
alone.

**This describes what exists.** The envelope and the encoding below are built
and tested. The handshake and the sealing are specified in
`REQUIREMENTS.md` section 6.7 and are **not built yet**; the section "What is
not here yet" at the end says exactly what is missing, so that nothing in this
document is mistaken for something a client could talk to today.

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
wrong - which would be true but useless.

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

Six things must each arrive, exactly once, in the order they were sent: the
lobby, the session, the weather, an aircraft's definition, the terrain
dataset and a controller swap. They go as **seven kinds of message**, because
the weather is two of them. They ride the reliable layer below,
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

Which aeroplane a slot is flying.

| written as | field |
| --- | --- |
| `u8` | `04`, the kind |
| `u8` | the slot's index |
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

An aircraft handed between a person and an AI pilot.

| written as | field |
| --- | --- |
| `u8` | `06`, the kind |
| `u8` | the slot's index |
| `u8` | the `CONTROLLER` it is going to |
| `f64` | when it takes effect, on the simulation's clock |

### What a reader must refuse

A count larger than the limit above is refused rather than trimmed: a sender
asking for more than this protocol allows is not one to guess at. A message
with anything left over after its last field is refused, so that nothing can
be hidden behind one. A message that has been cut short is refused. A slot
index of 4 or more is refused, because a session has at most four. The
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

**A client must fly what it sent, not what its stick said.** The client
predicts its own aircraft by running the flight model on its own inputs; if it
flew the stick's exact number while the server flew the rounded one, the two
would diverge for a reason no measurement could explain. So the client rounds
first and flies the rounded value.

The seventeen controls, in order: elevator, aileron, rudder, throttle,
mixture, flaps, left brake, right brake, pitch trim, propeller, gear,
supercharger, speedbrake, the two throttle offsets, and the two cooling flaps.
**The wire does not know what they are**: the flight model hands them over as
a flat list and takes them back the same way, which is what keeps this
document and the simulation from drifting apart when a control is added.

### What is inside a sealed body

**Everything that is not a handshake or a refusal travels inside a `SEALED`
datagram**, and there is more than one kind of thing to send. So the plaintext
inside the seal begins with one byte saying which kind it is. That byte is
inside the seal, not in the envelope, because nothing outside a session needs
to know which of these a datagram is.

| value | name | what follows |
| --- | --- | --- |
| `01` | `RELIABLE` | the reliable layer's datagram, carrying one message |
| `02` | `INPUTS` | an input packet |
| `03` | `STATE` | a state update from the server |
| `04` | `PING` | `u64`, a token |
| `05` | `PONG` | `u64`, the token from the `PING` it answers |

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

**`RELIABLE`, `INPUTS` and `STATE` are numbered here and nothing sends them.**
The numbering is fixed before anything uses it so that it cannot move later;
what each one would carry is defined above, or, for `STATE`, not at all.

### State updates

**What the server sends back, and why the server is authoritative.** A client
sends inputs and predicts its own aircraft from them; the server flies every
aircraft for real and says, 20 to 30 times a second, where they all are. The
client reconciles its prediction against its own aircraft's line and
interpolates everybody else's.

It rides inside a `SEALED` datagram as `Inside::state` (`03`), and **it is not
reliable and must not be**: a state update is worth nothing once a newer one
exists, so repeating a lost one would deliver stale positions late. Each is
sent once; the sealing's replay window throws away an old one that arrives out
of order.

| written as | field |
| --- | --- |
| `u8` | `03`, the kind |
| `f64` | the simulation's clock, seconds since the session began |
| `u32` | the newest input sequence from this client the server has applied |
| `u8` | which aircraft below is this client's own, by index, or `FF` for none |
| `u8` | how many aircraft follow, at most 20 |

Then, for each aircraft:

| written as | field |
| --- | --- |
| `u8` | the server's number for this aircraft, steady for as long as it flies |
| `u8` | who is flying it, a `CONTROLLER` |
| `f64` | its position, Earth-centred and Earth-fixed, metres, x |
| `f64` | the same, y |
| `f64` | the same, z |
| `f32` | its velocity in the same frame, metres a second, x |
| `f32` | the same, y |
| `f32` | the same, z |
| `f32` | its heading, degrees |
| `f32` | its pitch, degrees |
| `f32` | its roll, degrees |

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
full is 1,015 bytes, and 1,045 with the envelope and the sealing in front of
it, inside the 1,232 a datagram holds; a test fills one to its limits and
holds it to that.

**A reader refuses**: a kind that is not `03`, fewer bytes than the fields
need, any byte left over at the end, more than 20 aircraft, a controller this
version does not know, and any NaN or infinity in any of the ten numbers. It
does not refuse a `your_aircraft` that names no aircraft in the packet: a
client that cannot find itself has no aircraft yet, which is what `FF` says.

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
sent in the clear because it is not a secret, and it is the additional data
the tag covers, so changing it only stops the body opening.

**A replay window of 64.** The opener keeps the highest number it has opened
and a bitmap of the 64 before it. A number it has already opened is refused,
and so is one more than 64 behind the newest. **That is the stated limit**: a
datagram delayed by more than the window cannot be told from a replay, and is
refused rather than guessed at. Only a body that opens moves the window, so a
datagram that is not ours cannot push the window forward.

**The handshake** is `Noise_IK_25519_ChaChaPoly_BLAKE2b`. `REQUIREMENTS.md`
6.7 names BLAKE2s; libsodium has no BLAKE2s, BLAKE2b is a hash the Noise
specification defines, and the choice is recorded in
`src/net/handshake.hpp` and `docs/PROJECT_STATUS.md`. The initiator must
already know the responder's static public key, which the server prints at
startup.

## What is not here yet

- **Noise's own BLAKE2b.** The handshake is named
  `Noise_IK_25519_ChaChaPoly_BLAKE2b`, and Noise's BLAKE2b has a 64-byte
  hash. This one uses BLAKE2b cut to 32 bytes for its hash, its chaining key
  and its keys, and the protocol name - 33 bytes - is cut to the first 32 to
  start the hash with, where Noise would pad it to 64. Both ends agree, so it
  works; **a client built on a standard Noise library would not complete the
  handshake**, and until this is changed the name above says more than the
  bytes do. Found 2026-09-23, when copying the 33-byte name into a 32-byte
  array showed up as a buffer overflow under MSVC's debug checks.
- **Three of the five things a sealed body can hold.** `RELIABLE`, `INPUTS`
  and `STATE` are numbered above and nothing sends or reads them, so the seven
  messages and the input packets - both defined and encoded - do not yet
  travel. `PING` and `PONG` do.
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
round trip, **read where every aircraft is 25 times a second, and fly its own
aircraft by sending inputs**, and be let go when it stops. What it cannot do
is be told anything: none of the seven messages travels, so it never learns
the lobby, the weather or what aeroplane anybody is in.

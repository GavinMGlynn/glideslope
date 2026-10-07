# Transport

How a glideslope client and a glideslope server speak to each other, byte for
byte, so that a third party could write a working client from this document
alone.

**This describes what exists.** The envelope, the handshake, the sealing,
inputs, state updates, the keepalive and the goodbye are built and tested,
and a client written from this document alone - `tests/doc_client/doc_client.cpp`, by
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
- **It does not keep the version with the layout, yet.** Until a first
  release, a message's layout changes without the version changing, so a build
  of another day may not read today's updates - it refuses them as the wrong
  length, and says nothing more useful.
- **It does keep the version with the ground.** The collision ground is the
  DEM with every runway made its own surface, from the runway strips the
  build carries (`assets/runways/strips.csv`) by rules that are numbered
  (`world::collision_ground_rules`). A client predicting on other ground than
  its server's would drift for no reason it could see, so either changing
  moves the version, and builds on different ground refuse each other at the
  envelope as `WRONG_VERSION`. A test holds the version, the rules' number
  and the strips' SHA-256 together. Version `02` is the first with runways
  flattened: rules 1, strips SHA-256
  `6c1ba3c3e6dc3bf19a6b0a402a00734b4d886b95e40c1097f1c69a301576898f`.
  Version `03` is on the same ground, and is the first whose server tells
  every client the session, the lobby, the ground it collides on and the
  weather it flies (below, "What a client is told on joining"); its
  `WEATHER` carries three fields more than `02`'s. Version `04` is on the
  same ground, and is the first whose initiation's payload is read: the
  aeroplane a player asks for (below, "Starting a session"); and the first
  whose state update names the engine stopped (below, "State updates").
  Version `05` is on the same strips by rules 2: where runways overlap, the
  nearest runway's surface wins, so one runway's shoulder no longer pulls
  another's pavement. Version `06` is on the same ground, and is the first
  whose state update carries the watched aircraft's speedbrake lever (below,
  "State updates"). Version `07` is on the same ground, and is the first
  whose server says when it refuses a take-over (`TAKE_OVER_REFUSED`, below).
  Version `08` is on the same ground, and is the first whose
  `CONTROLLER_SWAP` may ask for, and announce, the learnt landing (below).
  Version `09` is on the same ground, and is the first whose server says to
  the client that asked when it refuses the learnt landing, and why
  (`LEARNT_LANDING_REFUSED`, below).
- **It does not authenticate a person.** It authenticates a key. Who holds
  that key is the lobby's business.

## The envelope

Every datagram begins with the same 6 bytes.

| offset | size | field | value |
| --- | --- | --- | --- |
| 0 | 4 | magic | `47 4C 44 53`, the ASCII `GLDS` |
| 4 | 1 | version | `09` |
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
| `07` | `DROPPED` | the operator dropped this key; it is refused for the rest of the server's run |

**The magic is checked before the version.** A datagram from another protocol
is told that it is another protocol, rather than being told its version is
wrong - which would be true but useless. **The length is checked before
either**: fewer than 6 bytes is `TOO_SHORT`, whatever they are, and an empty
datagram is not answered at all.

A reason a client does not know is read as `UNKNOWN`, so `DROPPED`, added
after the other six, is refused as an unknown reason by a client older than
it: it still stops that client's attempt.

A `REFUSAL` is always 7 bytes - the envelope, with this version, `09`, and
type `04`, then the reason - whatever the datagram it answers said its version
was. The server sends one:

- to a datagram whose envelope it cannot read, with the reason above;
- **instead of a handshake answer**: `SERVER_FULL` when every slot is taken
  and the initiation is not answered - a stranger's key, a claim of a
  player's key that does not complete, or anything past a full server's
  budget of reads (see "Starting a session") - `BAD_HANDSHAKE` when the
  initiation does not complete on a server with a slot free, and `DROPPED` when it completes from a static key the
  operator has dropped (see "Leaving");
- to a `SEALED` datagram from an address that has no session: `BAD_HANDSHAKE`.

It sends nothing back to a `HANDSHAKE_RESPONSE` or a `REFUSAL`, which a server
is never sent, nor to a `SEALED` datagram that does not open under its
address's session. **A refusal is not sealed, so anybody can forge one.** A
client should believe one only while it is waiting for the answer to its
handshake, or once its session has already gone quiet - which are the two
times the server has a reason to send it one - and only from the server's
own address. **A session gone quiet** is one under which nothing has opened
for three seconds: three of the server's `PING`s and two of the client's own
(below) unanswered. A `BAD_HANDSHAKE` heard then says what the silence already
did, that the server has let the session go; both of this project's clients
then join again by themselves (see "Starting a session"). One heard while
the session is working is ignored, and so is any other reason then.

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

**The initiation's payload is the aeroplane asked for** (version `04`,
2026-10-06), sealed with the initiation - no message of its own, because a
server gives a player an aircraft as it admits them:

| field | bytes | |
|---|---|---|
| length | 1 | 1 to 32 |
| id | length | a catalogue id: `a`-`z`, `0`-`9`, `-` and `_` |

and nothing after. **An empty payload asks for nothing**: the player flies the
server's plan's aeroplane. A payload that does not read so - a length of 0 or
over 32, a byte outside those, anything trailing - is read as none, and so is
an id the server's catalogue does not hold, which it says on its standard
output; either way the player is admitted, flying the plan's. The id is
looked up among the catalogue's ids, never joined to a path. The aeroplane
starts where the plan's starts, at its own starting speed; every client is
told what it is by its `AIRCRAFT` message, as any aircraft is. A second
session for a key already in shares the key's aircraft, whatever it asks for;
a client joining again asks again, in its new initiation. The answer's
payload is empty. **The answer is the admission**:
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

**A key has one slot and one aircraft, however many addresses it has
sessions from.** An initiation for a key that already has a session at
another address - a client started again from a new port while its old
session is live, or anybody's replay of a captured initiation - makes a
session that shares the key's aircraft rather than being given another. **It
takes over at the first datagram sealed under it that opens**, not at the
answer: the server then sends each older session on the key `LEAVING`, so
that a client still running there stops rather than joining again, and lets
it go, leaving the slot and aircraft to the new one. A replayer holds no
ephemeral secret, cannot seal, and so takes nothing.

**Until something sealed under it has opened, a session is sent nothing but
its handshake answer** (and that answer again, for a resent initiation): no
state updates, no `PING`s, no reliable messages. **So a client seals
something at once, and keeps sealing something until anything opens under
the session** - one sealed datagram may be lost, and the server will say
nothing until one arrives. This project's clients send a `PONG` nobody
pinged for as the handshake completes, which the server ignores; the
command-line client sends inputs or a `PING` of its own after that, and
`net::ClientSession` a sealed `PING` every quarter of a second until anything
has opened. A resent initiation does not count as hearing from a session,
so one that seals nothing is let go `--timeout` after it was admitted,
however often its initiation is resent. A key may have at most two such
unproven sessions; a third lets the oldest go. The slot and the aircraft go
with the key's last *proven* session, and any unproven one left on the key
goes with them; an operator's drop lets go every session on the key.

**On a full server a key already in is let in again at once**: a client
started again from a new port, its old session not yet let go, is answered
like any other session for that key - sharing its slot and aircraft, and
taking over at the first datagram sealed under it that opens - rather than
refused until the old one's `--timeout` has run out. A full server reads an
initiation only as far as the initiator's static key, which is unsealed under
the first of IK's X25519 operations (`es`), and goes on only if that key is
one of its players'. Any other initiation is refused `SERVER_FULL` there,
before the second; and so is one claiming a player's key that does not then
complete - a full server never answers `BAD_HANDSHAKE` to an initiation, so
that which refusal came back says nothing about whether a key is a player's
here. **A full server reads at most 32 initiations a second**, with as many at
once and no more; one past that is refused `SERVER_FULL` unread, as every
initiation to a full server once was. So a player started again while
somebody floods the server may still be refused, and waits out the timeout
as before. Nothing on the wire changes: the same datagrams, the same answers.

A client sending the same initiation again must have it answered while the
session it made is live, which is what resending until answered does. **Not
defended: a client that loses every answer for a whole `--timeout`.** Its
resends are answered but, being copies, keep nothing alive; its session is
let go `--timeout` after it was admitted, and a resend after that, from the
same address, is dropped as a copy already taken. That client gives up
waiting for an answer and must start again with a new initiation. A
client whose session has gone and that wants another makes a new initiation,
with a new ephemeral key. This project's clients mint a new one for every
connection.

**A client the server has let go joins again by itself** - both of this
project's clients do, the command-line one and the one with the window. Having
heard a refusal of a session gone quiet (see "Refusals"), it makes a new
initiation with the same static key and a new ephemeral one, from the same
socket, and resends it every quarter of a second until it is answered, a
minute the most. It is not a copy, so the server takes it: a new session, a
slot, and a new aircraft, the old one having gone as `--on-leave` said. A
`BAD_HANDSHAKE` while it waits is not an answer - sealed datagrams sent under
the old session may still be on their way to be refused - and does not end
the attempt; `SERVER_FULL` and `DROPPED` from the server's address end it,
with the reason. **It knocks on the old session while it waits**: with
each initiation it sends, it sends a sealed `PING` under the old session's
keys, carrying a token chosen at random for this attempt. If the `PONG` to
that token opens under the old keys, the session was never gone, and the
client goes back to it - a server that still has it drops the new initiation
from that address without a word. **Nothing else that opens under the old
keys is a reason to go back**: an update the server sealed before it let the
session go, held on the way, opens as well as a live one, and is dropped. A
server that has let the session go answers the knock with nothing - refused
`BAD_HANDSHAKE` before the new initiation arrives, opening under nothing
after - so the client takes the new session. **Back in it, nothing starts again**: the
server's count of the client's inputs and both reliable streams are where the
session left them, so the client numbers its inputs on from the last it sent
- inputs numbered afresh would all be older than the newest the server had
applied, and dropped. A client told it was dropped does not join
again at all. **A client that ends while joining again says no goodbye**: it
has no session to seal one under, and the old one's is already gone at the
server, so there is nothing for a `LEAVING` to end. A reliable message
queued under the old session and not yet acknowledged is lost with it, and
this project's clients queue none while joining again - a new session is a
new reliable stream.

**What it does not claim.**

- **A client whose address changes between resends**, a NAT rebinding its
  port, is two addresses to the server. Each gets a session, sharing the
  key's one aircraft (below). The one the client does not use goes quiet and
  is let go after `--timeout`. It is not defended.
- **The memory is finite.** The server remembers the newest 16,384
  initiations it has taken, about 3 MiB, and forgets them all when it
  restarts. A copy of one older than that, or from before a restart, is
  answered as a new one would be.
- **Nothing can refuse an old one the server has forgotten.** The initiation
  carries no timestamp, as WireGuard's does, that would let it.

**A session ends when its client says goodbye, or when the server stops
hearing from it.** A client that is leaving sends `LEAVING` (see "Leaving"
below), and the server lets the session go at once. Otherwise the server lets
a session go when no datagram that opens under it has arrived for its
`--timeout` (10 seconds unless it was told otherwise) - a client that crashed,
or whose goodbye was lost. Either way its slot is free for somebody else, and
its aircraft goes as the server's `--on-leave` says; any sealed datagram that
opens counts as hearing from it, so a client sending inputs, or only answering
the server's `PING`s, is kept - once its session is proven. Until something
sealed under it has opened, the server sends it no `PING`s to answer, and
nothing else, not even a resent initiation, counts.

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

Nine things must each arrive, exactly once, in the order they were sent: the
lobby, the session, the weather, an aircraft's definition, the terrain
dataset, a controller swap, which aircraft a client is watching, a
copilot's route and a take-over refused. They go as **ten kinds of message**,
because the weather is two of them. They ride the reliable layer below,
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
| `09` | `COPILOT_ROUTE` |
| `0A` | `TAKE_OVER_REFUSED` |
| `0B` | `LEARNT_LANDING_REFUSED` |

A kind this version does not know is not a message, and is refused rather
than skipped.

### Who is flying: `CONTROLLER`

Several messages carry one byte saying who is flying an aircraft.

| value | name | means |
| --- | --- | --- |
| `00` | `NOBODY` | the slot is open |
| `01` | `PERSON` | a person's input |
| `02` | `AI` | an AI pilot |
| `03` | `LEARNT_LANDING` | the AI pilot, flying the landing learnt by reinforcement learning (since `08`); in a `CONTROLLER_SWAP` only |

A value this version does not know makes the message it is in unreadable, and
so does `LEARNT_LANDING` anywhere but a `CONTROLLER_SWAP`: everywhere else an
aircraft the learnt landing flies is flown by `AI`.

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
| | then: |
| `f64` | when it took over from the weather before it, on the simulation's clock, not below nought |
| `f64` | how long it blends in over from then, seconds, 0 to 86,400; nought, none |
| `u8` | `01` if a `WEATHER_ALOFT` follows it, `00` if none does |

At its limits this is 1,079 bytes.

**Both ends blend from the same moment.** A new weather is blended in, not
stepped to: every value moving linearly, over the blend, from what the old
report gives to what this one gives, and the turbulence changing halfway. The
receiver blends from when the sender says and over as long, so the air is the
same while it changes as well as before and after. A first weather - or the
first a client hears, joining while one blends in - has nothing to blend from
and is flown whole.

**The air is on the simulation's clock**, not on each aircraft's own: gusts,
turbulence, a microburst's start and the blend are all of the session's time,
so two aircraft side by side meet the same gust whichever was made first.

**A weather whose forecast follows is not flown until the forecast has
come**, so that a report is never flown for a moment without the forecast
above it. The reliable layer delivers them in order: the forecast is the next
of the two. A `WEATHER_ALOFT` with no such `WEATHER` before it is dropped.

**An empty METAR is still air**: the standard atmosphere with no wind, which
is what a server given no weather flies. Every other field of it must then be
nought, or `00`, and its microbursts none; a reader refuses still air carrying
a place, a turbulence severity, a seed, a microburst or a forecast to follow.
It is sent all the same, so that a client is told the server flies no weather
rather than left to fly one of its own.

**A reader refuses a station past the pole** - a latitude beyond 90 degrees
either way or a longitude beyond 180 - a change before nought or a blend
below nought or over a day (86,400 s). A METAR the receiver cannot read is
refused too: the weather flown before it is kept, and the client says so.

**A METAR longer than 256 bytes is cut by the server, whole words from its
end**, and the server flies the report as cut: a report cut on the wire alone
would be one its clients never heard.

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

**What is hashed.** The collision ground is the Copernicus DEM as the build
knows it - `dem/coverage.txt`, which says which tiles exist and is made from
the two buckets' pinned tile lists - with the runway strips the ground under
runways is made from (`runways/strips.csv`) and the rules it is made by. The
SHA-256 is over three lines, each ending in a line feed:

```
coverage.txt <the SHA-256 of coverage.txt, 64 lowercase hex digits>
strips.csv <the SHA-256 of strips.csv, likewise>
ground rules <the rules' number, 1>
```

The name and version are for a person - this server sends
`Copernicus DEM GLO-30 and GLO-90, with runway strips` and
`coverage 24e568c5, strips 6c1ba3c3, ground rules 1`, the first eight digits of
each file's hash - and only the hash is compared.

**A client on other ground refuses it**, says so, and leaves; it does not
fetch the server's. The ground is the data the build carries and the tiles
it names, fetched from the open buckets by every machine alike - there is
nothing the server could send but a hash - so a client whose hash differs is
another build or has other data, and its remedy is the matching release.

### What a client is told on joining

**A server tells each client what the session is** before anything else it
must arrive, in this order, once the session is proven (it has opened
something sealed under it) and on the update after: `TERRAIN_DATASET`, then
`SESSION` - its clock as it was when sent - then `LOBBY`, then `WEATHER`, and
`WEATHER_ALOFT` after it where the weather has a forecast; then each
`AIRCRAFT`. After that:

- **the lobby again, whole, whenever it is not what was last sent** - a
  player joining or going;
- **the weather again whenever it changes**: a station's fetched again
  (every fifteen minutes by default), or a test's change, each saying when it
  took over and over how long it blends in;
- the ground and the session never again: neither changes in a session.

A client joined again after being let go is in a new session, and is told all
of it again. **A client flies the weather it is told**, not one of its own,
and a client on other ground than the server's refuses it and leaves.

A server running no aircraft at all sends no state updates and none of these
either.

### `CONTROLLER_SWAP`

An aircraft handed between a person and an AI pilot, by the server's number
for it. **It travels both ways.** A client asks for its own aircraft to be
handed to the AI pilot (`02`) or back to it (`01`), with the time written as
nought; the server honours it only for that client's own aircraft, and says
so to every client, with the time it took effect. A request for another's
aircraft, or to `NOBODY`, is acknowledged and nothing more - with one
exception, taking over.

**Taking over.** A client asks for an aircraft the AI is flying, not its own,
to go to `PERSON` (`01`). Unless the server was started with `--no-take-over`,
it is that client's aircraft from then on: the next state update to it names
it as the client's own, carrying its motion, and the client predicts it from
there. The aircraft the client had goes to the AI and is given a new number,
as an aircraft appearing, with its `AIRCRAFT` message. Its old number is free
again, and may be given to a player who joins later. Numbers are used again,
lowest first: an aircraft's number is unique while it flies, and no longer. The server announces both to every client, the one taken over to
`PERSON` and the one left, under its new number, to `AI`. A request for a
player's aircraft, one the AI is not flying, a wreck, or on a server that
forbids it is acknowledged, and answered to that client alone with a
`TAKE_OVER_REFUSED` (since `07`).

While the AI flies an aircraft, the server applies none of its client's
inputs, and a state update gives its controller as `AI`. A client whose
aircraft has been handed over stops predicting it and draws it from the
updates like any other; given it back, it predicts again from the next
update carrying its motion, and until the server has applied an input sent
since, it knows nothing of how it is being flown. The server brings the
controls from the AI's to the pilot's at the pace of a hand - full travel in a
second - rather than jumping them, so for that second the pilot's inputs are
not yet all it flies.

**The learnt landing** (since `08`). A client asks for its own aircraft to
go to `LEARNT_LANDING` (`03`), with the time written as nought. The server
honours it only for that client's own aircraft, only where the server has a
learnt landing for its model, and only where the aircraft is at that
landing's gate on the final approach to a runway: 1.6 to 2.4 nautical miles
before the threshold, within 60 m of the extended centreline and 20 m of a
3-degree glidepath aimed 300 m past the threshold, within 5 degrees of the
runway's heading, from 3 kt under the landing's reference speed to 8 kt over
it, with the landing flap out and not moving, and in a wind of no more than
15 kt across, 8 kt ahead and 5 kt behind (the numbers are the learnt
landing's file's and the simulation's, not the wire's). Honoured, it says so to every client
with `LEARNT_LANDING` and the time it took effect; from then a state update
gives the aircraft's controller as `AI`. Otherwise it is acknowledged, and
answered to that client alone with a `LEARNT_LANDING_REFUSED` saying why
(since `09`; before it the client was told nothing). Taking it back is `PERSON` (`01`), as from any AI.

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

### `COPILOT_ROUTE`

**A route for the client's own aircraft, from its copilot**, sent by a
client (the project owner, 2026-09-30; `REQUIREMENTS.md` section 5). The
player's client asks a language model with the player's own key and sends
only what came of it: waypoints and orbits, and a glide airspeed for an
engine that has stopped. **The key is never sent.**

**It is an input, not an order.** The server honours it only for that
client's own aircraft, and a wreck's not at all. It reads the route as a
flight plan from where the aircraft is, and checks it against the aircraft
as the server has it - every waypoint within 200 km, every height 500 ft
above both the sea and the ground beneath the aircraft - the collision
ground, as the aircraft meets it - and every airspeed
within the speeds the aircraft's figures file says it holds clean round a
tight turn - never below its approach speed (none of these for a glide,
which flies neither), every orbit wide enough for its airspeed,
a glide only with the engine stopped and from the approach speed to the best
climb (with no approach speed, between its climb-away speed and its slowest
plan speed), and with the engine stopped nothing but a glide. **A route that fails
is refused, and nothing changes**: the aircraft goes on as it was. The server
says nothing back; a client learns what its aircraft does from the state
updates, like any other. A route that passes is flown by the server's AI
pilot, from the step it is read: if the player was flying the aircraft it is
handed to the AI first, announced to every client with a `CONTROLLER_SWAP` as
any hand-over is, and the client stops predicting it.

| written as | field |
| --- | --- |
| `u8` | `09`, the kind |
| `u8` | the aircraft's number, as a state update gives it |
| `u8` | `01` if the route is a glide, `00` if not |
| `f64` | the glide's airspeed, knots calibrated, and nought when there is no glide |
| `u8` | how many waypoints follow, 1 to 12: at most 12 |
| | then, per waypoint, in the order they are flown: |
| text | its name, at most 32 bytes: letters, digits and underscores |
| `f64` | its latitude, degrees |
| `f64` | its longitude, degrees |
| `f64` | its altitude, feet above mean sea level |
| `f64` | its airspeed, knots calibrated |
| `u8` | `01` if it is an orbit, `00` if it is flown to and passed |
| | and only for an orbit: |
| `f64` | its radius, metres |
| `u8` | how many times round, `00` for round and round |
| `u8` | `01` turning right, `00` turning left |

At its limits this is 936 bytes: 12 before the waypoints, and 78 for each
of twelve orbits with 32-byte names - a text's two-byte length, 32 bytes, four
`f64`s, the flag, the radius, the turns and the direction. A count of none or
more than 12, a name longer than 32 bytes, an empty name or one with any byte
but `A`-`Z`, `a`-`z`, `0`-`9` and `_` - a newline would add a plan's line, a
`#` comment one out, and an escape reach the operator's terminal - a flag
other than `00` or `01` in any of its three places, and a glide airspeed
other than nought with the flag `00` are refused.

### `TAKE_OVER_REFUSED`

**A take-over the server will not make**, sent by the server to the client
that asked for it and to no other (since `07`): the aircraft's number as the
request gave it. **The server answers every take-over it reads** - a
`CONTROLLER_SWAP` to `PERSON` for an aircraft not the client's own: made,
which the state updates say, or refused with this, whatever refused it - a
server that forbids it, a player's aircraft, one the AI is not flying, a
wreck, a client with no aircraft to leave, or a request past the client's
rate (`THREATS.md`). The one answer that can still go missing is one the
server could not queue, or one lost with a session let go; so a client
gives up a take-over unanswered after a while of its own choosing (the
client with the window: five seconds of flight). A request for the
client's own aircraft to `PERSON` is not a take-over but a take-back, and is
answered as a take-back is: by a `CONTROLLER_SWAP` if anything changed. A take-over made is said by the state updates, which name
the aircraft taken as the client's own; this says the other answer, so that
a client can tell a take-over refused from one still on its way, and need
not guess which of its aircraft a request sent meanwhile - a hand-over, say -
would reach. It says nothing of why; the server's own log does.

| written as | field |
| --- | --- |
| `u8` | `0A`, the kind |
| `u8` | the aircraft's number, as the take-over asked for it |

### `LEARNT_LANDING_REFUSED`

**The learnt landing the server will not give**, sent by the server to the
client that asked for it for its own aircraft and to no other (since `09`):
the aircraft's number, and why, in the words the server's own log uses -
`not at the learnt landing's gate: YSSY 16R: 2.9 miles out; the gate is 1.6
to 2.4 miles out`, `the c182 has no learnt landing`. The reason is for a
person to read, not for a program to parse: its wording is not part of the
protocol. A server cuts a longer one at 160 bytes; a reader refuses one
longer than that, and one with any byte outside printable ASCII (`20` to
`7E`) - it is printed on the player's terminal, where an escape would reach -
and an empty one is allowed. A request for another
client's aircraft is neither honoured nor answered, and one the server could
not queue an answer for is said in its log.

| written as | field |
| --- | --- |
| `u8` | `0B`, the kind |
| `u8` | the aircraft's number, as the request gave it |
| text | why, 0 to 160 bytes |

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
| `06` | `LEAVING` | nothing: a client's goodbye |

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
invented one tells it nothing. **A client knocks too, when it has heard
nothing**: both of this project's clients send a `PING` of their own once a
second while nothing has opened under the session for a second, with a token
each counts up from 1. A server that has the session answers with a `PONG`, and
one that has let it go refuses it (`BAD_HANDSHAKE`), which is how a client
that sends nothing else learns it has been let go. **A client that answers is also a client the
server does not let go** when `--timeout` comes round, which is why the
knocking is the server's job: the server is the one deciding who has gone.

A client sends `INPUTS`, `RELIABLE`, `PING`, `PONG` and `LEAVING`; the server
sends `STATE`, `RELIABLE`, `PING`, `PONG`, and `LEAVING` to a player it drops
or whose session a newer one took over (see "Leaving"), and ignores a
`STATE` from a client. What the server does with a client's `RELIABLE` is
under "Reliable messages" above.

### Rates

**A server holds each session to two rates**, each with a second's worth at
once and no more (a token bucket):

- **240 sealed datagrams a second**, of any kind but `LEAVING`. One past it
  is opened - so that a datagram forged from the client's address spends
  nothing - and then dropped unread: no input applied, no `PONG`, no
  acknowledgement. A `LEAVING` is never dropped for it.
- **8 requests a second**: reliable messages from the client. One past it is
  acknowledged, as every reliable message is, and ignored - a swap not made,
  a route not read, a watch not changed.

Nothing is sent to say so; a client that sends this fast is not one of this
project's (inputs go 30 times a second) and gets nothing for it. Nothing on
the wire changed with them (2026-10-06).

### Leaving: `LEAVING`

**A client that is leaving says so**, and the server lets its session go at
once - its aircraft as `--on-leave` says, its slot back - rather than after
its `--timeout` of silence.

| written as | field |
| --- | --- |
| `u8` | `06`, the kind |

That is the whole plaintext: one byte. Sealed, it is a 31-byte datagram - the
envelope (6), the sequence number (8), the byte, and the tag (16). **A `06`
with anything after it is not a goodbye**, and is ignored like any kind that
cannot be read; the session stays.

**It is sealed, so only the session's own client can end it.** The server
lets a session go for a goodbye only once it has opened under that address's
session keys. One sealed under another session, or sent from another address,
opens under nothing there and lets nobody go (`docs/THREATS.md`, "Ending
somebody else's session").

**It is not reliable: it is sent three times.** The client is going away and
will not wait a round trip to hear it acknowledged, and nothing acknowledges
it. This project's clients send it three times back to back, each sealed
afresh under its own sequence number - one sealed once and sent three times
would be refused twice by the replay window. The first to arrive lets the
session go; the copies after it arrive at an address with no session, and are
answered with `REFUSAL` `BAD_HANDSHAKE`, which a client that has left does not
read. **If all three are lost**, the server's timeout lets the session go as
it always did. So a client may leave the goodbye out altogether: it is let go
all the same, only later, and its slot is held until then.

**The server says goodbye too, when its operator drops a player, or when a
newer session for the same key takes over.** It sends the same `LEAVING`,
sealed under that session, three times, each sealed afresh, and lets the
session go. A client that opens a `LEAVING` from the server has been dropped
or taken over - it cannot tell which, and needs not: it stops, and does not
join again. **A session that has never had anything open under it is sent
no goodbye**, as it is sent nothing else; it is let go in silence. The server
also remembers the dropped player's static key for the rest of its run and
refuses any initiation from it with `DROPPED` (`07`), whatever address it
comes from - so if all three goodbyes are lost, the client that joins again
is refused, and stops there. A server that restarts forgets whom it dropped.

**A goodbye does not make an initiation new.** A client that comes back from
the same address after its goodbye must handshake again with a new initiation,
as after a timeout ("An initiation is taken once from each address").

`glideslope_cli connect` says goodbye at the end of its stay (not with no
`SECONDS`, which is a test's silent client, nor with `--no-goodbye`), and the
client with the window says it when it quits or its session otherwise goes.

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
| `u8` | whether it is flying or a wreck: `00` flying, `01` wrecked, `02` flying with an engine stopped |
| `u8` | which engine has stopped, the first by its number from `00`, when the condition is `02`; `FF` exactly when it is not (version `04`). Anything else is refused |
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
| `u16` | how many steps of 1/120 s the server had flown it on the newest input it had applied, when it took this motion - from `0000`, the input applied and not yet flown, to `FFFF`, which is that many or more |

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
| `i16` | the speedbrake lever, out at 1 - or `-32768` where it has no speedbrakes (since `06`) |

A flag other than `00` or `01`, or `-32768` in any control but the gear and
the speedbrake lever, makes the packet unreadable.

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
1,138 bytes, and 1,168 with the
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

**An engine stopped** (`02`, 2026-09-30) is an aircraft still flying with at
least one of its engines not running, for whatever reason the flight model
has: a failure the server was told to give it (`--fail-engine-at`, a
test's), and as much its fuel run out, in any flight. A player's copilot,
asked on the player's machine, is told so, and answers with a glide
(`COPILOT_ROUTE`). Since version `04` it says which engine - the first
stopped - so that a client predicting its own aircraft stops the same one;
not whether it will start again: flown again after a wreck, an aircraft's
engines run, and the condition goes back to `00`.

**A client built before the condition `02` refuses the whole state update**
while any aircraft in it has an engine stopped, as it refuses any condition
it does not know (below) - every aircraft's position with it, not only that
one's - as an older client refuses `DROPPED` as an unknown reason. The
condition `02`, like `COPILOT_ROUTE`, came without the protocol's version
moving (it is `02` too, for the ground; "It does not keep the version with
the layout, yet", above): such a client must be brought up to date to play
on a server where an engine can stop.

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
forward again from the step of its own that the motion was about - see "Where
the server's word falls on the client's clock" below - through every step it
has flown since, on the inputs it flew them on. How far that moves the
aircraft is the correction; a small one is hidden by blending it in, and one
larger than about a wingspan is not.

**The first update is already a trip old**, so a client should keep the
inputs it sends before it has heard one, and fly those the server has not
yet applied forward from it. Until the server has applied an input of the
client's, though, the client is joining rather than predicting: the server
flew the aircraft for that trip on no input of the client's, and the client
cannot know how.

**Where the server's word falls on the client's clock.** The server flies
an input from the first step after it is read until the next input is, so an
input that jitter makes late is flown late, and for longer or shorter than the
client flew it. The update says how many steps the server had flown on the
input it names, and its clock says how many steps it had flown in all (the
clock times 120): so the input began at the server's step `clock x 120 -
steps into it`. Less the step at which the client began flying that input, that
is how far the server's clock is behind the client's, plus however late the
network made that input. The least of it over the last two seconds of updates
is the clocks' difference - the input that waited least - and the motion is
about the client's step `clock x 120 - difference`. The client puts its
aircraft back to the motion there and flies every step after it again.

A client that replayed from the end of the applied input instead - as though
the server had flown every input exactly as long as it had - was wrong by up
to the aeroplane's speed times the jitter and an input's length: at 50 m/s, 60
ms of jitter and inputs thirty a second, 5 m. One that took each input's own
lateness as the difference, rather than the least, was wrong by the jitter
alone. For its first second the difference is still coming down as inputs that
waited less arrive, and each time it does the client is moved by the steps it
changed by.

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
byte. The hash is BLAKE2b, libsodium's own, where the brief named BLAKE2s,
which libsodium does not have; the project owner decided it on 2026-10-02
(`REQUIREMENTS.md` 6.7 and section 9). The initiator must
already know the responder's static public key, which the server prints at
startup.

## What is not here yet

- **A session's name chosen by its operator.** `SESSION` names a server by
  its port; nothing sets a name of anyone's choosing.
- **Any check on what a client's inputs say.** They reach its aircraft with
  no range check: a value outside -1 to 1 cannot be written, because the wire
  is a 16-bit fraction. How often it sends is held (below, "Rates").
- **Choosing where to start.** A player may ask for an aeroplane (the
  initiation's payload, above), and starts where the server's plan starts.
- **Rate limiting before a session, and the cookie an overloaded server
  would demand.** A server does an X25519 operation for any stranger that
  sends it an initiation. `docs/THREATS.md` says what that costs and what would bound it.

What a client written from this document **can** do today: complete the
handshake with a server whose public key it was given, be admitted to a slot,
seal and open datagrams under the keys that handshake agreed, answer the
server's knocking so that it stays in its slot and the server can measure the
round trip, **read where every aircraft is 25 times a second, learn what
aeroplane each one is, and fly its own aircraft by sending inputs**, and say
goodbye when it leaves, or be let go when it stops, **and be told the
session, the lobby, the ground it collides on and the weather it flies**.

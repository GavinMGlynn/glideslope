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
| `f64` | 8 | its IEEE-754 bits, written as a `u64` |
| text | 2 + n | a `u16` length, then that many bytes, not terminated |
| bytes | n | a length agreed by the message, then that many bytes |

**No floating-point number is written as a floating-point number.** A double
goes on the wire as its IEEE-754 bits in a `u64`, which has one
representation rather than a compiler's choice of one. So `1.0` is
`00 00 00 00 00 00 F0 3F`.

Text is bytes, not characters: the length is in bytes, and it is not
terminated. A reader is entitled to a limit and to refuse anything longer.

### What a reader must do

**Everything that comes off the wire is hostile until it has been read.** A
reader must never read past the end of the datagram it was given, whatever
the lengths inside it say. This project's reader marks itself broken on the
first read it cannot satisfy, answers zero from then on, and is asked once at
the end whether any of it was real; a client may do it another way, but it
must not trust a length it has not checked.

## What is not here yet

- **The handshake.** `Noise_IK_25519_ChaChaPoly_BLAKE2s`, with libsodium's
  primitives, as `REQUIREMENTS.md` 6.7 specifies. The client is to know the
  server's static key out of band, from `--server-key`, which the server
  prints at startup. Neither end is built, and libsodium is not yet a
  dependency.
- **Sealing.** `SEALED` bodies are to be ciphertext under the handshake's
  keys, with a sequence number and a replay window per message. Nothing seals
  anything today.
- **The messages.** Which messages exist, what each carries, and which of them
  need the reliable layer. None is defined yet.
- **Anyone to talk to.** The sockets exist - UDP, non-blocking, on BSD
  sockets and on Winsock - but nothing listens on one, because there is no
  server.

Until those exist there is nothing to connect to, and a client written from
this document can encode and decode an envelope and its values, and send them
into the dark.

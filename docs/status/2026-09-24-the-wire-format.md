### The wire format, 2026-09-21 — item done 2026-09-24, above

Phase 6's first item, started. **The envelope and the encoding are built,
tested and written up byte for byte; nothing yet connects to anything.**

**Six bytes at the front of every datagram**: four of magic, one of version,
one of type. The magic is `GLDS`, this project's own, so a gearstick client
and a glideslope server refuse each other at the first four bytes rather than
somewhere deeper. The magic is checked before the version, so another
protocol's datagram is told it is another protocol rather than told its
version is wrong - true, but useless.

**Little-endian, fixed-width, no padding**, and a double goes on the wire as
its IEEE-754 bits in a `u64`: one representation rather than a compiler's
choice of one. The test pins the bytes themselves, not only the round trip -
`1.0` is `00 00 00 00 00 00 F0 3F` - because a third party writing a client
needs the bytes.

**Everything off the wire is hostile until it has been read.** The reader
never reads past the end of what it was given, whatever the lengths inside
say: it marks itself broken on the first read it cannot satisfy, answers zero
from then on, and is asked once at the end whether any of it was real. That
is what lets it be fuzzed without a crash being the expected outcome.

**Walked, not sampled.** Every truncation of a whole datagram - every length
from nothing to one byte short - and every single-byte change at every
position, all 256 values at each: the whole space, with its size stated and
checked. The round-trip test states that it walks 40 values, and caught its
own arithmetic when it was written as 39.

**The document and the code are held to each other.** A document written so a
third party could build a client from it alone is worth nothing if it drifts
from what this end sends, so a test reads every number back out of
`docs/TRANSPORT.md` - the magic in hex and in ASCII, the version, the
envelope's size, every type and every refusal by value and by name - and
holds them against the code. It also holds the document to having a section
saying what the transport does not claim, and one saying what is not built.
It found the document at once: it said "six bytes" in words where a
byte-for-byte specification should give the number.

**What is not built**: the handshake, the sealing, the messages, and sockets.
`src/platform/` has no socket code on any platform and libsodium is not a
dependency. Until those exist a client written from the document can encode
and decode an envelope and its values, and no more - which the document says
plainly rather than leaving to be discovered.

**Verification run.** Seven tests, all registered:
`every_value_written_to_the_wire_reads_back_as_itself`,
`the_wire_puts_the_least_significant_byte_first`,
`the_envelope_is_the_magic_then_the_version_then_the_type`,
`an_envelope_that_is_wrong_is_refused_with_the_reason_it_is_wrong`,
`every_truncation_of_a_datagram_is_refused_without_running_off_the_end`,
`no_single_byte_changed_anywhere_in_a_datagram_can_break_the_reader` and
`the_transport_document_and_the_code_agree_byte_for_byte`. They take under a
fifth of a second between them. 307 of 307 tests pass locally at `-j4`, in
945 s.

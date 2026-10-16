### The handshake is Noise's own, byte for byte, 2026-09-24 — tail done

**`Noise_IK_25519_ChaChaPoly_BLAKE2b` is now the suite the Noise Protocol
Framework defines, not one that shares its name.** Three things differed, and
any one was enough to stop a standard Noise client completing it:

- BLAKE2b was cut to 32 bytes. Noise's BLAKE2b is 64 - the hash and the
  chaining key - and its cipher keys are the first 32 bytes of what the HKDF
  gives. The 33-byte protocol name was cut to fit; it is now padded to 64.
- **No prologue was mixed in.** Noise hashes the prologue after initialising,
  empty or not; the protocol's is empty.
- In writing the fix, each X25519 output was held in the new 64-byte hash
  type and went into the key padded with 32 zeros. The known-answer test is
  what caught it.

**Verification.** `the_handshake_is_noise_ik_byte_for_byte_as_the_cacophony_vector_has_it`
fixes every key, the prologue and the payloads as the Noise community's
`cacophony` vector for this suite does (haskell-cryptography/cacophony,
vectors/cacophony.txt, public domain) and requires both ends to write its
two handshake messages byte for byte, and the keys they split into to seal
its four transport messages to its bytes. The same message was reproduced
first in a few lines of Python from the specification, to find where the C++
parted from it. `Initiator` and `Responder` take a prologue and, for this
test only, a fixed ephemeral key. The 149 network, session and client tests
pass. The handshake on the wire has changed: a server and client from before
this cannot talk to one from after, which nothing deployed depends on.

**`TRANSPORT.md` corrected on the way**, towards the transport item's own
verification (a client written from the document alone). It said the
handshake and sealing were not built, and that nothing sent inputs or state;
it gave `your_aircraft` as a place in the list where the server writes the
aircraft's number - a test held the document to that wrong wording, and holds
it to the right one now; and it left a reader to find out by experiment the
handshake's framing and sizes, how a lost one is resent, that a session is its
address, which split key is which, what the tag covers, the replay window's
edge, what each control means and must be left at, which refusals the server
sends and when, that a server flying nothing sends no state, and that a
session ends only by silence. An independent client written from the old
document found each of those; the document now says them.

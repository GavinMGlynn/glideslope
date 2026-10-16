### The handshake, and a decision taken without the owner, 2026-09-22

**What is missing first: nothing is sealed yet.** The handshake agrees keys
and neither end uses them. `SEALED` bodies are still plaintext and there is no
replay window, so the transport is not yet private for a single byte.

**The suite is not the one `REQUIREMENTS.md` 6.7 names, and the reason is
libsodium.** It asks for `Noise_IK_25519_ChaChaPoly_BLAKE2s`. libsodium - the
library that section chooses - ships `crypto_generichash_blake2b` and **no
BLAKE2s at all**, which was checked against its installed headers rather than
assumed. BLAKE2b is a hash the Noise specification itself defines, so
`Noise_IK_25519_ChaChaPoly_BLAKE2b` is a real suite and not an invention. The
alternatives were SHA-256, also native, or carrying a BLAKE2s of our own -
hand-written cryptography in a project that has none. **The owner was asked
and has not ruled**; the goal was reaffirmed instead, so the choice was taken
rather than blocking on it. It is one line: `handshake_name` and the functions
it names.

**What the tests prove, and what they do not.** They prove two honest ends
agree crosswise - each sends under the key the other receives under - that
each learns who the other is, that eight runs with the same keys give eight
different sessions, that a client told the wrong server key gets nowhere, and
that **25,755 one-byte changes and 101 truncations of an initiation, and 190
changes to an answer, are refused**. They do **not** prove it matches the Noise
specification: there are no published vectors for this suite in the project,
and conformance is not among the things shown. `THREATS.md` says so.

**A replayed initiation is answered.** The responder has no memory and cannot
tell, so it does the work; the session it makes is one the replayer cannot
read, lacking the initiator's ephemeral secret. That is a denial of service,
not a compromise, and it is named rather than defended.

**A hole in my own test, not in the handshake.** The answer-tampering test
made a fresh handshake for each change but decided what to skip from the
*first* answer's bytes. The ephemeral key differs every run, so sometimes it
"changed" a byte to the value it already had and then reported that an
unmodified answer had been accepted. One acceptance in 191. The skip is now
made against the answer actually being changed.

**The server's static key is real too**: minted, printed and read back, held
against **RFC 7748's own X25519 vector** - a round trip of our own could never
say the key agreement had stopped being X25519.

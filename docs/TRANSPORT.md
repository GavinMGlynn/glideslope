# Transport

**Not written yet, because there is no transport yet.** It arrives in Phase 6.

When it exists, this file describes the wire protocol byte for byte — the
handshake, the envelope, every message, and what the transport does **not**
claim — so that a third party could write a working client from this document
alone. The design it will describe is in `REQUIREMENTS.md` section 6.7:
gearstick's `Noise_IK_25519_ChaChaPoly_BLAKE2s` transport over UDP, with a magic
value of this project's own so that a gearstick client and a glideslope server
refuse each other cleanly.

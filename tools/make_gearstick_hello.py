#!/usr/bin/env python3
"""The first datagram a gearstick client sends, for a glideslope server to refuse.

`COMPLETION_PLAN.md`'s transport item asks that "a gearstick client is refused".
gearstick is this project's sibling (github.com/GavinMGlynn/gearstick), whose
transport glideslope's was taken from, with its own magic so that the two refuse
each other at the first four bytes. This writes exactly what a gearstick client
puts on the wire first, made from gearstick's own docs/TRANSPORT.md at commit
a78a8132e402fe2c4130c3844733684e7ae7f066, sections 3 and 4:

- the envelope: magic `47 53 53 56` ("GSSV"), version 1, type 1 (`HANDSHAKE`);
- message one of `Noise_IK_25519_ChaChaPoly_BLAKE2s` with the prologue
  `gearstick/1`, an empty payload, 96 bytes: `e, es, s, ss`. The 33-byte name
  is longer than BLAKE2s's 32, so `h = BLAKE2s(name)`, as its section 4.1 says.

The keys are fixed, derived from labels, so the file is the same every time it
is made; a gearstick client makes fresh ones. The server's key is anybody's -
a glideslope server refuses at the magic, before it looks further.

Usage:  tools/make_gearstick_hello.py > tests/data/gearstick_hello.hex

Needs Python 3 and the `cryptography` package, for X25519 and
ChaCha20-Poly1305; hashlib has BLAKE2s. Only this script needs them: the test
reads the committed hex.
"""

import hashlib
import hmac

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.x25519 import (X25519PrivateKey,
                                                              X25519PublicKey)
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305

NAME = b"Noise_IK_25519_ChaChaPoly_BLAKE2s"
PROLOGUE = b"gearstick/1"


def blake2s(data):
    return hashlib.blake2s(data).digest()


def hkdf2(ck, ikm):
    temp = hmac.new(ck, ikm, hashlib.blake2s).digest()
    one = hmac.new(temp, b"\x01", hashlib.blake2s).digest()
    two = hmac.new(temp, one + b"\x02", hashlib.blake2s).digest()
    return one, two


def secret(label):
    return X25519PrivateKey.from_private_bytes(hashlib.sha256(label).digest())


def public(key):
    return key.public_key().public_bytes(serialization.Encoding.Raw,
                                         serialization.PublicFormat.Raw)


def main():
    client_static = secret(b"gearstick hello: client static")
    client_ephemeral = secret(b"gearstick hello: client ephemeral")
    server_static = secret(b"gearstick hello: server static")
    rs = public(server_static)

    h = blake2s(NAME)  # longer than HASHLEN, so hashed, not padded
    ck = h
    h = blake2s(h + PROLOGUE)
    h = blake2s(h + rs)  # <- s

    e = public(client_ephemeral)
    h = blake2s(h + e)  # e
    ck, k = hkdf2(ck, client_ephemeral.exchange(X25519PublicKey.from_public_bytes(rs)))  # es
    sealed_s = ChaCha20Poly1305(k).encrypt(bytes(12), public(client_static), h)  # s
    h = blake2s(h + sealed_s)
    ck, k = hkdf2(ck, client_static.exchange(X25519PublicKey.from_public_bytes(rs)))  # ss
    sealed_payload = ChaCha20Poly1305(k).encrypt(bytes(12), b"", h)

    datagram = b"GSSV" + bytes([1, 1]) + e + sealed_s + sealed_payload
    assert len(datagram) == 6 + 96, len(datagram)
    print(datagram.hex())


if __name__ == "__main__":
    main()

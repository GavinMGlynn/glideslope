### A client written from TRANSPORT.md alone, and a gearstick client refused, 2026-09-24 — item done

**`tests/doc_client/doc_client.cpp` was written by somebody who read
`docs/TRANSPORT.md` and the Noise specification and nothing else of this
project**, using libsodium and the system's sockets - an agent told to open no
other file, and to write from the document rather than by probing the server.
It completed a session on its first run: the handshake, 193 state updates, its
own aircraft found, 9 of the server's knocks answered, 240 of 241 input frames
applied and the aeroplane rolled past 90 degrees on full left aileron, and a
second one refused with `SERVER_FULL`.

**It took two rounds.** The first client, written from the document as it
was, worked too - but only after finding by experiment a dozen things the
document did not say, and one it said wrongly (`your_aircraft` is a number,
not a place in the list). Those were written in (see "The handshake is
Noise's own") and a second client written from the corrected document, with
the first put out of its reach, needed no experiment. Its last few gaps -
how often to resend a handshake, the angles' signs, the rounding of a
control, that a client refuses nothing - are written in too.

**A gearstick client is refused.** gearstick's first datagram, made from its
own `TRANSPORT.md` (at a78a813) by `tools/make_gearstick_hello.py` - magic
`GSSV`, its `HANDSHAKE` type and a real `Noise_IK_25519_ChaChaPoly_BLAKE2s`
message one with its `gearstick/1` prologue - is sent to a running server by
`glideslope_datagram_check`, and the answer must be `47 4c 44 53 01 04 01`,
`NOT_THIS_PROTOCOL`.

**Verification.** `a_client_written_from_the_transport_document_alone_completes_a_session`
builds the document's client linking libsodium alone, holds its includes to
the system's, and runs it against a server flying one AI aircraft; seen to fail
with the server's state updates naming no aircraft as the client's own.
`a_gearstick_client_is_refused_as_not_this_protocol`, seen to fail with the
refusal's reason made `WRONG_VERSION`. The client's Winsock branch has only
been compiled by CI.

**Found on the way: two players can be given the same aircraft number** - a
tail in `COMPLETION_PLAN.md`. The first client, run four at a time, got two of
them into one slot's number: a slot is a key's rank, and the server numbers a
player's aircraft by the slot it had on arrival.

### Two things the wire believed that it should not, 2026-09-22 — two tails done

**What is missing first: nothing has ever arrived over a network.** There is
no handshake and no sealing, so every byte these two defences have turned
away was put there by a test. They are the parsers' own defences, not a
session's, and neither of them tells a forged datagram from a real one; that
is the sealing's job and the sealing is not built. Both were found writing
`THREATS.md` and both are in `COMPLETION_PLAN.md`'s tails.

**An acknowledgement of a message that was never sent.** `Reliable::received`
took the acknowledgement number out of any datagram handed to it and let go
of every message up to it, so one datagram carrying `0xFFFFFFFF` emptied the
send queue and nothing sent those messages again. The layer now keeps the
highest number it has actually put on the wire and ignores anything above it:
an endpoint cannot have received what was never sent. The rest of the
datagram is read as before - this is one field not believed, not a reason to
throw a message away - and every legitimate acknowledgement behaves exactly
as it did.

**The space, stated, because a `u32` has too many values to feed one at a
time.** 240 acknowledgements are fed: **175 at the boundary**, which is every
number from 0 to two past the end for every sender that has put 0 to 4
messages on the wire with 0 to 4 more queued behind them; **64 far ones** -
every power of two, every power of two less one, and `0xFFFFFFFF`; and one in
the middle of a real exchange, after which all four messages still arrive
once each and in order. Everything not walked is strictly above what was
sent, which is the case `0xFFFFFFFF` walks, and the layer does no arithmetic
on the number for a larger one to do differently.

**Watched failing**: before the fix, *"a sender that had put 0 on the wire
with 1 behind them, told it was acknowledged up to 1, should hold 1 and holds
0"*.

**A number that is not one.** Every `f64` was read as whatever bits arrived,
so a NaN or an infinity in a latitude, a microburst's radius or the
simulation's clock was accepted and handed up - a NaN position spreads
through the floating origin and the terrain query, and an infinite duration
never ends. **The refusal went where it cannot be forgotten**: one reader,
wrapping the wire's `Reader`, through which every floating-point field of
every message passes. A field added later is checked without anyone
remembering to, and it wraps rather than derives so that reaching past it for
something it does not forward is a compile error rather than a way round.
`src/net/protocol.hpp`'s `Reader` is untouched, because the envelope and the
input stream have their own rules about what they carry.

**The space, stated**: the seven kinds carry **nineteen** floating-point
fields between them - one in `SESSION`, three in `WEATHER` and six per
microburst, five per pressure level and three per near-ground wind in
`WEATHER_ALOFT`, one in `CONTROLLER_SWAP`. `LOBBY`, `AIRCRAFT` and
`TERRAIN_DATASET` carry none at all, and are named in the test as left out
for that reason. Each of the nineteen is overwritten with **six** patterns
that are not a number - both infinities and NaNs quiet and signalling, signed
and with a payload - and with **five** that are numbers however extreme, the
largest finite double, the most negative, the smallest subnormal and both
zeros, which must still read. **114 refused, 95 still read.** The test finds
each field by writing a distinct dense-mantissa value into it and insisting
its eight bytes appear in the encoded message once and once only, so the
layout is not written out a second time to be got wrong.

**Watched failing**: before the fix, *"session with +infinity at byte 34 must
be refused"*.

**`docs/TRANSPORT.md` says both**, where it says how values are written, what
a reader must refuse, and what the reliable layer does with an
acknowledgement; the test that holds that document to the code passes.

### What the server will send back, 2026-09-22

**What is missing first: nothing sends one.** The `STATE` packet is defined,
encoded and tested, and no server writes one and no client reads one. The
server flies its aircraft and tells nobody where they are.

**But it is the piece the whole of Phase 7 waits on**, because a controller
swap is only visible if there is a state stream to see it in. A packet carries
the simulation's clock, the newest input sequence from this client the server
has applied - which is what a client reconciles against - and up to twenty
aircraft, each with its number, who is flying it, a position, a velocity and
an attitude.

**Positions are Earth-centred, Earth-fixed and double precision, and that is
not a style choice.** The whole world is in play, so there is no session
origin for an aircraft to be near: two in one session may be on opposite sides
of the planet, which the server already does. A float has 24 bits of mantissa,
which at Earth's radius is half-metre steps. Velocities and angles are floats,
because a float holds them far better than anything can measure them, and
twenty aircraft twenty times a second is worth the bytes.

**Twenty aircraft is the most, and it fits.** Four players and sixteen AI is
what `--players` and `--ai` allow. A full packet is **1,014 bytes, and 1,044
with the envelope and the sealing in front of it, inside the 1,232 a datagram
holds** - measured by the test, not calculated in the document. The weather
message taught this lesson the expensive way: it was 5,419 bytes in an item
that had already been ticked.

**`f32` had to be added to the wire first**, with its own row in the format
table and its own eleven values in
`every_value_written_to_the_wire_reads_back_as_itself`, which now walks 51
values rather than 40 and counts them.

**Walked, not sampled.** Every count of aircraft from nought to twenty; 1,456
truncations; all 1,312 one-bit changes to a three-aircraft packet - and the
rule there is not "does not crash" but **what the reader takes must write back
to the very bytes it was given**, so nothing is lost and nothing invented: 37
refused, 1,275 read exactly. All 256 bytes in the controller's place, three of
which are controllers. A NaN and both infinities in each of the ten numbers a
packet holds, refused on the way out and on the way in, thirty cases counted.

**Watched failing.** With the trailing-byte check and the reader's NaN check
taken out, three of the seven went red - including the one-bit test, which
found the NaN hole on its own, because flipping bits into an exponent makes
one.

**And two parsers were missing from the fuzzing corpus.** `read_state` is new,
but `knock_token` was not: it went in when the ping did and nothing put it in
the list. Thirteen parsers and nineteen seeds now, each counted in the test so
that a parser added without a seed fails.

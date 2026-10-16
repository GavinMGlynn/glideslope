### The reliable messages, 2026-09-22 — item done

**What is missing first: nothing carries them yet.** There is no server and
no handshake, so these messages are written, read and delivered between two
endpoints in a test and nowhere else. What is proved is that the six exist,
that they survive loss, and that a third party could write them from
`TRANSPORT.md`.

**The six the item names now exist**: the lobby, the session, the weather, an
aircraft's definition, the terrain dataset and a controller swap. Each body
begins with one byte saying which kind it is. `src/net/messages.hpp` defines
them and `docs/TRANSPORT.md` writes every field of every one out byte for
byte.

**There are seven kinds, because the weather is two of them.** The first cut
made the weather one message carrying the METAR and the forecast above it,
with limits of a 1024-byte report, 64 pressure levels, 8 near-ground winds
and 32 microbursts. At those limits its body was **5,419 bytes against the
1,218 a datagram leaves** once the envelope and the reliable header are in
front of it - 4,201 over - and even at the figures this project really
fetches, nineteen levels and four winds, a report with a single microburst
came to 1,235. Nothing fragments, so such a message could not be sent at all:
the socket refuses it and the reliable layer would repeat it for ever. The
report now goes as `WEATHER` and the forecast as `WEATHER_ALOFT`, the limits
came down to 256 bytes, 24 levels, 4 winds and 16 microbursts, and
`every_message_filled_to_its_limits_fits_in_one_datagram` fills every message
to its limits and holds it to the datagram. The largest is now the forecast
at **1,093 of 1,218**, and the forecast actually fetched is 877.

**`src/net/` still includes nothing but the standard library.** The wire
structures are the wire's, not the simulation's - turning a
`world::WeatherReport` into a `net::Weather` is the server's job. That is what
lets the whole transport be tested without a socket, a terrain tile or a
flight model, which is how it can be put through 4,096 loss patterns.

**The weather is sent as what it was made from.** A METAR is a line of text
with twenty optional fields; re-encoding them is twenty chances for two ends
to disagree. The raw report goes on the wire and the receiver parses it with
the same code the sender did. The forecast above it has no such text, so its
levels go as numbers.

**Verified:**

- `every_message_writes_and_reads_back_what_went_into_it` - seven kinds, every
  field set to something that is not its default, so a field the writer forgot
  cannot pass.
- `no_message_reads_as_a_kind_it_is_not` - all **49 pairs**; the 42 that are
  not a kind reading as itself are refused.
- `every_truncation_of_every_message_is_refused` - **465 prefixes**.
- `every_message_with_anything_trailing_is_refused` - nothing can be hidden
  behind a message.
- `every_single_byte_change_to_every_message_is_read_or_refused` - the whole
  space, **118,575 changes**: 108,141 still read (most bytes are a double's,
  where another value is another valid double) and 10,434 are refused. None
  crashes and none reads as a kind it is not.
- `the_message_kinds_and_controllers_are_the_ones_the_document_names` - seven
  kinds and three controllers, and 249 of the 256 possible first bytes are no
  kind at all.
- `every_reliable_message_arrives_exactly_once_and_in_order_under_loss`
  - **the item's own verification**: all seven real messages through the
  reliable layer under all **4,096** patterns of loss over twelve datagrams.
  In each one every message arrived exactly once, in order, and read back as
  the message it was written from. The worst pattern took 21 datagrams.
- `the_transport_document_and_the_code_agree_about_the_messages` - 7 kinds,
  3 controllers and 8 limits read back out of `TRANSPORT.md` and held against
  the code. It was watched failing when the limits changed and the document
  had not, which is what it is for.

**A build trap worth knowing.** `glideslope_net` lists its sources rather than
globbing them, so `cmake --build --target glideslope_net` succeeded without
compiling `messages.cpp` at all, and only the link of the tests found it.

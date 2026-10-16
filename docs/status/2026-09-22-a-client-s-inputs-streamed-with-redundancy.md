### A client's inputs, streamed with redundancy, 2026-09-22 — item done

**What is missing first: nothing sends them.** There is no handshake, so no
client is connected and no input packet has ever crossed a socket. What is
built is the stream itself, where it can be walked against every pattern of
loss rather than against a network that happens to be working.

**Repeated, not retransmitted.** A lost input frame is worth nothing a moment
later - the aircraft has moved on - so waiting for an acknowledgement would
deliver it too late to use and cost a round trip to find out. Every packet
carries the last four frames instead, at 141 bytes.

**The bound is stated, because "no loss ever" is not true.** A frame rides in
the packet of its own number and the three after it, so **it arrives if and
only if at least one of those four arrives**. That is the invariant, and it is
held frame by frame over all **4,096** patterns of loss across twelve packets:
1,490 lose nothing at all, 2,606 lose at least one.

**The first shape of that test was wrong, and it caught itself.** It claimed
nothing is lost unless four packets go in a row - true in the middle of a
stream, false at its end, because losing only the last packet loses the last
frame, which no later packet carries. Pattern 2048 found it. The rule above
has no such edge.

**Quantised, and the client flies what it sent.** Each control goes as a
16-bit fraction of -1 to 1: both ends exact, a step of 3e-5, a quarter of a
double's size. The client predicts on the rounded value rather than on what
its stick said, because a prediction fed different inputs from the server
would drift for a reason no measurement could explain - which is exactly the
drift this project promises to bound.

**`src/net/` still includes nothing but the standard library.** The wire does
not know what a control is: `sim::Controls::as_list` hands seventeen numbers
over and `from_list` takes them back, so the list lives with the flight model.
`every_control_the_flight_model_has_is_one_the_wire_carries` holds
`sizeof(sim::Controls)`, so a control added without being put in `as_list`
fails rather than being quietly left out of every flight.

**Watched failing**: a receiver that keeps only the newest frame, throwing the
redundancy away, is told "frame 1 did not arrive but a packet carrying it did
get through".

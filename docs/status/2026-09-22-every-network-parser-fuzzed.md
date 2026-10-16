### Every network parser fuzzed, 2026-09-22 — item done

**What is missing first: nothing has ever reached these parsers over a
network.** There is no handshake, so every byte they have seen was put there
by a test. What is proved is that none of them can be made to read past the
end of what it was given.

**Eleven parsers, eighteen seeds, and every seed through every parser.** The
parsers are the envelope, the seven reliable messages, the kind byte, the
reliable layer and the input receiver. The seeds are every datagram and
message this project writes plus five it never would - an empty datagram, one
byte, sixty-four bytes of `0xFF`, an HTTP request and 1,232 bytes of `0xA5`.
**Cross-feeding is the point**: a datagram arrives before anybody knows what
it is, and a parser only ever shown its own output has not been tested at all.

**188,617 reads, none out of bounds.** Each seed whole, cut to every length,
changed a byte at a time at six values, and then mutated from a fixed seed -
bytes changed, dropped, inserted, and two seeds joined - so that a failure can
be had again.

**It is under sanitizers by construction.** The build compiles with
`-fsanitize=address,undefined -fno-sanitize-recover=all`, so a read past the
end ends the test rather than printing and carrying on, and ctest runs it in
CI with everything else. The corpus is written to `build/fuzz-corpus` so it
can be handed to libFuzzer or AFL rather than only living inside the test.

**Watched failing**: a `kind_of` that takes `body[0]` without checking there
is a byte is caught on the empty seed, with UBSan reporting a reference bound
to a null pointer and the test exiting non-zero.

**A false alarm I raised against my own rig.** I first read that exit code
through a pipe into `tail`, got `0`, and concluded the sanitizers were not
failing the test - which would have been a hole in the whole project's test
build. They were: `$?` after a pipe is the last command's, not the test's.
The flags were right all along.

### The server binary and its flags, 2026-09-22 — item done 2026-09-24, above

**What is missing: three of its six flags drive nothing yet.** `--store`
names a file nothing is written to, because there is no session to keep.
`--key` is checked to be 64 hexadecimal characters but nothing seals
anything, and this build cannot mint one: a server secret is an X25519 static
key and there is no libsodium here to derive its public half with, so a
secret whose public half cannot be printed would be of no use to a client.
`--timeout` cannot let a silent client go, because no client can connect
until the handshake exists. The dashboard says so on screen rather than
showing an empty table that looks like a fault.

**What works.** `glideslope_server` takes every flag the item names, checks
each, binds its port and draws a dashboard that refreshes each second: the
version, the port, how long it has been up, the datagrams and bytes it has
seen, and a row per slot with who is in it, their ping and their traffic.
`--headless` prints the settings and runs without one. `--dry-run` prints the
settings and exits without binding, which is what lets a test exercise a flag
without a server that has to be killed.

**It links nothing presentational.** The dashboard is written to a terminal,
not drawn, so the server links the simulation, the world and the platform and
no SDL. It does not link the transport yet either, because it does not speak
to anyone yet.

**Verified**, fourteen tests: `--version` and `--help`; the defaults with no
flags; **every one of the six flags together in one run**; both ends of the 1
to 4 player range and both sides of them, which is the item's own
verification; a key of the wrong length and a key that is not hexadecimal; a
port that is not one; a timeout that is not a length of time; a flag with
nothing after it; and an option it does not know.

**Two things CMake does with a semicolon.** `-DARGS=--players;0` does not
pass three arguments - `separate_arguments(... NATIVE_COMMAND)` splits on
whitespace, so the whole thing arrived as one argument named
`--players;0;--dry-run`. And a semicolon inside an expected pattern is a list
separator too, so `--players is 0; a session is 1 to 4` became two patterns.
Nine of the fourteen tests failed on those two until the arguments were
separated by spaces and the server's own messages written without semicolons.

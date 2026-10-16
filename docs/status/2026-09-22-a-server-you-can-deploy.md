### A server you can deploy, 2026-09-22

**What is missing first: the Dockerfile has never been built.** It is written,
and Docker Desktop's WSL integration is off on this machine, so no daemon is
reachable and half the deployment item's verification is unproven. That is
said in the item rather than left to be discovered.

**The systemd half is proven.** The unit that ships is run by `systemd-run`
with only its paths pointed at the build, and a client connects to it: "a
server started by systemd admitted f21c7f56 to slot 0". A ctest does it, and
reports itself skipped where there is no systemd.

**The server runs with no privilege.** `ProtectSystem=strict`, `ProtectHome`,
`PrivateDevices`, `NoNewPrivileges`, `MemoryDenyWriteExecute`,
`RestrictAddressFamilies=AF_INET AF_INET6`, and `SystemCallFilter` cut to
`@system-service` less `@privileged` and `@resources`. It binds a high UDP
port, reads its own data and writes one SQLite file, and `StateDirectory=` is
what gives that file somewhere to live - a server that lost its key on every
restart would lock out every client it had.

**`-DGLIDESLOPE_SERVER_ONLY=ON`.** The server links no renderer, so a
deployment image should not build one. The option leaves out SDL, Cesium
Native, the shader compiler, the renderer, the model loader and the client,
and does not use vcpkg at all: libsodium, SQLite and libcurl come from the
system. It **configures in 6.2 seconds where the full build takes seven
minutes**, and builds the server in about thirty.

**And building Release found two defects the debug preset never sees.**

`src/platform/socket.cpp` writes sixteen bytes into a caller's address through
a raw pointer, and the bound that makes it safe was carried by a ternary
several lines above - a gap allows at most seven groups and no gap means
exactly eight. GCC at -O2 cannot follow that and says "writing 16 bytes into a
region of size 4". It was unreachable, and the invariant now costs one line
and is stated where the writes are.

`src/net/sealing.cpp` built the sequence number with eight `push_back` calls.
Inlined into `Sealer::seal` at -O2, GCC decides the vector's buffer might be
freed at a non-zero offset and refuses the build. Sizing the vector once and
writing into it says the same thing where the optimiser can see it, and does
one fewer capacity check per byte.

**Neither would have shown up in a debug build**, which is the argument for a
server-only Release configuration that takes half a minute: it is cheap enough
to run.

### A line you can send somebody, 2026-09-22

**What is missing first: the client with the window still cannot connect to
anything.** `--online` is in `glideslope_cli` only, so a person who flies
rather than types cannot use it. That is the whole gap in the item now.

**`server.txt` is one line: a host, a port and the server's public key.** That
is everything a client needs and nothing that is a secret - the key is the
half a server prints at startup precisely so it can be handed out, so unlike a
Cesium token the file may be e-mailed, posted, or committed to somebody
else's repository. `platform::default_server()` reads it from the config
directory, or from wherever `GLIDESLOPE_SERVER_TXT` names.

**Both forms `REQUIREMENTS.md` 6.6 names work**: `connect --online` and
`connect --server HOST PORT --server-key HEX`.

**Thirteen ways a line can fail to be one, walked and counted.** A port of
nought is refused because it means "any free port" to a server and is
meaningless to a client; a port past 65535; a key a digit short, a digit long,
or not hexadecimal; a fourth word. Comments and blank lines are skipped so the
file can say where it came from, an upper-case key is read because somebody
will paste one, and a line that is not one is skipped rather than fatal so a
file can keep old servers below the current one.

**The test names the file by the environment.** A test that dropped a
`server.txt` into the person's own `~/.config/glideslope` would be editing
their machine to make itself pass.

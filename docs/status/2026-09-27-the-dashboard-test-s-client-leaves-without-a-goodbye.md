### The dashboard test's client leaves without a goodbye, 2026-09-27 — main made green

Once a client said goodbye when it left (the leaving message), the dashboard
test's client, which stays three seconds against a server running four, was
let go at once; the server's last dashboard pass came after and showed slot 0
empty, and the test failed on Windows debug on CI. The test is about the
dashboard's columns, so its client now leaves in silence (`--no-goodbye`), as
every client did before; so does the duplicate-handshake test's, which reads
the same last pass for exactly one slot taken (found by review). Every other
test's server runs until its clients have gone, which a goodbye only hastens. **Verified**: the test passes on Linux; seen to fail,
it is CI's own Windows run.

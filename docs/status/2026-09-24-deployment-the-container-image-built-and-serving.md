### Deployment: the container image built and serving, 2026-09-24 — item done

**The Dockerfile's image builds, and a server started from it accepts a
client** - in CI, on every pull request, in the job "The server's container
image" (first green in run 35980838463). With `a_server_started_by_systemd_accepts_a_client`,
which already held the systemd unit to the same thing, both of the item's
halves are verified.

It took three changes nobody could have known without building it: the image
is Rocky Linux 10, as the server will be hosted, because Rocky 9's CMake (3.26)
is older than the project needs (3.28); EPEL is enabled for libsodium and CRB
for ninja; and dnf may swap the minimal image's curl-minimal for the full
libcurl (`--allowerasing`), with shadow-utils for `useradd`. The job also holds
the server-only build to refusing `--window` as built without one. The first
builds of the server-only configuration anywhere were these.

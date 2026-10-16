### A client dropped by the operator says so every time, 2026-09-30 — tail done

**What is not covered first.** Nothing in the client or the server changed:
a real client that loses all three copies of its goodbye still learns it was
dropped only from the refusal of its next knock, which needs the server still
running. And the relay's last pass takes everything the server sent only if
a loopback send is in the relay's socket by the time the server's output has
ended - so in practice, not by guarantee: Linux can defer loopback delivery to
ksoftirqd on a loaded machine, and macOS hands lo0's input to a thread of its
own; Windows is not documented either way. Losing that race is not hidden: the
relay says what it took and delivered after its input ended, and the window
client's drop test puts that line first in its failure.

**What was wrong.** `the_client_with_the_window_dropped_by_the_operator_says_so_and_does_not_join_again`
failed on CI four times on 2026-09-30, "the dropped client did not say it was
dropped". The server dropped the client and, with nobody left, stopped
(`--until-empty`); the relay between them (`glideslope_impair
--until-input-ends`) stopped on the first pass that saw the server's output
end, with the server's goodbye still unread or held for its next pass.

**What changed.**
- **The relay passes on what its server sent before it went.** Once its input
  is seen ended it makes one more pass, reading the flag before that pass's
  receives, and stops only when everything held for a client is delivered.
- **It says so.** On standard error: `impair: after its input ended, took N
  from the server and delivered M to the clients; K still held when it
  stopped`. Its time running out with something still held for a client is a
  failure: it says so and exits 1, as it does for giving up with its input
  open.
- **Winsock reports no "port unreachable" on a UDP socket.** Every socket is
  made with `SIO_UDP_CONNRESET` off (`platform/socket_winsock.cpp`). Windows
  alone turned the ICMP answer to a datagram sent to a closed port into
  `WSAECONNRESET` on the sender's next receive, which `receive()` cannot tell
  from nothing waiting: the relay, forwarding a client's knocks to a server
  that had gone, could stop reading with the server's goodbye still unread.
  POSIX gives an unconnected socket no such error, and the client and server
  could meet the same.
- `glideslope_datagram_check --answer PORT FILE SECONDS` stands in for a
  server that answers once and goes.

**Verification.** `the_relay_passes_on_what_its_server_sent_before_it_went`,
and the window drop test made to fail on purpose; the detail and the runs are
in the tail's entry below, under Tails.

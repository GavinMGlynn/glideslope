### UDP, on both kinds of system, 2026-09-21 — groundwork, no item of its own

**`src/platform/` had no socket code on any platform; now it has UDP on
both.** BSD sockets on Linux and macOS, Winsock on Windows, one header and
two implementations, and nothing above knows which it has. Like the take-off
autopilot this has no item in the plan - Phase 6 needs it and nothing else
does.

**Nothing blocks.** A socket is non-blocking from the moment it is made:
`receive` answers at once with what was waiting, or with nothing. The
simulation steps at a fixed rate and cannot wait on a datagram that may never
come. **Nothing throws** either: a socket that cannot be made is an empty
optional, and a send or receive that fails says so.

**An address is bytes, not a name.** Nothing here resolves a host name -
that is a blocking call into the system's resolver, and it belongs where
waiting is allowed - so `localhost:26000` is refused as firmly as
`256.0.0.1:1`. The parser and the printer are shared by both systems and use
no system headers at all, so the two cannot drift.

**A datagram is at most 1232 bytes.** IPv6 obliges every path to carry 1280;
40 are its header and 8 are UDP's. Nothing this sends is fragmented, and more
than 1232 bytes is refused rather than broken up. One that arrives too long
for the buffer is dropped rather than cut, because half a datagram is not a
datagram.

**Winsock has to be started before it can be used**, once per process, and
stopped as many times as it was started; a counter does that at the first
socket and undoes it with the last, so nothing above has to know.

**Verification run.** Five tests:
`an_address_written_down_and_read_back_is_the_same_address` walks nine
addresses of both families, each written, read back, and written again;
`anything_that_is_not_an_address_is_refused` walks twenty-three things that
are not one, host names among them, each with its reason in the test;
`a_datagram_sent_to_the_loopback_arrives_whole_and_says_where_from`;
`a_datagram_too_large_for_the_smallest_path_is_refused`; and
`a_socket_gets_the_port_it_asks_for_and_no_two_share_one`. Watched to fail
with the size guard taken out of `send`: "a datagram of 1233 bytes is
refused". **The Windows implementation is written and has never been run
here** - this machine is WSL - so CI's two Windows jobs are the first thing
that will have compiled it. 312 of 312 tests pass locally at `-j4`,
in 972 s.

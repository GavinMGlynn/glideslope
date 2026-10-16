### Two network tests that failed on slow runners: the ground test's clients race no more, and the rate test's burst is read at once, 2026-10-08 — tails left open for their month of CI

**What is still missing first**: both tails' verifications ask for a month of
CI runs without a failure, which has not passed.

**The ground test's last race.**
`a_client_told_the_servers_collision_ground_refuses_other_ground_and_leaves`
failed again on CI's macOS release (job 112807518355, 2026-10-07) with
"1;1;0", after both earlier fixes. What the clients said shows why: the
server admitted only the client on other ground, which refused the ground and
left 0.048 s later; "everybody who joined has gone, 0.1 s in", the server
(`--until-empty`) stopped, and the client on the same ground, whose first
word had not yet been read, heard "no answer from". The two clients started
together, so which joined first was the runner's choice. **Reproduced** by
starting the same-ground client two seconds late: "1;1;0", "no answer from",
exactly CI's words. **Now each step waits on the one before**: the client on
the same ground writes a file once it has been told the ground and answered a
knock (`glideslope_cli connect --told-file FILE`), and the client on other
ground starts only once that file exists (`--after-ready`), so the server is
never empty until both have been. The script takes `-DSAME_AFTER=S`,
`-DOTHER_AFTER=S` and `-DSERVER_EXTRA=...` to stagger the three. **Verified**:
passes with the same-ground client 2 s late, the other 2 s late, and the
server held to 300 ms a step (`--test-step-ms 300`); and 50 runs of 50 green
pinned to two cores beside three busy loops. On failure it prints the
server's output and each client's own words, or that it never began.

**The rate test's burst.** `a_client_sending_faster_than_its_stated_rates_is_held_to_them`
failed on Windows clang-cl (job 112799347926) and debug with "the server took
10 of 50 requests sent at once, not 8 or 9". The client sent its 50 requests
straight after its 1,000 pings, into a datagram budget the pings had emptied:
those dropped unread were sent again by the reliable stream a quarter of a
second later, and by then the request budget had refilled two. On a fast
machine almost none got through the first time and all were read together,
8 taken; a slow runner let a few through first, 10. **Reproduced** by sending
the requests 50 ms after the pings: 10 taken, twice in three, and the server
(now saying when it read them) "read over 0.251 s". **Now** the client sends
the requests a second after the server has answered its last knock - the
datagram budget full again - and the server says over how long it read a
session's requests ("read over X s"); the test holds the bucket's own rule,
at most a second's worth plus the rate times that time (to the millisecond,
plus one), at least a second's worth, and fewer than all. Read over 0 ms
here, 8 taken. **Seen to fail**: with the request budget one a second over
its stated rate, "took 9 of 50 requests read over 5 ms, not from 8 to 8".
**Verified**: 50 of 50 green pinned to two cores beside three busy loops.

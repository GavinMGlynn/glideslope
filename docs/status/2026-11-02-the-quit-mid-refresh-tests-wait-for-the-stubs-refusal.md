### The quit-mid-refresh tests quit only once the stub has turned the refresh away, 2026-11-02

**What is still not true first.** The new wait has not yet run on CI; it was
seen red and green only on this machine, and the stalled variant was not
shown red.

**The 429 quit test on Rocky 9 (job 125180276633) now waits on the
event.** "answered 0 requests 429, and 4 with FILE": the stub, which counts
every connection it accepts, was never reached by the refresh before the
client quit - the refresh had begun 37 ms before the client ended, five
frames of two ticks. Not #300's doing: #300 line-buffers standard error, and
the lines this test times from go to standard output, flushed as said; the
stub's count is of connections. Of the failed test jobs in the last 172 runs
of ci.yml and nightly.yml it is this test's only failure, and 20 loops here
passed (the refresh begun 34 to 44 ms before the quit ended). But a test that
only sometimes builds its situation is worse than none, so the situation is
now built by an event: `glideslope_http_stub --answered FILE` writes FILE once
it has answered its first request with its status (or begun its stalled
transfer), and the client's `--quit-once FILE` holds the quit at `--quit-at`,
flying on and drawing no shot, until FILE exists (two minutes at most). The
quit's flight waits on it; the control does not. The 2 s bound is unchanged,
timed from the quit as before.

**Verification.** Both quit tests on vulkan (429 and stalled), linux-release,
DISPLAY and WAYLAND_DISPLAY unset: 20 of 20 passed each. Seen red: with the
weather refresh delayed 300 ms before its first request (a sleep put into
`WeatherFetch`, removed after) and the quit's `--quit-once` taken out, the
429 test failed 20 of 20 on "the refresh was never turned away"; with the
delay and the wait, it passed 5 of 5.

### A long frame does not put the window client's own aircraft right too far: a cause found and fixed, 2026-10-02 — tail still open

**What is still missing, first**: none of CI's four failures was reproduced
here in the processes themselves, so that this was their cause is argued
from a unit test and the numbers, not watched; one loaded run with the fix
was still put right 12.8 m, and after the rebase two of three 9.4 and
11.3 m, all under the bound but not explained, and all once the clocks'
difference was known (below); and
the tail's verification is a month of CI runs, which starts with this
change.

**The cause.** The window client's pass read the stick, sent it, and then
flew every tick the frame was owed on it - so an input sent at the end of a
long frame was flown here from that frame's beginning, and on the server,
which cannot fly an input before it arrives, a whole frame later. The
prediction takes the clocks' difference from where each input began here
against where the server applied it (`sim::Prediction::hear_clock`), so that
input said the clocks differ by the frame more than they do. Once the
difference is known, the least of two seconds' words leaves such an input
out. But while it is being learnt (the first 25 words after joining, or
after a take-back) it can be the least heard; when an input sent
after a quick frame came, the difference fell by the long frame, and the
aircraft was put right by the way flown in it. The guess in the tail - the
clock estimate thrown off by an input begun at the head of a long pass's
catch-up ticks - was right; the input said the clocks differ by more, not
less, which is why only the learning was hurt. A frame's length long or
short also moved every input's word on the clocks, by the frame before it
was sent: frames of 20 to 230 ms at random moved the known difference by
three steps and put the client right by 1.05 m (a probe, not kept).

**Built in a unit test**:
`a_long_frame_puts_a_client_right_by_little_whether_or_not_its_clocks_difference_is_known_yet`.
A client paced by real time against a server on its own, in microseconds,
each pass the window client's; long frames of 142, 167, 284, 428 and 730 ms
(CI's, and 428), each at the first and second frame after the prediction
began - where the first input whose beginning is known is the one sent at
the long frame's end, which the test checks - and three seconds on, when
the difference is known, at 0 and 5 ms each way: thirty flights, counted.
Every correction is held to 3 m, learning or known. **Seen to fail** with
the pass as it was (sent, then flown on), and again with that order put
back into `fly_a_pass` itself: "142 ms 1 frames in, 0 ms each way: put
right by 7.709581 m, over the 3.000000 m bound"; the probe before it
measured 7.7 m at 142 ms, 16 at 284, 25 at 428 and 43 at 730, in the
learning, and nothing over 0.6 m once known. With each deliberate bug
reverted, it passes: the worst
learning 2.57 m (placing a word to the step, half a metre a step at 120
knots), the worst known 0.33 m. The model flies its passes through the
window client's own `glideslope::client::fly_a_pass` (below), its parts the
model's, so the order tested is main.cpp's.

**Fixed** in the window client (`Online::fly`, `Online::flown`, and the
order in `glideslope::client::fly_a_pass`, src/frontend/client/pass.hpp,
which main.cpp's passes on a server and the unit test's both go through,
so that putting the order back turns the test red): the stick
is still sent as the frame's clock is read, but the ticks - the frame gone
by - are flown on the input sent before, which is what the server flew
over the same time, and the new one from the next tick. Each input now
begins here at the step it was sent at. The first input, with none before
it, is flown at once. The cost is a frame's latency on the controls
online: 17 ms at 60 frames a second.

**And a second, smaller error, found on the way**: the pass read the socket
after its ticks, so a word that came while they were flown was about a
moment past the last of them, and the prediction was put forward to it
with nothing to replay, and back by the next word. Printed by a debug line
(not kept) on the held-frames test, linux-debug, on four cores shared with
three busy loops and the other window test: corrections of 2.0 to 4.0 m
each "replayed 0", every one of them. Now `Online::listen` reads what has
arrived when the clock is read, before the ticks, and `Online::hear` hears
it after them, as of the time it was read. Nothing tests this half but the
window tests.

**With the session kept by its own thread** (#88, which this follows): the
keeper polls the session behind the lock while the frame loop is away, so
`listen` takes the lock to read, and notes the session's count of joinings
again; `hear` takes it again for what it hears, and if the session was
joined again in between - the keeper heard it during the ticks - what was
read is the old session's and is let go, not heard as the new one's: that
would name the old aircraft as this client's own, and take the newest
word's time from a restarted server's old clock, so that every later word
was ignored as older. Not tested: building a join-again inside one pass's
ticks needs the server restarted, with the same key, at a moment the
client is mid-pass, which neither a unit test (Online needs a socket
session) nor the window tests (which restart it between passes) can place.
As #88 does, the lock is let go before a word puts the flight right.

**Said by the client now**: "the worst while its clocks' difference was
learnt X m, and once it was known Y m", after the corrections line, and
the on-server window tests repeat it whether they pass or fail, so that a
run on CI says which it was.

**Verified**, linux-debug, DISPLAY unset: the unit test above, and the
prediction unit tests. The on-server window test and the held-frames one
(`client_on_server.cmake` run by hand on ports of their own, 31811 and
31812, so as not to meet another working copy's), ten runs of each side by
side on four cores (taskset 4-7) shared with three busy loops: all twenty
pass; the held-frames worst 0.32 to 1.39 m, the on-server worst 0.47 to
1.08 m in nine and **12.8 m in one** - under 20 m, but not explained: that
run said nothing of which half it was in, which is why the tests now say
it. A candidate (from the review): the server's mirror of the cause - a
long server pass applies an input that came early only after its owed
steps, so that input says the clocks differ by more than they do, and
while the difference is learnt that is a quarter of a second at 50 m/s,
about 12.8 m. If so, the new line would say "learnt"; it did not. Rebased
on #88, three of each again under the same load (and a Windows build
running): held-frames worst 1.35, 3.82 and 1.30 m, on-server 9.38, 11.33
and 1.29 m - and the 9.38 and 11.33 m came **once the difference was
known** (learnt 0.54 and 1.84 m). So what is left is not this cause's
learning. A guess not yet tested: a server starved of its core falls
behind real time and catches up four steps a look, applying an input that
came meanwhile at a step behind the clock, so that input says the clocks
differ by less than they do, the least of the window drops, and the
client is put right by the server's lag - the open item "A server that
falls behind real time puts its clients' prediction off by metres". Before
the fix, the
same load, three of each: held-frames worst 1.4,
2.2 and 4.0 m, the "replayed 0" corrections above; unloaded, three of each
way, all under 2.3 m. The two CLI network checks at 100 and 200 ms failed
once beside them, "no answer from 127.0.0.1:24781", and passed alone - the client
of the command line is unchanged, and that port is every working copy's. Windows: the client
and the unit tests build with MSVC (tools/windows_build.sh, windows-debug).

The command-line client (`glideslope_cli connect`) flies its own way and
counts only once the difference is known; it is not changed. The selftest
does not use the prediction; its hash is unchanged, 30ac70b84cab7d7c
(linux-debug).

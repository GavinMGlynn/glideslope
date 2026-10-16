### A client follows the server's clock at the server's own rate, 2026-09-25 — tail done

**Found by the network checks on CI's Windows debug runner.** Twice, the AI
aircraft was drawn 4.7 to 7.5 m from where it was, at 100 ms of latency. The
client kept time (its longest gap between frames was 121 ms), and nothing was
wrecked. The fault was its clock. It took the session's time from the fastest
update it had ever heard and counted on from there in real time. A debug
server on a loaded runner runs slower than real time, so the client got
further ahead of it every second. Everything it drew was then a guess carried
past the newest update (74 such frames).

**Reproduced here** by making the server's clock run at 80% of real time: the
AI was drawn up to 190 m from where it was.

**What changed.** `net::SessionClock` fits the rate as well as the offset: a
least-squares line through the last two seconds of updates. The offset is
then the one the fastest-arriving update of the last second gives at that
rate. The predicting client uses it, and `TRANSPORT.md` now tells any client
to do the same.

**Verified** by `the_client_follows_the_servers_clock_at_the_servers_own_rate`:

- nine sessions of a minute each: 80%, 100% and 125% of real time, each with
  no jitter, 30 ms and 60 ms of it, over 100 ms of latency and 5% loss;
- after two seconds the estimate is never more than 20 ms ahead of what the
  updates could say, nor 50 ms behind. At worst it was 9.9 ms ahead and
  28.1 ms behind;
- the rate is fitted to within 1%.

With the clock made to assume real time again, the test goes red at once
(203 ms ahead). Against the server at 80%, the network check's worst frame
went from 190 m to 2.0 m. That frame came in the first two seconds, while the
rate was still being learned.

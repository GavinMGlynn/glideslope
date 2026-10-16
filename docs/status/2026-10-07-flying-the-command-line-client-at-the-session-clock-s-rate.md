### Flying the command-line client at the session clock's rate, tried and taken out, 2026-10-07 — the tail stays open

**What is still missing first: all of it.** A server behind real time still
puts a predicting client off by metres; nothing here changed that.

**What was tried.** `glideslope_cli connect --predict` flew its own aircraft
at the session clock's fitted rate (`net::SessionClock::rate`) instead of
its machine's. Against a server slowed by sleeping in its steps
(`--test-step-ms`) here it cut the median prediction error from 1.3 m to
nought (worst 0.3 to 0.8 m). It did not survive CI (runs 37593355936,
37598776651): the sleeping server fell as far behind as each runner was slow
- 2,498 to 7,394 steps, under the rate's floor of a half - and the medians
were 0.36 to 5.5 m. Built deterministically instead, with the server's clock
run at a set 0.6 of real time (a `--test-pace` flag, not kept), it passed
once and then compared only 35 and 167 updates in 40 and 120 s: **pacing on
a fitted rate is open-loop**, and a rate fitted a little low leaves the
client's steps falling behind the server's, so the clocks' difference - the
least over two seconds - goes stale, the server's word is placed at steps
not yet predicted, and nothing is compared while corrections reach 8 m. A
pace has to be steered by the prediction's own clocks' difference, held
constant, not taken open-loop from the interpolation's fit - not attempted.

**The engine-stop test's Windows failures** (run 37583810544: medians after
47 and 429 mm against 43 and 51 before, worst 16.9 m) stay explained by the
same cause - servers behind real time: built here with its server slowed,
it failed unpaced (median after 16.2 m). They are not fixed.

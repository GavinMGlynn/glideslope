# A client's gusts are still a step early in up to half its steps, with inputs naming their step, 2026-11-21

**Closed on the bound, 2026-11-21 (the owner's call):** the item's
verification - gusting air within the steady air's bound - is met. The steps
flown early are a named limit, not fixed: holding each input to its named
step (a server input buffer) would cost one or two steps of input latency
and rework the held-up server's rules, for an error already inside the
bound.

**The named limit.** Inputs naming their step (protocol 19,
2026-11-17) did not close the Phase 12a item "A client's gusts are not the
server's while its clocks' difference settles". The prediction's median
error in gusts is within the steady air's bound, as it was before. But in
each of three runs, 14 to 59 per cent of the steps judged were still flown
one or two steps before the server flew them. The item stays `[ ]`. No code
changed.

**Why.** The server still flies the newest input it reads from the step
that is due when it reads it (`net::HeldInputs::stamp`). The step the client
names only spaces the older inputs read in the same pass. A word's own
clocks' difference is the server's step at arrival less the client's step
at sending, and it moves by a step or two with the phase of sending against
the server's passes. The client flies on the least of 50 words
(`sim::Prediction::hear_clock`), so every word above the least is flown
early by the gap. No client can know that gap before the word comes.

**The cure, not made here.** The server would hold each input until the
step its client named plus a difference the client steers. That is
Overwatch's command frame with its server-side input buffer and the
client's time dilation (Ford, GDC 2017). It adds a step or more of input
latency. It also changes what a server held up or stopped does with late
inputs (a held-up server's late input must not raise the hold for good),
and so it changes the paced, stopped-alone and held-up-unevenly tests'
rules. That is a design decision for the owner and more than this item's
hour. Searched: "server input buffer client tick command frame jitter buffer
Overwatch server holds inputs until their tick prediction".

**Measured.** Linux debug on this machine, main at 75630cd1, one run at a
time under the machine's heavy-job lock.
`a_client_predicting_in_gusts_meets_each_at_the_servers_moment_within_the_steady_bound`
was run three times and passed each time:

| run | judged | at the server's moment | early (most) | later | out otherwise | median | worst |
|-----|-------:|-----------------------:|-------------:|------:|--------------:|-------:|------:|
| 1   | 541    | 466                    | 75 (1)       | 0     | 0             | 0.030 m | 0.059 m |
| 2   | 540    | 407                    | 133 (1)      | 0     | 0             | 0.030 m | 0.058 m |
| 3   | 541    | 220                    | 321 (2)      | 0     | 0             | 0.033 m | 0.515 m |

Steady air (`a_client_flies_the_servers_weather_and_every_change_of_it_not_its_own`,
once): median 0.029 m, worst 0.055 m over 540 updates. The bound is 0.25 m
at the median.

Run 3's worst error, 0.515 m, was on a step flown two steps early. That is
the misplacement of 2026-11-17's table, when the estimate moves, which steady
air shows too.

The test is unchanged. It cannot yet assert that no step is flown early.
The selftest's hash does not move: no code changed.

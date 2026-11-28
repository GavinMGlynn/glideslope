### The racetrack test of two going around is given the time a slow runner needs, 2026-11-29

**What was wrong.** `the_one_behind_two_going_around_whose_leader_goes_around_again_flies_a_racetrack_and_both_land`
gave up at the test presets' 900 s on Linux debug three times: #365's run
43261047217 (shard 11/11, 2026-11-12), and runs 46157212817 and 46158030097
(shard 2/15, 2026-11-28, two branches). It neither hangs nor runs away: it is a
sim-only unit test (no server, no client, no `--until-empty`), four pairs each
flown in one loop bounded at 3,600 s of simulated time, ending when both are
down and clear. It is slow, and its time limit followed a fast measurement down.

**Why it gave up.** Its Linux debug CI figures, as measured: 370 s (table of
2026-11-11), 798 and 856 s (2026-11-12, run 43138678345 and the table), 464 s
(table of 2026-11-27), 788 s (run 46157163819, shard 2, green), then 900+ twice.
#365's run gave up with 856 s in the table and the presets' 900 as its limit;
the rule that a test's limit is half as long again as its slowest table figure
was made for it (1,284 s at 856). When the costs were measured again on 2026-11-27 its figure fell to 464 s, so
its limit fell back to the presets' 900. The new dealing of 2026-11-28 put it
in Linux debug's shard 2 beside the Vulkan client-drawing tests, and in the
timed-out run the shard's tests took up to 2.2 times their table figures
(F-22A orbit 530 s against 275, 737-300 orbit 351 against 170): 464 times 1.94
is the 900. The PR it failed on (#435) changes no simulation code the test
runs; Windows debug ran it in 239 s on the same dealing.

The Windows debug shard 6/13 that overran its 45 minutes in run 46146207168 has
no log; nothing ties it to this test beyond holding it.

**What changed.** `tests/CMakeLists.txt` sets the test's TIMEOUT to 1,800 s by
hand, so a faster measurement cannot lower it below what a slow runner needs
(the tables can still raise it). Each pair's line is flushed as it is flown,
and says the simulated seconds it flew: a run given up at its limit now says
how far it got - the three runs above printed nothing.

**Verification.** The failure is seen in CI's logs, three times at 900.1 to
900.2 s, and the slowdown measured from the timed-out shard's other tests.
Not yet done, and why the item stays `[ ]`: a local run under load seen give
up at 900 and pass at 1,800 (the machine's heavy-job lock was held by another
suite for the whole of this item's time box), and this branch's full CI run.

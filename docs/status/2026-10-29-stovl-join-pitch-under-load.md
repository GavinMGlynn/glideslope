### The STOVL join's predicted pitch under load, 2026-10-29 - cause found, not fixed; the tests stay on pull requests

**What is still not true first.** Under load, a client that joins an F-35B
whose STOVL law is flying her jet-borne does not keep her predicted pitch
within half a degree of the server's. The cause is in her law's jet-borne
state: no word carries it, and it drifts away from the server's. Most of the
drift comes during the 150 inputs B sends before it selects STOVL mode. That
is not fixed. **Both tests stay on every pull request's run**: CLAUDE.md
moves a test to the nightly label when it fails twice on CI for one cause.
The lost scene has failed on CI once (0.592, run 40800355368), and the join
scene never has. The local failures under load are recorded here, not
counted toward that rule. A second CI failure of the lost scene for this
cause moves it to the nightly label then.

**The failures.** CI's Ubuntu linux-debug run 40800355368 gave 0.592
degrees in the lost scene, with no machine stalls, a median of 41 mm and
only 0.030 m off the path. On Linux debug here, 14 busy loops on 20 threads
(load average 14 to 16) beside the tests:

| Runs | Join scene pitch | Lost scene pitch |
|---|---|---|
| 2026-10-29 earlier (stovl-lost-join-median-measured) | 0.539 in 1 of 5 | 0.502, 0.547 in 2 of 5 |
| here, as on main | 0.264, 0.456, 0.309 | 0.892, 0.266, 0.381 |

**What was ruled out: the comparison.** The client's track (`--track`) now
gives two pitch errors for each update compared: one at the step the word
was placed at, and one at the step nearest it in place. In the 0.892 run,
the worst 25 updates had 0 steps off, and the two errors were the same to
the digit. The error builds over seconds, from 0 at 4 s to 0.89 at 5.7 s
and back to 0.06 by 9.4 s. Through all of that her position stays within
54 mm, because every word puts the motion right, but her law's integrals
are not put right. So the word is compared at the right step, and it is
not the catching-up after a stall either, since there were none.

**Where the worst lies.** The worst error falls between inputs 100 and 241,
and after input 250 the errors are 0.04 to 0.30 degrees. Client B is the
one that joins. For its first 150 inputs (`stovl_selected_from`), B sends
what a fresh aircraft sends: the selector off and the lever at 0.6. Her law
starts converting her back towards the wing, and turns again at the 150th
input. So the test header's "its selector on, the stick centred, as A left
them" was not true for those seven seconds.

**Tried, and reverted: selecting from B's first input when it is given her
in STOVL mode.** Under the same load, over two rounds:

| Scene | Pitch | Median | 95th pct |
|---|---|---|---|
| join | 0.040, 0.049 deg | 68, 98 mm (bound 60) | 133, 174 mm |
| lost | 1.933, 1.975 deg | 183, 197 mm (bound 120) | 260, 259 mm |

The join scene's pitch became good. The lost scene drifted steadily
instead: her speed went 1.8 m/s and her pitch 1.8 degrees off, and stayed
there for the whole run. With the manoeuvre gone, her law holds jet-borne,
and the server's and the client's jet-borne laws move apart unchecked.
Phase 9 says those laws need no telling once both change stage on the same
step (0.012 degrees). That was measured on a client that selected for
itself, not one that joined a law already flying her. This is not a small
fix, so the change was reverted. The lead for whoever takes it: compare the
jet-borne integrals in LAW_STATE (`Transition::State`) with what the server
holds after the join, and see whether the lever's latch at selection is the
same on both sides.

**The decision.** The cause is recorded. Both tests stay on pull requests
(see above), and the line goes against Phase 12a's open busy-machine item.
Nothing in the test's flying or bounds is changed. Its header now says
what B actually sends: the selector off and the lever at 0.6 for its first
150 inputs.

**The track's pitch fields, kept.** Each `compared` line in the client's
`--track` now ends with her pitch error at the step placed and at the
nearest step in place. Those two fields told the law's drift apart from a
word placed a step off, which the existing fields could not do. They are
kept for whoever takes up the jet-borne state. They change no printed
figure and no test's reading.

**Verification.** The figures above come from the real output, Linux debug
under load. No test is added or bound changed, so there is no new check to
see red.

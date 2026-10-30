### The STOVL at-once slow scene's pitch drift measured on CI's Windows, 2026-10-30

**What is still not true first.** The cause is named from CI's figures, not
reproduced: on Windows debug here the slow scene was run once, unloaded
(0.001), not 20 times nor loaded - the Windows working copy was another
item's for most of this one. The bound below is from six and seven CI runs
on Windows debug - few - and
the mechanism (her prediction flying further from each correction when the
machine is loaded) is inferred from which scenes drift, not instrumented.

**The failure.** #280's full run 40992438234, Windows debug, shard 3/7:
`a_client_slower_to_prepare_than_the_servers_timeout_is_not_let_go_and_seeds_its_law_as_the_server_does`,
"predicted pitch where she was within 0.012 degrees over 3000 updates,
matched 0 steps and 0.001 m off; 0.017 degrees at the step placed at, 5
steps off", red against 0.010. Matched 0 steps and 0.001 m off the path:
not placement, as #276's failures were - her pitch at the place she was
differed by 0.012 degree. The client said 71 steps flown again between two
words at most, 37,129 in all; the test ran beside the shard's heaviest
tests (the HUD on Vulkan, the orbit tests).

**The figures.** Every CI test job of the three at-once scenes since #276
(`gh run view --job --log`, the `figure` lines, 38 runs listed, 2026-10-29
06:00 UTC on), the figure where she was, n and worst per platform:

| platform | lone (RUN_SERIAL) | slow | spent |
|---|---|---|---|
| Ubuntu release | 4, 0.001 | 4, 0.001 | 4, 0.001 |
| Ubuntu debug | 6, 0.001 | 6, 0.002 | 6, 0.002 |
| Rocky 9 | 4, 0.002 | 4, 0.001 | 4, 0.001 |
| macOS release | 4, 0.002 | 4, 0.001 | 4, 0.002 |
| macOS debug | 1, 0.001 | 1, 0.001 | 1, 0.001 |
| Windows release | 4, 0.001 | 4, 0.002 | 4, 0.002 |
| Windows clang | 4, 0.002 | 4, 0.001 | 4, 0.002 |
| **Windows debug** | 6, 0.002 | **6, 0.012** (0.012, 0.008, 0.005, 0.005, 0.005, 0.004) | **7, 0.011** (0.011, 0.007, 0.003, 0.003, 0.002, 0.002, 0.001) |

The spent scene's 0.011 is #281's draft run on 0049630a, red there against
0.010 - two of the three scenes have now passed #276's bound on CI's
Windows debug, the two not run alone.

Here: linux-release, the slow scene alone, 20 runs: 0.001 in every one.
Windows debug (tools/windows_build.sh, the three scenes one after another,
unloaded), one round: lone 0.002, slow 0.001, spent 0.001 - the slow scene
drifts here no more than the others when nothing runs beside it.

**Cause, as far as the figures say.** Only Windows debug, and only the two
scenes that are not RUN_SERIAL; the lone scene, the same flight run alone,
is 0.002 at most there too. The three scenes fly her alike once she has
joined - the slow one only joins six seconds later - so what differs is the
load beside them: a debug build on a loaded runner flies more steps between
corrections, and her pitch drifts from the server's over them. Floating
point drift of the law over the replay, bounded, as CLAUDE.md allows; not a
seeding fault (the throttle integral and pitch trim agree to 17 digits in
the failing run, as the test checks).

**The bound.** The lone scene stays 0.010 (0.002 at most, every platform).
The slow and spent scenes 0.025: the slow scene's six on Windows debug have
mean 0.0065 and standard deviation 0.003, and 0.025 is six of those above
the mean; the spent scene's seven, mean 0.0041 and s.d. 0.0037, put six
above at 0.026. 0.025 is under half
the 0.054 a law gone astray made (2026-10-29), so that test is still red
for it. Stated in tests/CMakeLists.txt beside each test.

**Not done, a tail.** The three scenes 20 times on Windows debug here,
unloaded and loaded, to see the drift follow the load and to grow n.

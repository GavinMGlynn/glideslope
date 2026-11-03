### The trim walk a height at a time - 2026-11-04

**What was not true.** The walk that trims every aircraft, at every loading,
at three heights and three speeds, as a player's is trimmed, was one test.
On Linux debug it took 435 to 771 s on CI's runners (runs 41656529894,
41658361664, 41660502001, 41662691322), against its 900 s limit, and ran past
it twice for that one cause, on two branches that do not touch it: #307 (run
41683267179) and #311 (run 41685289441).

**Now.** It is three tests, one for each height, each walking every
aircraft at every loading at three speeds at its own height:
`every_aircraft_at_every_loading_at_three_speeds_at_{1000,5000,10000}_ft_is_trimmed_as_a_players_is`.
Each asserts its own space, walked, trimmed or left out by name, and that
every point on the left-out list at its height was walked and failed. The
space and the list are the same as before. tests/ci_costs splits the old
cost three ways until the next measurement.

**Verification.** linux-release, this branch: the three pass in 6.9, 8.2 and
8.9 s, 183 points each, 549 in all as before; 1,000 and 5,000 ft trimmed
whole, 10,000 ft 169 trimmed and the 14 left out walked and failing, the
F-22A's 6 a height in pitch alone, said. Seen red: the left-out count taken
from the points at other heights failed the 1,000 ft test ("every point at
this height on the list of 14 left out was walked and failed: 0 walked"),
then reverted. From the review: each test also checks that every point on the
left-out list is at one of the walk's heights, since a point at any other
height would otherwise be left out by none of them. Seen red with a point at
3,000 ft added ("15 left out ... 14"), then removed. Linux debug's time is the train's full CI run.

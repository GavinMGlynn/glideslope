### The slowest orbit tests split, one to an aircraft, so no CI shard runs out of time, 2026-10-07 — fix

CI's Ubuntu linux-debug shard 3 was cancelled at the job's 30 minutes on
every pull request by two tests on main: `no_aircraft_holds_what_a_plan_asks_one_step_past_its_slowest_or_fastest`
(1,282 s) and the airliners' and business jets' fastest tightest orbits
(past its 900 s timeout). **Now each aircraft has its own**:
`the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_<aircraft>`,
`..._fastest_..._by_the_<aircraft>` and
`the_<aircraft>_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest`;
the S.23's and the F-22's orbits each way round in a test of its own, and
their two bounds apart. Each orbit is held to its group's limit as before
(60 m, or 5% and 2% of the radius), and
`every_aircraft_has_its_own_tests_of_its_tightest_orbits_and_one_step_past_them`
asserts every class in a group once and every aircraft in the catalogue
tested once - seen red with the F-35B left off its list.

**One step past asks the cheap check first**, which proves "not held" as
well as the whole: past the fastest, power in hand (the S.23 at 151 kt held
its orbits and heading, 108 s, before its 151.9 kt level failed it); below
the slowest, the heading in calm air and a crosswind (the F-22 at 245 kt
held its orbits, 69 s, then swung 1.6 degrees of sideslip). Only where those
hold is the rest of `sim::holds_plan_speed` flown. Seen red: the S.23's
fastest put at 131, it holds 141; the B-2's slowest put at 204, it holds 194.

**Times**, this machine's linux-debug, four at once: the longest the F-15C's
fastest orbits, 55 s, the A380's 51, the 747-400's, B-2's and F-35B's 47;
every one-step-past test under 7 s. CI's linux-debug ran the one-step-past
test 4.19 times as long as this machine did (1,282 s against 306), so
tests/ci_costs/linux-debug.txt has these times times 4.19 - the longest 232
s - and every other preset's are those scaled by its median ratio to
linux-release over its tests of 30 s or more, against linux-debug's 3.18.
Estimates until CI measures them.

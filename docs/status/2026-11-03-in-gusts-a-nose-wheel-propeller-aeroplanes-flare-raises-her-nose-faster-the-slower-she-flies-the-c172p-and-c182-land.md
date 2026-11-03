### In gusts a nose-wheel propeller aeroplane's flare raises her nose faster the slower she flies: the C172P and C182 land on every seed, and seeds 9 to 40 are flown every night, 2026-11-03 - item still open

**What is still missing, first**: the item. Six of sixteen are still named
in `not_yet_landed_in_gusts()` (tests/unit/test_lesson.cpp), the J-3 Cub
among them, unchanged by this: on forty seeds a gust lifts her at 137, 132
and 78 ft/min in her flare (seeds 20, 37, 40), she touches at 356 (seed 33)
and rises 0.82 ft (seed 39); on Windows release she climbs on seeds 32 and
37 and touches at 357 on seed 8. Seeds 9 to 40 are seen on linux-release,
an x86-64-v2 build and Windows release here, and on every CI preset in a
run dispatched on this branch (below): every failure there is named on its
seed and platform.

**What now works**: the C172P and C182 are judged, unnamed but for macOS's
seed 16 (both) and 34 (the C182): ten of sixteen
land in moderate turbulence within limits with every margin on the eight
gust seeds of every pull request, and on the forty of every night. No
aeroplane judged before is worse on any of the forty seeds on any platform
seen; the change touches a nose-wheel propeller aeroplane's gusty flare
alone (the C172P, C182 and PA-28), and jets, tailwheel aeroplanes, the
flying boat and calm air are untouched.

**The forty seeds, asserted** (#308's review): the gust test's seeds 9 to
40 are tests of their own (`..._seed_9` to `_40`), each aeroplane judged or
named on each seed as on the eight, labelled `gust_sweep`
(tests/CMakeLists.txt): forty seeds' flights are too long for a pull
request's run. ci.yml leaves the label out of a pull request's runs, as it
does `nightly`'s, and every other run - the nightly schedule's, where a
failure is a red tree - has it; tests/cmake/ci_shards.cmake asserts both,
and that each dealing leaves out what its run does. Not the `nightly`
label: tests/cmake/nightly_tests.cmake is for tests moved out of pull
requests after failing on CI, which these have not. **A stale name fails**:
a judged aeroplane named on a seed she passes with every margin fails that
seed's test, "take her name off" (seen red with a name put on the PA-28's
seed 9, which she passes: "pa28 (gusts, seed 9) is named on this seed and
passed it with every margin: take her name off"). A judged aeroplane that fails a seed on a
platform is named on that seed and platform (`named_on_a_seed`): on Linux
the F-15C's seed 34 (a float into a go-around) and the flying boat's seeds
11, 13, 25, 28, 30, 32 and 40; on Windows the 747-400's seeds 29, 31, 33
and 40 (a hard touch and three floats into a go-around) and the flying
boat's 24 and 38. All were so before this change.

**Every CI preset, measured** (#308's review): ci.yml run 41640422104,
dispatched on this branch, every preset with the label. Ubuntu linux-debug
and linux-release, Rocky 9 linux-release and Windows debug, release and
clang passed all forty seeds with the names above: none stale. macOS, debug
and release alike (the same figures on both), failed these, now named for
macOS on its seed: the flying boat's 3 (0.93 ft; before this named for
macOS release alone, and main's nightly run 41544342155 failed it on macOS
debug), 10 (1.13 ft) and 11 (2.03 ft); the C172P's 16 (396 ft/min) and the
C182's 16 (a gust climbing her at 96 ft/min in her flare); the 747-400's 24
(410 ft/min); the F-15C's 28 (981 m along, past the zone); and the C182's
and PA-28's 34 (climbing at 92 and 76 ft/min in the flare). How macOS flew
those seeds before this change is not measured: no run had the forty there.
The run's one other failure, `an_autopilot_engaged_on_an_aeroplane_already_stalled_recovers_it`
on macOS debug (JSBSim's FGTable.cpp:618 assertion, a table looked up out
of its range), is not a gust test and was not in main's last nightly run;
its cause is not looked for here.

**What the label costs a night**, its tests' summed time in that run
(ctest's label summary, test-seconds; jobs run tests in parallel, so a
runner's wall time is less): Ubuntu linux-debug 4,584 s, Windows debug
2,173, macOS debug 1,186, Windows release 494 and clang 481, Ubuntu
linux-release 410, Rocky 9 linux-release 322, macOS release 189: about
9,840 test-seconds, 2.7 hours, of which the debug presets are 7,943. Kept
on every preset: macOS debug named the flying boat's seed 3 that macOS
release alone had been named on.

**What the AI was doing** (linux-release, traced): the C182 on seed 23 lost
her headwind five feet up, 70 kt to 55; her flare asked for 9.1 degrees of
nose and she had 5.7 at the touch, her sink grown from 210 to 390 ft/min,
and she met the runway at 377. The attitude asked rose at its 6 degrees a
second, but the pitch loop damps any nose-up rate past a degree a second
(`want_q_degps`, tuned on 2026-11-02 for every aeroplane), and a slow
aeroplane needs more nose for the same gust's sink: the change of path
that arrests it is that sink over her speed.

**The published technique** (each quoted as printed, the FAA's Airplane
Flying Handbook, FAA-H-8083-3C):

- Ch. 9, "Round Out", p. 9-8: "The round out rate should also be
  proportional to the rate of closure with the ground", and "the pitch
  attitude should be increased at a faster rate when full flaps are used".
- Ch. 14, "Wheel Landing", p. 14-5: "In some wind conditions, the need to
  retain control authority may make it desirable to make contact with the
  runway at a higher airspeed than that associated with the three-point
  attitude. This necessitates landing in a flatter pitch attitude on the
  main wheels only"; "a soft touchdown and a slight relaxation of back
  elevator just after the wheels touch are key ingredients"; and
  "Three-Point Landing", p. 14-5: "application of full back elevator during
  the flare lowers the tail, increases the AOA, and quite naturally puts
  the plane in climbing flight." (Read from the FAA's own PDFs of the
  chapters, faasafety.gov's for ch. 9 and faa.gov's for ch. 14.)

**The change** (src/sim/lander.cpp, `gusty_nose_rate_kt_degps`): in a
gusty flare a nose-wheel propeller aeroplane's undamped nose-up rate is 265
knot-degrees a second over her approach speed, never less than a degree a
second: 3.7 degrees a second for a C182 at 72 kt. The 265 is tuned, not
sourced (about a quarter of a g of change in her climb). A jet keeps a
degree a second: given it, on Windows the F-15C and F-35B floated to within
25 m of the touchdown zone's end on three seeds they had passed. A
tailwheel aeroplane keeps it too, as ch. 14 wheel-lands her flatter in
gusty wind and warns that back elevator in the flare puts her climbing:
given it, the Cub, at 5.2 degrees a second, was lifted in her flare on
three more seeds (9, 19, 38: 32 of forty, was 35), and on Windows the
Mosquito, at 2.4, struck her airframe on seed 40, which she had passed.
The gate tried first, the faster rate only below her approach speed (the
lull traced) or only while she sinks faster than her flare wants (ch. 9's
closure), left the Cub at 32 and 34: both put her nose up in a gust's
float all the same.

**Tried and measured** (forty seeds, passing every margin; linux-release
unless said):

| Variant | C172P | C182 | Cub | Worse |
|---|---|---|---|---|
| As it was | 40 (327) | 38 (377) | 35 | - |
| Throttle opened at 1/s, not 0.5 | 40 | 38 | 35 | - |
| Cushion from 75 ft/min, not 150 | 40 | 38 | 34 | - |
| Touch flown at 100 ft/min, not 150 | 39 | 40 | 33 | - |
| Speed held to the approach speed | 40 | 38 | 34 | - |
| Nose rate 3 deg/s, every aeroplane | 40 | 39 | 34 | Mosquito 37, A380, F-15C 35, 747 38 |
| Nose rate 6 deg/s, every aeroplane | 39 | 40 | 33 | Mosquito 3, F-15C 14, F-35B 24 |
| 250 kt-deg/s over speed, every aeroplane | 40 | 39 (338) | 32 | - |
| 265, every aeroplane | 40 (248) | 40 (331) | 32 | Windows: F-15C 39, F-35B 38, Mosquito 39 |
| 300, every aeroplane | 39 | 40 | 33 | F-35B 39, A380 35 |
| 265, propeller aeroplanes (round 1) | 40 | 40 | 32 | Windows: Mosquito 39 |
| Same, below her approach speed only | 40 | 40 | 32 | - |
| Same, sinking past the flare's want only | 40 | 40 (333) | 34 | - |
| **265, nose-wheel propeller aeroplanes (taken)** | **40 (248)** | **40 (331)** | **35** | **none** |

**Forty seeds, before and after** (pass with every margin; worst touch):

| Aircraft | Linux before | Linux after | x86-64-v2 after | Windows before | Windows after |
|---|---|---|---|---|---|
| C172P | 40 (327) | 40 (248) | 40 (248) | 38 (374) | 40 (268) |
| C182 | 38 (377) | 40 (331) | 40 (331) | 40 (312) | 40 (303) |
| J-3 Cub | 35 | 35 | 35 | 37 | 37 |
| Mosquito | 40 | 40 | 40 | 40 | 40 |
| PA-28 | 40 (320) | 40 (311) | 40 (311) | 40 (307) | 40 (298) |

Of those four the coordinator counted (C172P, C182, Cub, Mosquito): Linux
153 to 155, Windows 155 to 157. Every other aeroplane's forty seeds are
the same as before on all three builds. The C182's worst, 331 on seed 21,
is 4 ft/min inside the judged 335 on Linux.

**Verification**: `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_40` pass in linux-release (68 tests matching "gust|lesson", 100%) and,
seeds 1 to 40, on Windows release through tools/windows_build.sh.
**Seen red**: with the nose rate held to a degree a second for every
aeroplane (`jet_ || tail_wheel_` replaced by `true`), seeds 23 and 33
failed - "c182 (gusts, seed 23) touched at 377.212605 ft/min, not 25
inside 360" and 376.7 on seed 33 - and pass again restored.
`every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
and `every_compiled_unit_test_is_registered_with_ctest` pass with the 32
gust_sweep tests. The full suite is left to CI.

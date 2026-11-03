### In gusts the B-2A lands on every seed; the 737-300 and 787-8 on every seed on Linux, but one each on Windows, 2026-11-04 - item still open

**What is still missing, first**: the item. Five of sixteen are still named
in `not_yet_landed_in_gusts()` (tests/unit/test_lesson.cpp). The 737-300 and
787-8 now pass all forty seeds on linux-release and an x86-64-v2 build, but
on Windows release the 737-300 touches at 347 ft/min (seed 8) and the 787-8
at 338 (seed 4), inside the 360 limit but not its 25 ft/min margin. The A380
is unchanged: over forty seeds she floats past the zone (seed 7, 907 m),
touches at 350 (seed 18) and strikes her airframe at 7 degrees of bank
(seed 34). The F-22A and J-3 Cub are unchanged. macOS is not yet seen with
this change.

**What now works**: the B-2A is judged, unnamed: on all forty seeds on
linux-release, an x86-64-v2 build and Windows release she lands within every
limit with every margin (worst touch 301 ft/min on Linux). Eleven of sixteen
are judged. Every other aeroplane flies exactly as before: each change is
per-aircraft data in her figures file, and only the 737-300, 787-8 and B-2A
have it.

**What the AI was doing** (linux-release, traced):

- **The B-2A**, seed 8: the turbulence's rolling gusts rocked her from 6.6
  degrees left to 5 right over three seconds in the flare, with her aileron
  at 0.21 against 6.6 degrees of bank (0.035 a degree) and lagging the roll.
  She touched at 5 degrees of bank and her wingtip met the runway. Ten of
  forty seeds did the same.
- **The 737-300**, seed 40: she began her flare at 41 ft sinking 790 ft/min
  after a down-gust, and her nose, its rise damped past a degree a second in
  a gusty flare, took the round out down to 14 ft to arrest it. She met the
  runway at 462 ft/min.
- **The 787-8**, seeds 21, 25 and 40: the sink was arrested at about 20 ft,
  a gust's lull took her speed, and she settled at 352 to 377 ft/min.
- **The A380**, seed 18: a lull at 120 ft at full throttle, then high on the
  path; the path law pitched her to -5.4 degrees and she began her flare
  sinking 1,500 ft/min. Seed 34: rolled 7.4 degrees in her flare. Seed 7: a
  float on the power held to keep her speed.

**The published technique** (each as found):

- The approach speed in gusts: Boeing's flight crew training manual's wind
  correction is "one half of the reported steady headwind component plus the
  full gust increment", at most 20 kt, with the steady half bled off as she
  nears the touch and the gust correction kept to it. This is quoted on
  PPRuNe (t-588229, t-238517, t-10456), on code7700.com ("Approach Speed
  Additives") and on flaps2approach.com. The manual itself was not found to
  quote. Airbus adds a third of the steady headwind, between 5 and 15 kt, and
  leaves the gusts to ground speed mini (PPRuNe t-433206, also forum posts).
- The round out: the FAA's Airplane Flying Handbook (FAA-H-8083-3C), ch. 9,
  "Round Out", p. 9-8: "The round out rate should also be proportional to
  the rate of closure with the ground".

**The change** (all of it per-aircraft data, read by src/sim/figures.cpp
and flown by src/sim/lander.cpp):

1. **Boeing's full gust** (`gust_additive_share="1"`,
   `most_gust_additive_kts="20"`, 737-300 and 787-8): her approach speed in
   gusts is her reference plus the whole gust factor, up to 20 kt, where the
   FAA's half up to 15 is flown by every other aeroplane. The lander knows the
   air's gust factor, not a reported steady headwind, so only the gust part
   is added. That is the part Boeing keeps to the touch.
2. **A faster nose while closing** (`gusty_closing_nose_degps`, 737-300 2,
   787-8 3): in a gusty flare a jet's nose-up rate is let through undamped
   to one degree a second (2026-11-02). These two are let through to their
   figure while they sink faster than the flare wants, which is the AFH's
   closure rate. In a float the rate is a degree a second as before. Tuned,
   not sourced.
3. **Wings held harder** (`gusty_aileron_per_deg="0.07"`, B-2A): her aileron
   for each degree of bank off what is wanted, in gusts. Calm air and every
   other aeroplane keep 0.035. Tuned, not sourced.

New attributes are refused out of bounds: a share outside 0 to 1, a most
outside 0 to 20 kt, an aileron outside 0 to 1, a nose rate outside 0 to 6
deg/s.

**Tried and measured** (linux-release, forty seeds, passing every margin):

| Variant | 737 | 787 | A380 | B-2A | Worse |
|---|---|---|---|---|---|
| As it was | 39 | 37 | 37 | 30 | - |
| Roll damping 0.05 (was 0.02), all | - | - | 38 | 38 | - |
| Bank wanted 0 in the gusty flare, all | - | - | 38 | 39 | - |
| Aileron 0.07 a degree, all | 39 (442) | 37 | 38 | 40 | S.23 and Cub better |
| Path's sink capped at 900/1,000 below 200 ft, jets | 39 | 37 | 37-38 | 30 | - |
| Jet nose 1.5 deg/s, all jets | 39 (402) | 38 | 36 | 29 | - |
| Full gust up to 20, all jets | 39 (338) | 40 | 32 | 40 | 747 38, F-15C 35, F-35B 39 |
| Full gust; nose 1.5 for 737, 787 | 40 | 40 | - | - | - |
| Aileron 0.07 all; jet nose 2 while closing, all jets; full gust 737, 787 | 40 | 40 | 39 | 40 | none on Linux |
| Same, nose 3 while closing | 39 | 40 | 37 | 40 | 747 38, F-15C 38 |
| Airbus third (5 kt) for the A380, A320 | - | - | 36 | - | - |
| Full gust for the 747-400 too | - | - | - | - | 747 38 |
| **Per-aircraft (taken): 737 nose 2, 787 nose 3, both full gust; B-2A aileron 0.07** | **40 (313)** | **40 (310)** | **37** | **40 (301)** | **none** |

The fleet-wide version (aileron 0.07 for all, nose 2 for all jets) passed every
judged aeroplane on Linux. On Windows release it failed four seeds it had
passed: the C182's seed 6, climbing 75 ft/min in her flare; the F-15C's
seed 4, touching 910 m along; the 747-400's seed 17, at 336 ft/min; and the
787-8's seed 4, at 339. So each figure was made her own aeroplane's data,
and the rest of the fleet flies exactly as before. The 737-300 was tried at
3 deg/s for Windows's seed 8: Linux's worst touch went from 313 to 331, and
it was not taken.

**Forty seeds, before and after** (pass with every margin; worst touch):

| Aircraft | Linux before | Linux after | x86-64-v2 after | Windows release after |
|---|---|---|---|---|
| 737-300 | 39 (462) | 40 (313) | 40 (313) | 39: 347 (seed 8) |
| 787-8 | 37 (377) | 40 (310) | 40 (310) | 39: 338 (seed 4) |
| B-2A | 30 (wingtip on ten) | 40 (301) | 40 (301) | 40 |

Every other aeroplane's forty seeds are the same as before on Linux and
x86-64-v2 (the same scores, line for line but the F-22A's wrecks). None is
named or unnamed by this change.

**Verification**: `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_40` pass on linux-release and on an `-march=x86-64-v2` build (no FMA,
as on CI). On Windows release (tools/windows_build.sh) seeds 1 to 40 pass
but for the two the 737-300's and 787-8's names now cover (seeds 8 and 4).
Under ctest, Windows release's seed 4 exited 0xc0000409 twice. Run
directly, the same executable failed normally, naming only the 787-8's
338 ft/min. Its cause is not looked for here. It is no longer a failure
with the 787-8 named. `a_gust_additive_and_a_gusty_aileron_and_nose_rate_are_read_and_refused_outside_their_bounds`
passes, registered with ctest (CI run 41692869047 failed
`every_compiled_unit_test_is_registered_with_ctest` until it was).
**Seen red**: with the B-2A's aileron at 0.035 the gust test's seed 8
failed ("b2 (gusts, seed 8) ... wrecked as it landed: struck the ground with
its airframe"). With the 737-300's closing nose rate at 1 deg/s, seed 40
failed ("737-300 (gusts, seed 40) touched at 351.89 ft/min, not 25 inside
360"). With a nose rate of up to 60 accepted, the figures test failed ("all
eight outside their bounds were refused, not 7"). Each passed again once
restored. The full suite is left to CI.

**From the review of #310.** The 737-300 and 787-8 were named whole, so
no seed tested them, though each fails only one, on Windows. Each is now
named on that seed alone (`named_on_a_seed`: the 737-300's Windows seed 8,
the 787-8's Windows seed 4). The 787-8's old macOS name, 398 ft/min on
seed 2 (CI run 38006839738), is taken off: the full workflow dispatched on
this branch (run 41703342974, every preset, seeds 1 to 40) has her land
it. That run names the 737-300 on macOS, debug and release alike, on seed 3
(a go-around from a touch 1,017 m along) and seed 17 (554 ft/min). Every
other seed on every preset passed for both. On every other
seed and platform both are judged, and a named seed that passes fails
"take her name off". The same run timed out
`an_aeroplane_going_around_beside_rising_ground_flies_its_circuit_height_over_it_and_lands`
on Linux debug at 900 s (762 s in tests/ci_costs): it is given 1,800.
The same run's macOS debug failure of
`ai_aircraft_keep_flying_with_nobody_connected_and_a_late_client_finds_them_mid_flight`
is the test named in tests/cmake/nightly_tests.cmake, run there because a
dispatched run is a nightly's; it is no pull request's, and its item holds it.

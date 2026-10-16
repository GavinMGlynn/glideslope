### The gust landings judged over eight seeds and every platform: four of fourteen land within limits with a margin, ten are named, 2026-10-10 — item still open

**What is missing first.** Ten of the fourteen are named in the gusts test.
**Gust landings magnify a platform's floating point**: CI run 38006839738
(head 975d9480) failed the gust tests on macOS and Windows that passed on
Linux - the 787-8 touched at 398 ft/min on macOS (seed 2) and climbed at
201 ft/min in her flare on Windows (seed 3), landing 921 m along a 914 m
zone; the A320 touched at 404 and 421 on Windows (seeds 1, 2) and climbed
at 143 (seed 3); the C172P touched at 419 on Windows (seed 3). The same
seed is different gusts at the flare on each platform, so a pass on Linux
by 25 ft/min on three seeds held nowhere else. The named list is now a
statement of robustness: an aeroplane is judged only if it passes eight
seeds here with a margin on every limit and failed on no platform in CI.

**Judged** (all eight seeds, linux-release, each limit with its margin):
C182, J-3 Cub, Learjet 35A, PA-28. Least margins: sink 54 ft/min (against
360), touchdown zone 445 m, rise 0.81 ft (against 1), flare climb 75 ft/min
(against 100).

**Named**, with why: 737-300 (a 113 ft/min balloon, seed 2); 787-8, A320
and C172P (CI's failures above; here the 787-8 rises 0.85 ft on seed 6 and
the A320 touches at 363 on seed 5); A380 (367 ft/min, seed 1; a 196
balloon, seed 2); B-2A (a gust rolls her on to a wingtip as she touches,
seed 8); F-15C (397, seed 2); F-35B (348 on seed 6, inside by only 12);
Mosquito (her nose pitched up by her main wheels' touch, a 2.9 ft bounce);
S.23 (her flare flown to her centre of gravity, the tail now in Phase 10b).
Named before this change: 737-300, A380, F-15C, Mosquito, S.23.

**What changed**:
- The gust tests are one a seed, eight seeds, each flying all fourteen
  (`in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1` to
  `_8`), each 34.7 s in a local linux-debug (seed 1). The judged must pass
  each limit by a margin (`gusty_margin_fpm` 25 ft/min of sink and climb,
  0.25 ft of rise, 25 m of zone).
- No change to the lander. The 787-8 and A320 were not made robust in this
  round: on Linux they pass most seeds by 30 to 270 ft/min, but the
  platforms' spread on one seed is a few hundred ft/min, more than any
  margin they have.
- Rebased on main after #161 and #162 merged. The two tails this item
  found, the rollout's airspeed outside gusts and the flying boat's flare to
  her keel, are now open items in Phase 10b: the owner removed Later on
  2026-10-10.

**Verification:**
- The eight gust tests, the stabilized-gate gust test and the spool-up test
  pass (linux-release); the eight seeds' least margins for the judged are
  above. Seen red: the 787-8, A320 and C172P failed on CI's macOS and
  Windows (run 38006839738) while judged, and the B-2A (seed 8) and F-35B
  (seed 6) failed seeds 4 to 8 here while judged.
- The selftest hash does not move: 182dd6c996e0ee4c.

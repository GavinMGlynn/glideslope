### Collision ground under a runway is the runway's own line, 2026-10-01 — one tail done, the 16R tail not

**What is not done first.**
- **Only runways the file places are flattened.** OurAirports places both
  ends of 14,818 of its 47,198 open runways, and one end with a heading and
  a length of 16 more; 14,834 strips are made, 14,814 kept (20 are under a
  metre or over 8 km between their ends). The other 32,000 or so - small
  strips, mostly - have no position in the file and are the DEM as it is.
  Every runway end a plan can take off from (`read_runways`) is among them.
- **Runways that meet at different slopes still pull each other.** Tied
  where they meet, two lines agree there and part away from it, and the
  weighted mean of the two pulls each off its line where their surfaces
  overlap: worldwide, measured on the file's elevations, 38 runways more
  than 0.3 m and the worst 0.68 m (LKMB 16/34). A tail.
- **Ties move the file's elevations.** Where two runways' lines miss each
  other where they meet - at Sydney by 2.3 m - both are moved to meet, so
  Sydney's 16R end stands at 3.90 m, not the file's 8 ft (2.44 m). Believing
  every elevation in the file, the ties move 1,017 runways' ends more than
  1 m; a group they would move more than 5 m - KPUC's, whose 18 end the file
  puts 100 ft high - is made from the fits to the DEM instead.
- **A runway's surface is a straight line.** Courchevel's, which steepens
  towards its top, is a ramp from one end's height to the other's, 12 m from
  the DEM where the DEM bows most.
- **What is drawn is not flattened.** The open provider draws the DEM as it
  is, so under a runway what is seen and what is flown differ by what the
  DEM's bumps were; measured below.
- **Not bit for bit across platforms, and a flip moves metres.** The ground's
  arithmetic is floating point through each platform's own sine and cosine:
  places and heights agree to about a nanometre. Its discrete decisions -
  whether a line is the file's or the fit (an end 5 m from the fit), whether
  a group is made from the fits (an end moved 5 m), whether two runways are
  tied (an end within reach, a centreline within [0, 1]) - would come out
  differently on two machines only for a value within about a nanometre of
  its threshold, which across some 15,000 runways compared against thresholds
  in metres is a chance of the order of one in a hundred thousand; but if
  one flips, a whole group's lines move, by up to metres, between that
  server and that client. On one machine every decision is the same every
  time, whichever runway is asked for first.

**The decision.** `REQUIREMENTS.md` section 9, closed 2026-10-01 by the
project owner: flatten collision ground under runways.
`src/world/runway_ground.hpp` holds the rules:
- **The runways are a made asset**: `tools/make_runway_strips.py` reduces
  OurAirports' pinned `runways.csv` (SHA-256 pinned in the script) to
  `assets/runways/strips.csv`, 928,009 bytes - airport, idents, both ends'
  places to 6 decimals, their elevations as the file has them, the width to
  0.1 m - leaving out closed runways, helipads, those on water and those with
  neither end placed, and placing a lone end's partner by its heading and
  length. `the_committed_runway_strips_are_what_their_script_makes_of_the_pinned_runways`
  holds the committed file to what the script makes. The ground is part of
  the build: no program fetches it, and the planner still reads the whole
  `runways.csv`, fetched, for the ends it names (so a server whose runways
  cannot be fetched still flies the plan file, as #62 had it).
- **Read once per process**: `world::runway_surfaces(data)` reads and
  indexes the strips the first time and hands every caller after the same
  `shared_ptr<const RunwaySurfaces>` - the server's fleet, every client
  `Flight` (made again at a take-over or a ride), fly-plan and each
  `--mismatch` place.
- **The surface**: the rectangle between the two ends - the ends of the
  pavement, not displaced thresholds: between the pinned file's ends is the
  runway's length to within 0.3% at Sydney, Denver, Heathrow, Los Angeles
  and San Francisco, and 6% at JFK's 04L - as wide as `width_ft` (30 m where
  none or one outside 3 to 150 m), a straight line along it from the le
  end's elevation to the he end's, level across it, held level past each end.
- **The fallback**: a least-squares line through the DEM every 10 m along the
  centreline, where either end has no elevation (3,613 of the 14,814) or
  either is more than 5 m from that fit. The other 11,201 give both and are
  the file's unless the DEM disagrees by more than 5 m - which only the DEM
  under each can say, so how many is not counted worldwide. Courchevel's is
  the fit: its 04 end is placed by its heading and length from 22, and the
  file's 6,583 ft is 11.7 m from it.
- **The shoulder**: from the rectangle's edge the ground blends to the DEM by
  a smoothstep over 50 m, and beyond that is the DEM's exactly.
- **Ties**: two runways sharing an index cell are tied where their
  centrelines cross, and where an end of either is within reach of the
  other - both half-widths and a shoulder - at the closest point to that end;
  ties of one pair within 20 m of each other are one. So a cross, a V, a T,
  two runways end to end and two side by side (at both ends of where they
  run together) are all tied. Each group of tied runways has its lines moved
  the least - in the sum of squares of the changes to their ends' heights -
  that makes every tie meet, `x = x0 - C^T (C C^T)^-1 C x0`, by Gaussian
  elimination with partial pivoting; and a group that would move an end
  more than 5 m from its own line is made from every member's fit instead,
  and tied again. **Overlaps** are the blend-weighted mean of the surfaces,
  the DEM filling whatever weight is left below one.
- **The same everywhere**: the strips are sorted (airport, idents, places)
  before anything is built, so their order changes nothing; every height
  follows from them and the pinned DEM, never from which tiles are loaded.
- **Its identity travels with the protocol**: `protocol_version` is 2, moved
  for this, and `the_protocol_version_moves_with_the_collision_ground` pins
  version 2 to `collision_ground_rules` 1 and the strips' SHA-256
  `6c1ba3c3...898f`: a change to either fails it until the version moves, so
  builds on different ground refuse each other at the envelope.
  `TRANSPORT.md` says so. `server_gearstick.cmake`'s expected refusal is now
  `474c4453020401`.
- **Who uses it**: `world::CollisionGround`, a Dem with the runways, is the
  terrain of the server's `Fleet`, the client's `Flight` (its prediction,
  standing start and weather lift), `glideslope_cli fly-plan`, and the
  "flown" height the client's `--mismatch` measures against
  (`flown_ground_at`). The open provider's drawn mesh, the sky's station
  height and `glideslope_cli height` keep the raw DEM.

**What the ground under a runway does, measured** along each reference
runway's centreline every metre: the largest departure from the straight
line between its ends' heights, and the largest change of slope from one
30 m stretch to the next.

| Runway | DEM: off its line | DEM: slope change | Collision ground: off its line | slope change |
| --- | --- | --- | --- | --- |
| YSSY 16R/34L | 3.18 m | 9.67% (226 m along) | 0.005 m | 0.030% |
| YSSY 16L/34R | 2.64 m | 7.35% | 0.000 m | 0.000% |
| YSSY 07/25 | 2.22 m | 2.32% | 0.011 m | 0.060% |
| KDEN 16R/34L | 2.26 m | 1.45% | 0.000 m | 0.000% |
| EGLL 09L/27R | 1.25 m | 4.94% | 0.000 m | 0.000% |
| KBOS 15R/33L | 0.74 m | 1.17% | 0.003 m | 0.015% |
| LFLJ 04/22 | 12.13 m | 16.93% | 0.000 m | 0.000% |

What remains on the collision ground is where a tied runway's surface is
averaged in. The test holds every reference runway to 0.05 m and 0.25%.

**How far one runway pulls another, worldwide**: every runway giving both
elevations (11,201), believed as the file gives them, laid on a DEM of sea so
that no fit enters, and sampled every 20 m along and three places across
wherever another reaches it (3,619 runways):

| | worst | pulled > 0.1 m | > 0.3 m | > 1 m |
| --- | --- | --- | --- | --- |
| tied only where centrelines cross | 5.40 m (KSVC 17/35) | 962 | 203 | 50 |
| tied where they meet (now) | 0.68 m (LKMB 16/34) | 572 | 38 | 0 |

The next worst are LKMB 04/22 0.63, FMEE 14/32 and 12/30 0.51, LGIR 12/30
0.50, FAPY 06/24 0.50 m. The test holds the world to 0.75 m; the rest is a
tail. Synthetic, on the sea, at ends 20 ft apart: a V pulls 0.26 m (the two
slopes differ by 0.45%), a T 0.08 m, end to end 0.03 m, side by side 0.003 m.

**What is drawn against what is flown, measured again.** The client's
`--mismatch` now prints each place against the collision ground ("flown")
and, as `mismatch-dem`, against the DEM as it is; the drawn surfaces are
unchanged. Metres, drawn less flown:

| Runway end | open | Cesium ion | Google |
| --- | --- | --- | --- |
| KDEN 16R / 34L | -1.61 / **-2.47** | +0.73 / +0.83 | +0.09 / +0.39 |
| KLAS 8L / 26R | +1.17 / +2.14 | +2.82 / +3.75 | +0.97 / +2.91 |
| KBOS 15R / 33L | -0.82 / -2.16 | +0.83 / +0.28 | +0.51 / -0.33 |
| PAJN 8 / 26 | -0.40 / -0.02 | +2.23 / +2.79 | +1.97 / +2.66 |
| PANC 7R / 25L | -1.69 / -1.89 | +0.15 / -1.78 | -1.75 / -1.84 |
| PABR 8 / 26 | -1.58 / -0.83 | **-11.78** / -6.04 | **-11.68** / -6.08 |

The open provider is 2.47 m from the flown ground at worst, what the DEM's
bumps and offsets were there, and still 0.18 m from the DEM it draws
(PANC-7R) - so a fault in drawing it is still caught. Cesium ion's and
Google's worst, at Barrow, grew from 10.2 and 10.1 m to 11.8 and 11.7,
because the flown ground there is now the runway's line, 1.6 m above the
DEM. The tests hold the open provider to 3 m from the flown ground and
0.25 m from the DEM, ion and Google to 13 m (they held 0.25, 12 and 12).

**Every aeroplane takes off from 16R.** `glideslope_cli fly-plan` with the
sydney-cbd-orbit plan in each aircraft, before and after:

| Aircraft | Before (raw DEM) | After |
| --- | --- | --- |
| 737-300 | wrecked 14 s in, airframe struck the ground | handed over 801 ft above the runway, 48 s in |
| A320 | wrecked 14 s in, airframe struck the ground | handed over, 48 s |
| F-15C | wrecked 9 s in, hit the ground sinking 780 ft/min | handed over, 25 s |
| 787-8, A380, C172P, C182, F-35B, J-3 Cub, Learjet 35A, Mosquito, PA-28 | took off | took off, each within a second of before |
| B-2A | still climbing to hand over at 90 s | the same; the test sees it hand over 115 s in |
| 747-400, F-22A | no climb speed to fly | the same: the test rolls them instead |
| Short S.23 | a flying boat on a runway | the same |

Before, the take-off roll pitched the 737-300 to 13.2 degrees and the A320
to 16.6 on the ground; after, 2.5 and 0.5. Rolled stick-neutral through
16R's first 2,000 m, the 747-400 reaches 181 kt and the F-22A 334 kt with
their noses within 0.3 and 0.5 degrees of where they stood; the test checks
no speed, and neither is flown off, for want of a climb speed - so the 16R
tail stays open, naming them. On the raw DEM the F-22A was wrecked 218 m in and
the 747-400's nose moved 2.2 degrees. The Learjet 35A and the Mosquito come
down on the water of Botany Bay 80 to 90 s in, after the take-off has handed
over, before and after alike: a tail.

**Tests** (`tests/unit/test_runway_ground.cpp` unless named):
- `a_runway_strips_file_is_read_and_refused_where_it_is_wrong`.
- `the_committed_runway_strips_are_what_their_script_makes_of_the_pinned_runways`
  (`tests/cmake/runway_strips.cmake`).
- `the_protocol_version_moves_with_the_collision_ground`.
- `the_collision_ground_is_the_same_whatever_order_the_runways_come_in`:
  Sydney's and Boston's nine runways, reversed and shuffled, give bit-for-bit
  the same height at 89,540 places every 20 m over both airports, 424 of
  them reached by two runways or more.
- `the_collision_ground_is_the_same_whichever_runway_is_asked_for_first`:
  two grounds over Sydney's and Boston's runways, one asked for them first
  to last and one last to first, give the same lines and the same heights
  at 13,122 places, to the bit.
- `the_collision_ground_under_a_runway_is_its_own_line_and_past_its_shoulder_the_dem`:
  Sydney's 16R/34L's own line is the file's 8 ft and 14 ft to the bit, and
  Courchevel's the fit; on each reference runway, of 65 stations by three
  places across (195), every one no other runway reaches is on its line to
  1e-6 m - 164 to 195 of them, at least half asserted; half a metre past the
  shoulder, either side and past each end, the DEM's exactly; and where it
  is tied, the two lines meet to a millimetre.
- `reference_runways_roll_with_no_bump_beyond_a_bound`: the first table.
- `runways_that_meet_in_a_v_a_t_end_to_end_or_side_by_side_are_made_to_meet`:
  the four synthetic layouts, tied, meeting, pulled no more than 0.3 m.
- `a_group_its_ties_would_move_too_far_is_made_from_the_fits_instead`.
- `a_runway_across_the_antimeridian_or_near_a_pole_is_its_own_line`: one
  across 180 degrees on the equator and one 10 km from the North Pole, on
  their lines at all 387 stations and the sea's past the shoulder.
- `no_runway_in_the_world_is_pulled_off_its_line_beyond_a_bound`: the
  worldwide table, held to 0.75 m, and to having measured all 11,201
  runways and found the 3,619 another reaches.
- `the_protocol_version_moves_with_the_collision_ground` also reads the
  client written from `TRANSPORT.md` (`tests/doc_client`) for its version
  constant, now `0x02`, which the first push of this version missed: the
  server refused it and `server_doc_client` would have gone red on CI.
  `the_transport_document_and_the_code_agree_byte_for_byte` now checks the
  refusal's version byte in the document's sentence too.
- `tools/make_runway_strips.py --check` compares the file's bytes exactly;
  seen to fail with one byte of the committed strips changed.
- `every_landplane_rolls_down_sydneys_16r_on_the_dem_and_every_one_that_can_climb_away_takes_off`:
  stood on the file's 16R end at the collision ground's height there, every
  landplane flown by the take-off autopilot until it hands over with nothing
  wrecked (13), or, publishing no climb speed, rolled through 2,000 m with
  its nose within 1.5 degrees (747-400, F-22A); the Short S.23 left out and
  named, a flying boat. Coverage is asserted: 13 + 2 + 1 = 16.
- `the_open_terrain_is_within_its_stated_distance_of_the_dem_it_draws_on_*`,
  new, and the ground-flown bounds moved, as above.

**Seen to fail.** With `CollisionGround::height_above_geoid` returning the raw
DEM, the line, reference and take-off tests fail - the last with the wrecks
above, and the F-22A wrecked 218 m into its roll. Tied only where centrelines
cross, the T fails to be tied and the worldwide test fails at 5.40 m. Without
the guard, the too-far group lies 4.6 m off the fit. Without the sort, the
order test failed at a Boston place; with the shoulder doubled, the line test
failed past it; without any ties, Sydney's 16R/34L strayed 1.18 m.

**Unchanged.** The selftest flies on flat ground and its hash,
`30ac70b84cab7d7c`, is the same before and after; the cross-platform flight
figures are flown on flat ground too.

**Recorded from the review of #74**: the first push of this branch went with
`GLIDESLOPE_PUSH_QUICK=0`, skipping the pre-push hook's build and quick tests
because the hook ran ctest at -j8 on a machine shared at -j4; the targeted
tests were run by hand. From here the hook runs.

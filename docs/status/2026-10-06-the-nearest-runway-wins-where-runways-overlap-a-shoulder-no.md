### The nearest runway wins where runways overlap: a shoulder no longer pulls a neighbour's pavement; 16R closed, 2026-10-06 — one tail done, one still open

**What is not done first.** "Runways that meet at different slopes still
pull each other's surface" stays `[ ]`: its verification asks that no
runway in the world be pulled more than 0.1 m, and 262 still are. What
remains is where two runways' **pavements** overlap - a crossing, or the
first metres of a V. Each runway's surface is level across and straight
along, and two such surfaces sloping differently agree along one line at
most, never over an area; the ground there is their mean, which pulls each
by half their difference, and no choice of one height for a place can pull
the worse of the two less. Meeting 0.1 m needs a different rule inside the
overlap - a surface that is not level across one of them, or ties that
level the two's slopes towards each other, moving their ends by metres -
and that is the owner's choice, not made here. A smaller step that would
help the V's: tie two runways at the middle of where their pavements
overlap, not at the end that reaches the other; not tried.

**The rule, rules 2.** `CollisionGround::height_above_geoid` weighted every
runway reaching a place by its blend alone, so inside one runway's rectangle
another's shoulder - weight 0.2 at 35 m from its edge - still took a fifth
of the height. Now each runway's weight is also multiplied by a smoothstep
falling from 1 to 0 as the place is from 0 to `runway_overlap_band_m` (5 m)
further outside its rectangle than it is outside the nearest runway's
(`RunwaySurfaces::Placed::outside_m`). On a pavement, then, another runway
weighs nothing unless the place is within 5 m of that one's own edge; where
two pavements overlap both are at distance 0 and the mean is as before. The
ground is still continuous: every weight is continuous in the place.
`collision_ground_rules` is 2.

**The protocol moved with it**: version 4 as first pushed, 5 since #107 took 4 (2026-10-07, above). `the_protocol_version_moves_with_the_collision_ground`
pins version 4 to rules 2 and the strips' unchanged SHA-256; the client
written from `TRANSPORT.md` says `0x04`; `server_gearstick.cmake`'s
expected refusal is `474c4453040401`; `TRANSPORT.md` names version `04`
and why; `the_collision_ground_said_is_the_builds_coverage_strips_and_rules`
hashes "ground rules 2".

**Worldwide, as measured before** (`no_runway_in_the_world_is_pulled_off_its_line_beyond_a_bound`,
11,201 runways, 3,619 reached by another):

| | worst | pulled > 0.1 m | > 0.3 m | > 1 m |
| --- | --- | --- | --- | --- |
| rules 1 | 0.68 m (LKMB 16/34) | 572 | 38 | 0 |
| rules 2 | 0.61 m (LKMB 04/22) | 262 | 12 | 0 |

The next worst: LKMB 16/34 0.59, LFQB 05/23 0.44, FMEE 14/32 0.43, LGIR
12/30 0.43, FMEE 12/30 0.43 m. The synthetic V is pulled 0.20 m (was 0.26),
the T 0.05 (0.08); end to end and side by side 0.000.

**Tests.**
- `a_runways_shoulder_does_not_pull_the_pavement_of_another_beside_it`
  (new): two runways on the sea parting at 5 degrees from 80 m apart,
  sloping opposite ways, tied where they meet; at every metre along each and
  three places across, at the 152 and 156 places the other's shoulder
  reaches, each is its own line to 1e-6 m. **Seen to fail** with the band
  made 5e9 m - rules 1, in effect: "A: pulled 0.027283 m off its line".
- `no_runway_in_the_world_is_pulled_off_its_line_beyond_a_bound` now holds
  the worst to 0.62 m (was 0.75), and at most 12 runways over 0.3 m and 270
  over 0.1 m. **Seen to fail** the same way: "pulled more than 0.3 m: 38,
  more than 0.1 m: 572".
- Run with these and green: every test in `test_runway_ground.cpp` - the
  line, reference-runway, order, V/T/end-to-end/side-by-side and too-far
  tests, and all five 16R groups - the four `TRANSPORT.md` agreement tests,
  the gearstick refusal, the client written from the document, the
  open terrain's two mismatch tests, `a_client_told_the_servers_collision_ground_refuses_other_ground_and_leaves`,
  `a_plan_that_takes_off_and_orbits_the_cbd_is_flown_over_the_dem`,
  `two_planned_aircraft_wrecked_together_on_one_runway_fly_again_one_after_the_other`
  and `every_aircraft_the_data_holds_takes_off_from_a_runway`.

**16R closed.** "On the DEM at Sydney's 16R the runway is not flat enough
to take off from" is ticked: since the 747-400 and F-22A were given take-off
speeds (2026-10-06, above), the five 16R tests take all fifteen landplanes
off 16R on the DEM and fly each to the CBD orbit with nothing wrecked - none
is only rolled - and what the collision ground under a runway may do is
measured by `reference_runways_roll_with_no_bump_beyond_a_bound` (2026-10-01,
below). Both run again on rules 2, green; the reference runways are within
their 0.05 m and 0.25% bounds.

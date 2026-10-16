### The simulation works out the ground under a place once, and finds a property it has not got once, 2026-10-10 — item still open

**What is still missing, first.** "One test takes 15-22 minutes in Linux
debug" stays open: its verification is a time on CI, and no CI run has
had this yet. On this machine the AI separation test went from 393 s to
99 s in linux-debug, which at CI's ratio (882 s there to 393 s here) is
about 220 s, inside the 10 minutes. The jets' orbit-entry tests of PR #154
gain less in debug (25%, against 40% in release): their time is JSBSim's
own step, not the ground (below). Not built on Windows or macOS; nothing
here is platform code.

**The profile** (eu-stack samples of the running process, this machine).
- *The AI separation test's server*, linux-debug, 10,000 steps: 14 of 26
  samples in `FGGroundCallback::GetAGLevel` → `FunctionTerrain::height_m`
  → `CollisionGround::height_above_ellipsoid` → `Dem::height_above_geoid`;
  12 of the 26 came through `Aircraft::state` reading
  `position/h-agl-ft` and `position/terrain-elevation-asl-ft`, whose
  getters ask JSBSim's ground about the aircraft's location again. In
  release, 16 of 25. Under the DEM's lookups, a share in `std::list::remove`
  and in ASan's allocator: the tile and block caches' least-recently-used
  orders searched and reallocated a node at every one of a query's dozen
  sample reads.
- *The F-15C's orbit entry* (PR #154's branch), linux-debug: no terrain at
  all (JSBSim's own ground). 36 of 60 samples in `FGFDMExec::Run`
  (aerodynamics, flight controls, the gear's forces); 10 in
  `Aircraft::has_property` walking the property path through
  `SGPropertyNode::getNode` for properties the model has not got -
  speedbrakes, spoilers, cooling flaps, a supercharger, asked after by
  every `set_controls`. A found node was kept already; an absent one was
  looked for afresh every time.

**What changed** (src/sim, src/world; nothing that decides where the
ground is).
- **TerrainGround keeps what it found at a place** (`sim/aircraft.cpp`):
  the contact point, normal and height above it, for the last places asked
  about (64 slots, picked by the place's bits), each matched on its exact
  Earth-centred coordinates, bit for bit. Nothing near a place stands for
  it, so the ground is exactly the terrain's; the answer is the one working
  it out again gives, because the terrain is a function of position - now
  written into `sim/terrain.hpp`'s contract: ground that changes is a new
  Terrain, given with `set_terrain` (which makes a new TerrainGround).
- **The DEM's caches move a used tile or block to the front by a splice**
  (`world/dem.cpp`): each keeps its place in its order, so nothing is
  searched, allocated or freed on a hit. Which tile or block is evicted, and
  when, is as before.
- **A property found absent is not looked for again until it can have
  come** (`sim/property_nodes.hpp`, `PropertyNodes`, now holding the
  Aircraft's found nodes too): kept with the deepest node of its path that
  is there and that node's child count. Nothing removes a node from
  JSBSim's tree, so the property can only come as a new child of that node;
  while the count is the same it is not there.

**Measured** (alone on the machine, one test at a time through ctest; the
orbit-entry tests on PR #154's branch with this change copied in):

| test | release before → after | linux-debug before → after |
|---|---|---|
| AI aircraft kept 500 ft or 1.5 nm apart (108,000 steps) | 45.8 → 17.4 s | 392.7 → 98.9 s |
| the F-15C enters an orbit from a waypoint | 52.3 → 30.7 s | 355.9 → 266.3 s |
| the 737-300 enters an orbit from a waypoint | 48.5 → 33.9 s | 311.1 → 240.7 s |
| going around beside rising ground, then landing | 32.2 → 15.5 s | 147.9 → 113.7 s |
| the 737-300's glide band, left, 222-242 kt | 3.0 → 1.65 s | 16.9 → 15.2 s |
| the selftest | 0.82 → 0.46 s | 4.7 → 3.9 s |

**Nothing flown changed.** Every line those six tests print, before and
after, is the same, in both builds (their logs diffed, ctest's own lines
and the fixture's download lines left out). The selftest hash did not move:
182dd6c996e0ee4c in linux-release, 6e988ba216e5ee07 in linux-debug, before
and after - though the selftest flies on JSBSim's own ground, so it does
not reach TerrainGround; the separation and rising-ground tests, which fly
over the DEM and the runways, are the evidence for that part.

**Tests** (each seen to fail on its deliberate bug, the bug then reverted):
- `the_ground_under_an_aircraft_is_worked_out_once_for_a_place_however_often_it_is_read`
  - a hundred reads of the state ask the terrain nothing, and read the same
  figures. Red with every place worked out afresh: 1,000 asks.
- `the_ground_found_at_a_place_is_the_terrains_there_however_near_a_place_found_before`
  - a 300 m cliff along a parallel, the aircraft put 0.2 mm south of it and
  then north: the foot, then the top. Red with one slot matched within
  30 ft, a cache "within the DEM's resolution": the top read as the foot.
- `a_property_a_model_has_not_got_is_found_once_something_makes_it` - a new
  child of a node that is there, a path none of which is there, and an index
  beside one that is: each absent twice, then made and found. Red without
  the child count: a made property still not found.

Run besides, linux-release, `ctest -j4`: every test whose name has land,
orbit, glide, apart, separation, terrain, ground, selftest, dem, propert
or feather in it, but not the window client's or the package's (not
built here) - 263 tests, 247 passed and 16 skipped (the ones that ask a
language model now, with no key here), none failed, 332 s. The three new
tests pass in linux-debug, under the sanitizers.

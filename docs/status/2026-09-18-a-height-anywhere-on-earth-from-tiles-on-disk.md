### A height anywhere on Earth, from tiles on disk, 2026-09-18

**What is missing first:** the tiles must already be on disk - nothing fetches
them while the program runs - and no height has yet been compared with a
surveyed one. Nothing in the simulation or the renderer asks for a height yet.

**`world::Dem`** gives the DEM's height above the geoid, or above the WGS84
ellipsoid with the geoid added, at any latitude and longitude. Heights between
samples are bilinear from the four around them. Near a tile's south or east
edge those come from the next tile, and when that tile's samples are spaced
differently - north of 50 degrees, or a 90 m tile beside 30 m ones - its row or
column is interpolated to the point needed. The sea, which has no tiles, is at
zero. Positions on the grid are whole half-arc-seconds, in which every spacing
either dataset uses is a whole number, so tiles meet exactly. A tile that does
not fill its cell sample for sample is refused. Up to 16 tiles and 64 decoded
blocks are kept.

**Which tiles exist** is `assets/dem/coverage.txt`, 66 KB: for each 1-degree
cell, a 30 m tile, a 90 m tile only (25 cells around Armenia and Azerbaijan,
which the public 30 m set leaves out), or the sea. `tools/make_dem_coverage.py`
makes it from both buckets' tile lists, pinned by SHA-256 and fetched by the
tests, and a test checks the committed file is still what they make.

**The tests.** The coverage has 26,450 cells at 30 m, 25 at 90 m only and the
rest sea, and names Sydney's, the Tasman Sea's and Armenia's rightly. Tile
names and URLs follow the buckets'. On synthetic tiles built in the test, a
plane - which bilinear interpolation reproduces exactly - comes back to within
a millimetre at over a thousand places across 25 tiles with two longitude
spacings and one coarser tile among them, and exactly on their corners; a
height that depends on latitude comes back across the antimeridian from both
sides and from 540 degrees; both poles and past them; the coast falls to zero
halfway to the sea east and south, opening only the land tile; a tile for the
wrong cell is refused. On the real Sydney tile, the height at seven of the
independent decoder's samples, and halfway between two samples in different
internal tiles, comes back within a millimetre, and a point in the Tasman Sea
is at zero.

**Watched to fail:** edge samples placed with the latitude spacing rather than
the longitude spacing (the plane was 0.11 m out north of 50 degrees). The first
run also found a real bug: a latitude exactly on a whole degree was placed in
the tile to its north, whose southern edge is not its own, so an edge sample
asked for itself again until the depth guard stopped it, and a query at 33 S
opened the tile for 33-32 S that it did not need. A whole-degree latitude is
now the northern row of the tile below.

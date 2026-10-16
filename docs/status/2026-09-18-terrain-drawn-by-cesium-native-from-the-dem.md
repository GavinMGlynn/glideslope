### Terrain, drawn by Cesium Native from the DEM, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** the terrain is a region, not the world - the nine
whole-degree cells around where the flight starts, or the one cell the terrain
screen's eye is in - so a flight that leaves it flies over nothing. There is no
imagery: the ground is tinted by height and lit by a fixed sun. Tiles are made
from the DEM one at a time, behind one lock, so a region loads slowly the first
time, fetching each 30 m tile it needs (25 MB each). The HUD's horizon line does
not line up with the drawn horizon (see Gaps). Nothing from Cesium ion or
Google yet, and no measured visual-to-collision mismatch: that is Phase 5b.

**Cesium Native is built and linked** (`ext/cesium-native`, v0.64.0), with its
thirty dependencies from vcpkg, pinned to the commit Cesium Native's release is
built against and fetched outside the tree (`cmake/Vcpkg.cmake`,
`cmake/triplets/`, `vcpkg.json`; `ext/README.md` says how and why). They are
installed after the platform gate accepts the compiler. The first configure of
a machine builds them - 20 minutes here - and vcpkg's binary cache keeps them;
CI keeps that cache. Every package carries their licences.

**The open-data terrain is a Cesium Native tileset** whose loader builds its
tiles from the DEM (`gfx/terrain_tiles.hpp`): a quadtree over the region, each
tile a 32-by-32 grid of cells with skirts (`world/terrain_mesh.hpp`), down to
the DEM's own spacing - level 7 over one degree - and each tile's geometric
error half its cell size. Cesium Native chooses the tiles a view needs, loads
them on worker threads and caches them; the glue turns each tile's glTF into
the renderer's meshes, uploads them on the main thread and frees them when
Cesium Native drops the tile. The renderer can now remove a mesh. A `--shot`
waits for every tile its view needs, so the same command draws the same frame.
Requests Cesium Native makes go through the platform's HTTPS - plain GETs only,
for now; the DEM loader makes none.

**Shown where it is drawn.** The client draws the Copernicus DEM's notice along
the bottom of every frame with terrain in it, in the small type credits now use,
and `glideslope_cli height` prints it; `README.md`, which ships, carries the
liability sentence the licence asks for (`ASSETS.md`).

**The terrain screen**, `--screen terrain --at LAT,LON,HEIGHT --toward
LAT,LON,HEIGHT`, draws the terrain alone from a point toward another. The
flight draws the terrain under the HUD.

**The tests.**
- A terrain mesh over ground with a known slope puts every vertex on it within
  a centimetre, each normal perpendicular to it within half a degree, every
  triangle facing up, the skirt 50 m straight down; it meets the tile beside it,
  and a tile of the next level, within 2 mm at every shared vertex; and a
  rectangle off the Earth, or with no cells, is refused.
- **Mount Taranaki, against the DEM itself**
  (`a_shot_of_mount_taranaki_matches_the_dem_ray_cast_from_the_same_eye_on_<driver>`):
  the terrain screen shoots the mountain from 9 km east of its summit, and
  `glideslope_terrain_check` makes the reference frame without Cesium Native,
  tiles, meshes or GPU - a ray through every pixel, marched through the DEM
  until it meets the ground, coloured as the terrain is coloured - and holds
  the two together above the notice, which it reads back first. As drawn: the
  skyline 0.12 pixels out on average and 1 at most; 99.94% of pixels ground or
  sky alike; the colour 4.76 of 255 out on average, 23 at the 95th percentile.
  The tolerances are 0.25 and 2 pixels, 99.5%, 5.5 and 28.
- The HUD test now flies over the drawn terrain and reads both credits, the
  DEM's and Open-Meteo's, back out of its frame.

**Watched to fail**, each against the tolerances: only coarse tiles drawn
(screen-space error 200: the skyline 0.48 pixels out, the colour 7.80); tiles a
level or so too coarse (error 16: 0.30, 6.95); the terrain 20 m too high (0.34,
6.13); 100 m too high (2.10 pixels, 98.90% agreeing, 8.08); a vector leaked in
the client (the leak judged glideslope's). The first tolerances, set before any
of this, passed all four breakages, and were tightened on these numbers.

**Also:** LeakSanitizer gives Mesa's lavapipe driver, unloaded before it
reports, as the client itself, at offsets past the end of its image and with
no function named; a frame given as the client's with no function named is now
judged not to be the client's (`tests/cmake/client.cmake`). Under WSL, CMake's
package search walked Windows' PATH through `/mnt/c` for minutes; Cesium
Native's packages are now found through vcpkg's prefix alone. A sanitized build
at ninja's default of every core ran WSL out of memory; builds here are capped
(`CLAUDE.md`).

# Project status

**What this file is.** The single source of truth for what actually works, and
the place detail belongs. `FEATURES.md` says what the simulator should be;
`COMPLETION_PLAN.md` says what order it gets built in and how each item is
verified; this file says what is true *today*, with the gaps named first.

**Never describe a partial module as working.** "Working" means 100% of what it
claims. Anything less is reported with the missing part named *first* — "terrain
streams and draws; nothing collides with it yet", never "terrain works".

**Words used here.** *Gearstick* is the author's earlier project, a racing game,
whose build, documents and discipline this one follows. *The brief* is the
design brief that became `REQUIREMENTS.md`. *Presentation* means anything that
draws, opens a window, reads an input device or plays sound. A *tail* is work
found along the way and added to the bottom of `COMPLETION_PLAN.md`. *Locally*
means the development machine: Rocky Linux 10 under WSL, with GCC 14.3.1;
anything proved elsewhere names the CI run.

---

## The honest summary, 2026-09-18

**A Cessna 172P flies to its handbook, with nobody at the controls but a test
pilot, and the ground's height is known anywhere on Earth - but nothing yet
joins the two, and there is no terrain to see.** JSBSim is built and linked and
steps at a fixed 120 Hz, and glideslope's Cessna 172P lands inside its tolerance
on all nine published-figure checks; its state can be captured and restored;
`glideslope_cli selftest` flies a fixed five-minute log and every package flies
it; the five release builds fly every check to the same numbers.
`glideslope_cli height LAT LON` gives the Copernicus DEM's height above sea level
and above the WGS84 ellipsoid anywhere, downloading the tiles and the geoid it
needs, and every package does so at Sydney airport. The renderer draws built-in
test scenes, with reversed depth and a camera-relative floating origin, on
Vulkan, Direct3D 12 and Metal - but only test scenes. There is no terrain drawn,
no input, no HUD and no server.

**Phase 0 is complete — 7 of 7 items.** What exists is the ground everything
else is built on, one line per item, each verified:

1. a C++20 build with presets for Linux, macOS and Windows (MSVC and clang-cl);
2. a configure that refuses 32-bit toolchains and compilers below their floors;
3. warnings that are errors in every build type, on every compiler;
4. configure-time checks that the simulation includes and links no
   presentation, and a test that reads the CLI binary's real dependencies;
5. CI in which every preset configures, builds and passes its tests on its own
   platform;
6. a package per platform that runs from wherever it is unpacked;
7. these documents.

Every check above was also made to fail on purpose, and was seen to.

**Phase 1, the feel, is complete — 7 of 7 items.** JSBSim pinned and built; a
fixed 120 Hz step; the Cessna 172P flying to its handbook; state capture and
set/resume; the selftest's replay hash; the same flights on every platform; and
a packaged CLI that flies — each proved on every platform.

**Phase 2, the world, is under way: 6 of 12 items done**, each proved in CI on
every platform (run 35241851702): Earth-centred, Earth-fixed positions; a
camera-relative floating origin; reversed depth; a window and a GPU device on
Vulkan (Linux, and Windows through lavapipe), Direct3D 12 and Metal; the
Copernicus DEM, read directly, with a height query anywhere held to surveyed
runway ends and coastlines; and collision terrain the Cessna rests on. Done on
Linux and awaiting CI: flight controllers, the HUD, the client's test flags,
and frames from CI and every package. Not started: Cesium Native drawing the
terrain, and imagery on it.

## Gaps

Everything in `COMPLETION_PLAN.md`. The ones worth naming first, because they
are the risks the phase order is built around:

- **Restore settles what it cannot read.** It holds the aircraft at the captured
  state for two simulated seconds so JSBSim's hidden engine and actuator states
  converge; a restore is therefore not free, and whether that cost suits
  reconciliation many times a second is a question for Phase 6.
- **The checks are one aircraft's.** Every figure is the Cessna 172P's; other
  types arrive in Phase 5.
- **No terrain to see or touch.** The DEM gives heights, but nothing draws
  terrain (the Cesium-to-SDL_GPU glue does not exist) and nothing in the
  simulation stands on it.
- **Summits are low in the DEM.** A 30 m grid does not hold a peak: at five
  surveyed summits the DEM is 8 to 35 m below the survey. Runway ends and
  coastlines are within the dataset's stated 4 m.
- **The DEM is not thread-safe.** One `world::Dem` caches tiles and blocks as it
  goes; whoever shares one between threads must lock it.

---

## Log, newest first

### The flight screen: HUD, test flags, flight controllers and frames, 2026-09-18 — awaiting CI

**What is missing first:** there is still nothing to see but sky: no terrain,
no aircraft, no cockpit. The HUD is text and a horizon line. There is no
gamepad binding, no way to rebind a device but editing the file, and no
settings screen - `--screen` chooses between the flight and three test scenes.

**The flight screen** (`src/frontend/client/flight.hpp`), the client's default:
the Cessna standing on the DEM - tiles and geoid downloaded into the cache -
started over Sydney at 1,000 m, 100 kt, stepped at 120 Hz, seen from the
cockpit through a camera built from its position and attitude.

**The HUD** (`gfx/hud.hpp`): airspeed, altitude, heading, vertical speed,
pitch and bank as text, and a horizon line that pitches and banks. Text is a
5-by-7 pixel font drawn here, each font pixel a square of whole screen pixels,
over everything through a second pipeline with no depth test - so a frame can be
read back exactly (`gfx::read_text`).

**The test flags.** `--screen NAME`; `--shot FILE` and `--shot-at TICK`, which
now counts simulation ticks, with every frame advancing exactly two while
shooting whatever the clock says; `--trace`, a line of the flight's state after
every tick. Each is used by a ctest: the frame tests choose their scenes with
`--screen` and shoot at ticks, and `the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_<driver>`
flies 600 ticks with `--trace`, shoots the last, and has
`glideslope_hud_check` read every HUD line back out of the BMP and hold each
number to the traced state, within half its last digit, having checked the
trace holds ticks 1 to 600 in order.

**Flight controllers** (`platform/input.hpp`, library `glideslope_input`, apart
from `glideslope_platform` so that the CLI still links no SDL). `Joysticks`
reads every device SDL sees; `ControlMapper` turns them into controls by the
bindings in `assets/input/bindings.txt` - per kind of device SDL reports,
flight sticks and yokes or HOTAS throttles, each axis centred or a lever, each
button held or stepping, each hat direction stepping. An axis sets its control
when it moves, so a lever left alone does not undo a button. Pitch trim is a
new control. The keyboard still works beside them.

**Tested through SDL's virtual devices.** A virtual yoke and a virtual HOTAS
throttle, eight axes, sixteen buttons and a hat each: every one of the 56 inputs
alone moves a control, and none is unbound; a particular set of inputs sets the
controls they name to the values they should - stick forward is nose down, a
lever three quarters forward is 0.75, two presses are two notches of flap, the
brake held and let go - and those controls reach JSBSim's commands with the
signs JSBSim uses; a bindings file that cannot be read is refused by line.

**Frames from everywhere.** Packages now carry the client and SDL's licence.
Every package job runs the unpacked client headless on its platform's driver -
Vulkan through Mesa's lavapipe, installed in the stock Linux containers; Metal;
Direct3D 12 - flying 600 ticks of the flight, and uploads the frame. CI uploads
every frame its tests wrote.

**Watched to fail:** the HUD's airspeed shown one knot high ("SPD shows 79 at
tick 600 when the state is 77.896"); a binding removed from the file (the yoke
had an unbound input). The first run of the controller tests found that an axis
rewrote its control every read and so undid button steps to the same control -
flaps stayed at 0 - which is why axes now act only when they move.

### Standing on the DEM, 2026-09-18 — item done

**What is missing first:** only the tests connect the simulation to the DEM;
no program flies over it yet, and the selftest and the published-figure checks
still stand on JSBSim's own level ground. Nothing has flown into terrain: this
is ground contact for wheels, and whatever JSBSim does with the rest of the
airframe.

**`sim::Terrain`** (`sim/terrain.hpp`) is the ground the simulation stands on:
heights above the WGS84 ellipsoid at a latitude and longitude, from anything
- the frontends will give it the DEM. `Aircraft::set_terrain` puts a JSBSim
ground callback in place of JSBSim's level ground: the contact point straight
below, at the terrain's height, and the surface normal from the slope by
central differences 15 m to each side. `AircraftState` now reports height
above the ground and the terrain's elevation.

**A real bug, found by it.** `InitialConditions` latitude was given to JSBSim's
geocentric setter, while every latitude here - and the state JSBSim reports -
is geodetic. At 45 degrees that started an aircraft 0.19 degrees, 21 km, north
of where it was asked; at Sydney, where the figures and the selftest start,
about 20 km. It is now geodetic. The nine figures stay in range; the selftest
ends somewhere slightly different, and its hash is different.

**The tests.**
- Level terrain through the callback, at sea level and at 1,656 m, holds the
  Cessna exactly as JSBSim's own ground does: the same height, attitude and
  strut compression to within a millionth.
- On a 10% slope, facing up it and down it, the Cessna rests on all three
  wheels, its centre of gravity 4.39 ft above the slope as 4.36 ft above level
  ground, pitched by the slope's angle plus what its struts add - worked out
  from their measured compressions over the wheelbase - to within 0.1 degree.
- Engine stopped and brakes off, it rolls down that slope at over 5 kt in ten
  seconds, and stays still on level ground.
- On the real DEM, through the downloaded tiles and the geoid: at Boston's
  runway 33L (4.8 m), Denver's 34L (1,624 m), and halfway up Courchevel's
  runway, the Cessna comes to rest on all three wheels; the ground JSBSim
  stands it on is the DEM's height where it stopped, to 0.01 ft; its centre of
  gravity is 4 to 5 ft above it; and at Courchevel it is tilted by the DEM's
  own slope under its wheels, within 0.5 degree.

**Setting down.** JSBSim's initial altitude places the centre of gravity:
set down at the ground's height, the wheels start four feet under it and the
struts throw the aircraft up. On level ground it settles; facing down a slope
it went end over end for half a minute. The tests set it down 4.4 ft up.

**Watched to fail:** the normal set straight up (the rolling test: 0.000 kt
down the slope - the slope and rest tests passed, which is why the rolling test
exists).

**Also:** CI said MSVC refuses `getenv` in the tests (now one helper in the
harness), and CMake on macOS needs Objective-C++ enabled at the top of the
project before any target; CI's Windows runner now finds lavapipe
(`vulkaninfo`: llvmpipe, Vulkan 1.4.354) through the registry.

### The DEM, fetched as needed and held to the survey, 2026-09-18 — item done

**What is missing first:** summits. At five NGS summit stations the DEM is 8 to
35 m below the surveyed height, and even its highest sample within 90 m of each
is 5 to 21 m below: a 30 m grid of a radar surface does not hold a peak. The
plan's verification asked for summits within the stated accuracy; that is not
something a 30 m DEM can meet, and the verification is amended to say so
(`COMPLETION_PLAN.md`), with the shortfall pinned by a test. Also missing:
nothing in the simulation uses a height yet (the next item), and the DEM is not
thread-safe.

**Fetching** (`world/download.hpp`). `DownloadedTiles` keeps tiles in the cache
directory, fetching a missing one from the public bucket, checking it against
the MD5 the bucket gives as its ETag, and writing it into place only whole; a
tile that arrives damaged, missing, untagged or not at all is refused with the
reason and not kept. `fetch_pinned` does the same for a file pinned by SHA-256,
and `egm2008_geoid` uses it for the geoid grid. `world/digest.hpp` is SHA-256
and MD5, tested against FIPS 180-2's and RFC 1321's vectors and around every
padding boundary, whole and in pieces. `platform::cache_directory()` is
`GLIDESLOPE_CACHE`, or the user's cache directory on each system.

**HTTPS** (`platform/http.hpp`), through what each system trusts: WinHTTP,
NSURLSession, and on Linux the system's libcurl, loaded at run time, so nothing
is needed to build or to start and a missing libcurl is named when a download
is wanted. Recorded in `REQUIREMENTS.md`. Tested against the network: a pinned
1.1 MB file byte for byte, with its ETag equal to its MD5; GitHub's redirect to
the licence file at the commit that added it; a 404 as a response rather than an
error; a body over its limit, and a host that cannot exist, as errors.

**`glideslope_cli height LAT LON`** gives the height above sea level, the geoid,
and the height above the ellipsoid, fetching what it needs. Every package now
runs it at Sydney airport from an empty cache and must get 6.234 m: on stock
Ubuntu, which has no libcurl, it must first fail saying so, and then succeed
once libcurl is installed.

**Held to the survey** (`tests/data/dem/surveyed.txt`, sources in `ASSETS.md`).
Twelve runway ends from the FAA's data - Denver, Las Vegas, Boston, Juneau,
Anchorage, Utqiagvik, from 36 to 71 N, and so four bands of tile widths - are
within the Copernicus DEM's stated 4 m (the product handbook's "< 4m (90% linear
error)"), every one of them, worst 3.53 m, most within 2 m, and all below the
survey; five coastal waters are at 0.00 m. Twelve tiles are fetched for it, and
CI keeps them.

**Watched to fail:** the ETag check removed (the damaged tile was accepted);
heights above the ellipsoid compared with the survey instead of heights above
the geoid (Denver 20.7 m out); a wrong SHA-256 round constant, and a padding
boundary moved (both digest tests).

**Also:** AppleClang's `-Wdouble-promotion` refused a float returned as a
double in `dem.cpp` that GCC let pass; the Vulkan loader ignores
`VK_DRIVER_FILES` in an elevated process, which is what CI's Windows runner
runs, so CI now registers lavapipe in the registry instead.

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

### The geoid, and zip archives, 2026-09-18

**What is missing first:** the geoid converts heights but nothing uses it yet:
there is still no height query. (The grid's terms were at first not found
stated; PROJ's data package records NGA's EGM2008 as public domain, quoted in
`ASSETS.md`.)

**`world/geoid.hpp`** reads GeographicLib's geoid grids in their PGM form and
interpolates bilinearly, wrapping longitude and clamping latitude at the poles.
**`world/zip.hpp`** reads stored and DEFLATE entries out of a zip archive, as
GeographicLib distributes its grids, checking each against its CRC-32; Zip64,
encryption and multi-disk archives are refused by name.
**`world/byte_source.hpp`** is where the GeoTIFF reader's byte sources went, so
the zip reader could share them.

**The tests.** CRC-32 against its published check value. A zip archive built in
the test, with stored, deflated and empty entries, reads back, and six bad ones
are each refused for their reason. A small synthetic grid interpolates exactly
as bilinear interpolation must: at samples, between columns, between rows, in
a cell, across 360 degrees, at negative longitudes, at 180 E and W, at both
poles and past them; five malformed grids are refused. **The real grid**,
`egm2008-5.zip` pinned by SHA-256, gives GeographicLib's own online GeoidEval
undulations at 14 places within the grid's stated 0.478 m - the worst is 0.118
m - including the Indian Ocean low (-106.9 m), New Guinea (+70.2 m), both
poles, and the date line from both sides.

**Watched to fail:** negative longitudes negated rather than wrapped (the
synthetic test at "a negative longitude is the same place", and Denver 27 m
out); the interpolation weights between columns swapped (New Guinea 0.55 m
out). The sanitized build also caught a real bug on the first run: a read of
zero bytes passed a null pointer to `memcpy`.

### Reading the Copernicus DEM's GeoTIFF tiles, 2026-09-18

**What is missing first:** there is no height query yet. Nothing converts the
DEM's heights, which are above the EGM2008 geoid, to heights above the WGS84
ellipsoid; nothing fetches tiles at run time; nothing knows which tile covers a
point, or interpolates between samples. What exists reads a tile's samples.

**DEFLATE, written here** (`world/inflate.hpp`): zlib streams and raw DEFLATE,
with the Adler-32 checked and a limit on how much a stream may decompress to.
It is not zlib because the renderer's dependencies will bring their own, and
two static zlibs in one program are one set of symbols too many. It is tested
against ten streams Python's zlib made (`tools/make_inflate_fixtures.py`) with
every strategy zlib has - fixed and dynamic codes, stored blocks, runs,
literals only, sync and full flushes, and matches 32,000 bytes back - and
against hand-built streams: stored blocks of the largest size, a failed
checksum, every length of three streams cut short, a decompression limit one
byte too small, eight kinds of malformed header and block, and 2,000
deterministic corruptions under the sanitizers.

**GeoTIFF** (`world/geotiff.hpp`): single-channel float32 rasters on WGS84
latitude and longitude, in tiles or strips, either byte order, uncompressed or
DEFLATE, with or without the floating-point predictor, sample grids of points
or areas, the GDAL no-data value, and overviews. Anything else - BigTIFF,
integer samples, projected coordinates, a transformation matrix - is refused
by name. Tests write GeoTIFFs in all 32 combinations of those layouts and read
every sample back, and write 14 files it must refuse, each for its stated
reason.

**The real tile.** `Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif`, pinned by
SHA-256, decodes to exactly the heights a separate decoder gave - Python, its
zlib, and the predictor written again from TIFF Technical Note 3 - at 15
samples across its internal tiles, including tile edges and the sea. Its
sample (0, 0) is exactly at 151 E, 33 S: the Copernicus grid's samples are
points on whole arc-seconds, and each tile's south and east edges belong to
its neighbours.

**Downloads for tests** (`tests/cmake/fetch.cmake`): the files the tests need,
listed with their SHA-256 in `tests/data/downloads/files.txt`, fetched once
before the tests that read them. Without the network those tests are skipped;
with `GLIDESLOPE_REQUIRE_NETWORK` set, as CI sets it, that fails. A file that
arrives with the wrong hash always fails and is deleted. CI keeps the
downloads between runs. The unit-test harness can now skip a test (exit 77).
The DEM's source, pinned file and licence, with its notices quoted, are in
`ASSETS.md`.

**Watched to fail:** a distance code's extra-bits entry one short (the far
fixture referred back before its start); the stored-block length check
removed (the malformed test caught it); the predictor's running sum removed
(the layouts test, and the real tile at sample (0, 0): 198.016 for 274.590); the
half-sample shift for area grids reversed (the layouts test); a wrong pinned
hash (the fetch failed and left nothing behind). The first run of the
truncation test took 277 s: zeros past the end of a stream decoded as literals
up to the limit. The decoder now refuses input read more than four bytes past
its end.

### Shaders, reversed depth and the floating origin, 2026-09-17

**CI run 35229869171 said:** Metal on macOS and Direct3D 12 (WARP) on Windows
pass every frame test - sky, window, reversed depth, floating origin - in every
preset, and Rocky 9 passes. Ubuntu's sanitized build failed on the leak rule
(below), and Vulkan on Windows could not start: the runner has no Vulkan loader
or driver. CI now installs Mesa's lavapipe from mesa-dist-win 26.2.0 and the
Khronos loader from LunarG's 1.4.357.0 runtime components, each pinned by
SHA-256.

**What was missing first, before that run:** everything here was proved on
Vulkan on Linux alone. The meshes are coloured boxes and quads standing
in for mountains and an aircraft, drawn by one unlit pipeline. There is no
terrain, no texture and no culling.

**The shader toolchain.** Shaders are GLSL, compiled during the build by
`glideslope_shaderc` (`tools/shaderc/main.cpp`): glslang makes SPIR-V,
SPIRV-Cross makes MSL and HLSL, and on Windows D3DCompile makes DXBC. The
output is a C++ source per shader with each form and the resource counts
SDL_GPU asks for, read from the SPIR-V. The compiler checks every resource
against SDL_GPU's documented layout and fails the build naming the one out of
place. `ext/glslang` (16.6.0) and `ext/spirv-cross` (vulkan-sdk-1.4.357.0) are
new submodules; the program links neither. The decision is recorded in
`REQUIREMENTS.md`. The MSL and HLSL for a two-texture fragment shader were read
and put textures, samplers and uniform buffers where SDL_GPU's documentation
says: `[[texture(0)]]`, `[[sampler(0)]]`, `[[buffer(0)]]`; `t0`/`s0` in
`space2`, `b0` in `space3`.

**The renderer draws meshes.** `gfx::Renderer` now has a D32 float depth buffer
and a mesh pipeline, uploads meshes, and draws them from a camera. Positions are
double-precision ECEF until each mesh's transform relative to the camera is
worked out, in double, and narrowed to float (`gfx/scene.hpp`). The projection
is perspective with an infinite far plane and reversed depth: cleared to 0,
kept when greater. The client has `--scene sky|origin|depth` and
`--at LAT,LON,HEIGHT` / `--at-ecef X,Y,Z` to put a scene anywhere on the Earth.
`--size` is parsed without `sscanf`, which MSVC refuses as deprecated.

**The tests.**
- `a_distant_mountain_and_a_nearby_aircraft_draw_with_no_z_fighting_on_<driver>`:
  a face 40 km away with another 1 m in front, and a skin 1 m away with a decal
  1 mm in front, the nearer of each drawn first. Every pixel of both halves,
  shot at the Earth's centre and at Sydney airport, must be the nearer surface.
- `a_scene_draws_the_same_at_the_earths_centre_and_far_from_it_on_<driver>`:
  boxes from 7 cm to 2 m, shot at the Earth's centre, on the equator, at 45 N
  45 E 10 km up and at Sydney on frames 1 and 60 - every file identical, and at
  least a quarter of the pixels not sky.
- Six shader compiler tests: a shader keeping the layout compiles; a uniform
  buffer in the wrong set, a texture not bound from 0, a separate sampler, push
  constants and a GLSL error each fail with the reason.

**Leaks, judged rather than switched off.** Mesa's lavapipe leaks two small
allocations on a worker thread when it builds a pipeline, and Xlib, behind a
window, keeps its resource database and input method; both are unloaded before
exit. `tests/cmake/client.cmake` reads the leak report and judges each leak by
who allocated it - the first frame outside the sanitizer and the C and C++
runtimes: a leak allocated in glideslope or SDL fails the test, one allocated
inside a library loaded at run time does not, even when SDL called it. The
first CI run judged by any frame in SDL, and Ubuntu's Xlib, which keeps frame
pointers, failed it; the Ubuntu report was replayed through the new rule and
all 105 of its leaks were judged Xlib's, and a `new int[1000]` and a leaked
`std::string` in `main` still failed. The windowed test no longer runs with
leak detection off.

**Watched to fail:**
- positions narrowed to float before the camera offset was taken: the equator,
  45 N 45 E and Sydney frames all differed from the centre's;
- conventional depth (near 0.1 m, far 100 km, cleared to 1, kept when less):
  all 1,536 pixels of the far pair showed the face behind, at both places;
- the depth test off: both pairs showed the surface behind in every pixel;
- `new int[1000]` leaked in the client's `main`: the sky test failed, quoting
  the frame in `main.cpp`;
- each shader compiler case, as listed.

CI also said clang-cl compiled SDL's MSVC-only inline functions under this
project's warnings: SDL's include directory came from `SDL3_Headers`, which was
not marked as a system target. It is now.

**Verified locally:** `linux-debug` (sanitized) and `linux-release` pass 55 of
55.

### A window, and Metal's video driver, 2026-09-17

CI run 35226940913: macOS passed both presets, Metal headless and in a window;
Ubuntu and Rocky 9 passed with the windowed test on Xvfb. Windows did not build
(below, fixed in the next entry).

**What is missing first:** Direct3D 12 and Metal have still not drawn a frame.
The first CI run of the frame tests failed on both before a device was asked
for, for reasons that had nothing to do with the GPU (below); this commit is
their second attempt. Windows Vulkan needs a Vulkan driver the runner may not
have.

**What CI said about the last commit** (run 35224985264): Ubuntu and Rocky 9
passed, Vulkan on lavapipe. Windows did not compile the client — MSVC's
`<string>` does not bring in `std::runtime_error`, which GCC's does; the client
now includes `<stdexcept>`. macOS failed
`the_client_renders_the_sky_headless_on_metal` in both presets with SDL's
"SDL_HINT_GPU_DRIVER metal unsupported!". SDL's `METAL_PrepareDriver` accepts
only a video driver that can create a Metal view, and headless set SDL's
offscreen video driver, which cannot. On macOS headless now keeps Cocoa's video
driver and simply opens no window; everywhere else it is still `offscreen`.

**A window, tested.** `gfx::Renderer::presented()` counts the frames that
reached the window's swapchain, and the client prints it after a windowed
`--shot`. `the_client_renders_the_sky_in_a_window_on_<driver>` opens a window,
renders ten frames, requires at least one to have been presented, and reads the
frame back as the headless test does. On Linux with no `DISPLAY` or
`WAYLAND_DISPLAY` it reports itself skipped; CI sets
`GLIDESLOPE_REQUIRE_WINDOW`, which makes that a failure, and runs the Linux
tests on Xvfb. The sanitized build runs the windowed test with leak detection
off: the X11 and driver libraries loaded for a window leak allocations whose
stacks hold no frame of glideslope or SDL. The headless test keeps it on.

**Watched to fail:** with the present count never incremented the test failed
with "no frame reached the window's swapchain"; with no display and
`GLIDESLOPE_REQUIRE_WINDOW` set it failed, and without it it skipped.

**Verified locally,** under WSLg: both frame tests pass in `linux-debug`,
presenting 10 of 10 frames to the window.

### SDL3, a GPU device, and a frame, 2026-09-17 — Vulkan on Linux only

**What is missing first:** only Vulkan has drawn a frame, on Linux, through
Mesa's lavapipe on the CPU. Direct3D 12 and Metal have never created a device;
CI tries them with this commit, on runners that have no GPU, and may not be able
to. No test opens a window: every frame so far is headless. The frame is the
sky's clear colour and nothing else.

**What exists.** `ext/sdl` is SDL 3.4.16, built statically by
`cmake/Sdl.cmake`. `glideslope_gfx` is a new library: `gfx::Renderer` owns an
SDL_GPU device on the driver asked for, or SDL's choice, renders into a 1280x720
(or `--size`) colour target cleared to the sky, blits it to a window when there
is one, and reads a frame back off the GPU; `gfx::save_bmp` writes it.
`glideslope`, the client, is a new executable: `--headless` renders without a
window, `--gpu-driver` picks Vulkan, Direct3D 12 or Metal, `--shot FILE` and
`--shot-at FRAME` write a frame and exit. The simulation still links no SDL;
the layering checks still pass.

**The test** — `the_client_renders_the_sky_headless_on_vulkan` on Linux, and
the same for `direct3d12` and `vulkan` on Windows and `metal` on macOS — runs
the client headless on that driver, requires it to report that driver, and
reads the BMP it wrote: header, bit masks, and all 3,072 pixels of a 64x48
frame, each channel within 1 of the sky times 255.

**Watched to fail:** a sky a shade greener failed with pixel 0's green at 168;
a render pass that loaded rather than cleared its target failed with red at 0.

CI now installs SDL's Linux development packages on Ubuntu and Rocky 9 (EPEL and
CRB on Rocky), and Mesa's lavapipe with the Vulkan loader.

**Verified locally:** `linux-release` passes 46 of 46; the frame test passes
under the sanitized `linux-debug` too, with no leak reports from SDL or Mesa.

### Positions on the Earth, 2026-09-17

**Proved on every platform** by CI run 35223462224 (commit `29ed2f3`), which
passed all 45 tests in every preset. Nothing uses the conversions yet — the
simulation keeps JSBSim's positions, and there is no renderer or terrain.

`glideslope_world` is a new library, and `src/world/geodesy.hpp` its first
file: `Ecef` (double-precision metres, Earth-centred and Earth-fixed),
`Geodetic` (latitude, longitude, height above the ellipsoid), and the
conversions between them on WGS84. The forward conversion is the exact formula;
the inverse is Vermeille's closed form (2004), with no iteration.

**Tests:**

- `wgs84_reference_points_convert_exactly` — the equator at 0°, 90° E and 180°,
  both poles (100 m above the South Pole at an arbitrary longitude), and the
  derived semi-minor axis against WGS84's published 6,356,752.314245 m.
- `geodetic_positions_round_trip_through_ecef_within_a_millimetre` — 819
  positions, counted: 13 latitudes (both poles exactly and a ten-millionth of a
  degree away, the equator exactly and a billionth either side), 9 longitudes
  (both sides of the date line, the prime meridian), and 7 heights from 430 m
  below sea level to 40 km. Latitude and longitude must come back within a
  billionth of a degree and the position within a millimetre; the worst was
  3.4 nanometres.
- `the_geodetic_conversion_agrees_with_jsbsims_within_a_millimetre` — the same
  819 positions converted back by JSBSim's `FGLocation`, a different algorithm
  set up with JSBSim's own WGS84 axes in feet, not this project's constants.

**Watched to fail, three ways:** a spherical Earth in the forward conversion
failed the reference and round-trip tests; one term dropped from the inverse's
height failed the round-trip and JSBSim tests; and a flattening of 1/298.26
instead of 1/298.257223563 failed the reference test and — once the JSBSim
check used JSBSim's own axes rather than this project's — the JSBSim test too.

**Verified locally:** `linux-release` passes 45 of 45.

### The packaged CLI flies, 2026-09-17

**Proved on every platform.** The `package` workflow's run 35223462263 (commit
`29ed2f3`) passed all five jobs, each unpacked copy flying the selftest to the
same hash as the CI build of its platform: `3bf3106980a5ad7b` from the Linux
tarball in both containers, `6d19773416ce7b3f` on macOS, `d9a3d51b8c2cdd52` on
Windows.

Every package's check now runs `glideslope_cli selftest` out of the unpacked
copy — the Linux tarball in the stock Rocky 9 and Ubuntu 24.04 containers, the
macOS tarball and the Windows zip from a separate directory — after
`--version` and `aircraft c172p`. The selftest needs the model, the tuned
propeller and the selftest log to have travelled with the program, and flies
five minutes with them. The workflow now also runs when anything under
`assets/` changes, since that is what a package carries.

### Cross-platform flight checks, 2026-09-17

**The platforms fly identically, to the precision they print.** CI run
35223462224 (commit `29ed2f3`) compared the five release builds for the first
time: every one of the nine figures agreed to its two printed decimals on GCC
on Ubuntu and Rocky 9, AppleClang, MSVC and clang-cl — a spread of zero against
the 1% allowed — and the five-minute selftest ended at -34.1073165, 151.2320529,
1,537.3 ft, heading 335.73°, 110.33 KCAS on all five.

**The hashes still differ**, as they were designed to be allowed to: Linux GCC
prints `3bf3106980a5ad7b` (the same on Ubuntu and Rocky 9), MSVC and clang-cl
both `d9a3d51b8c2cdd52`, AppleClang `6d19773416ce7b3f`. So does the development
machine's own release build (`13253df30045304a`, GCC 14.3.1) from CI's GCC 14.2
release build, while CI's matches the development machine's sanitized build.
The bits differ somewhere in five minutes of flight; where the aircraft is, to
seven decimal places, does not.

**The tolerances, set before any cross-platform number was seen:** every one of
the nine published figures within 1% of its published value across all five
release builds, ten times tighter than the handbook ranges; and the five-minute
selftest ending within 300 ft horizontally, 30 ft vertically, 1 kt and 2° of
heading of the first platform's. The selftest's hash is not compared: the
machines are expected to differ in the last bits.

**How.** Each CI job now runs `glideslope_cli figures c172p` and
`glideslope_cli selftest` on its release build — GCC on Ubuntu and on Rocky 9,
AppleClang, MSVC and clang-cl — and uploads what they print. A new job, "The
same flights everywhere", needs all four jobs, downloads the five results, and
runs `tests/cmake/cross_platform_flights.cmake`, which names the five platforms
and fails if any is missing.

**Its test** makes five platforms from this build's own output. Identical, with
one platform's climb rate 0.3% higher, they must be accepted; a glide ratio 1.9%
higher on one, the selftest ending 400 ft north on one, and one platform's
figures missing must each be refused. **Watched to fail for the right reasons:**
the first version of the test moved the glide ratio from 9.38 to 956 instead of
9.56, and the check refused it — correctly, but for a gross error rather than
the 2% it claimed to test. With the arithmetic fixed, each refusal's message
names the glide ratio's platforms, the 400 ft, and the missing platform. The
test is
`the_cross_platform_check_accepts_platforms_that_agree_and_refuses_one_that_does_not`.

### The selftest and its hash, 2026-09-17

**Proved on every platform.** CI run 35222154134 (commit `3a6ef0c`) passed all
41 tests in every preset, so on each the hash held run to run and moved when
the drag did; the `package` workflow (35222154187) passed too. The hash holds
on one build: the release and sanitized builds on the same machine print
different hashes, and different platforms are not expected to agree.

**The hash, and the rule that goes with it.** On the development machine
(Rocky Linux 10, GCC 14.3.1), `linux-release` prints
`hash 13253df30045304a` and `linux-debug` prints `hash 3bf3106980a5ad7b`. A
change that moves either is deliberate, and is written here with its reason.

`glideslope_cli selftest [NAME]` reads `assets/selftest/NAME.log` (default
`c172p`) and flies it: brake release with 10° of flap, rotation at 55.6 KCAS, a
climb at 75.4 KCAS, flaps up, a climbing turn at 20° of bank, levelling at
1,500 ft, a level turn at 25°, and cruise, for 300 s — 36,000 steps, 0.19 s in
release. After every step it folds sixteen fields of the aircraft's state into
a 64-bit FNV-1a hash, bit for bit, and prints the hash with where the flight
ended: at 1,537 ft, 110 KCAS, heading 336° for the development machine's
release build.

**The log is of pilot commands, not stick positions.** The first log held the
controls themselves — throttle, elevator, aileron — and the aircraft rotated,
rolled left, stalled and cartwheeled down the runway, over and over, for three
minutes: a stable aircraft cannot be flown open loop. The log now holds commands
(`throttle`, `flaps`, `brakes`, `rotate_at`, `hold_speed`, `hold_altitude`,
`bank`, `end`) that the test pilot flies, which is as fixed as the flight needs
to be and stays sensible when the model is retuned. On the ground the pilot now
steers along the starting heading: without it the take-off roll swerved 90°,
and a first version of the steering had its sign the wrong way round and spun
the aircraft on the runway, because this model's rudder yaws left for positive
commands.

`glideslope_cli` gained `--data DIR`, reading data from DIR instead of `data/`
beside the program.

**Tests, 41 now:**

- `the_selftest_prints_the_same_hash_every_run` — two runs, identical output.
- `a_one_line_change_to_the_physics_moves_the_selftest_hash` — the selftest
  flown on an unchanged copy of the data prints the same hash as the data
  itself, so `--data` really is read; on a copy whose zero-lift drag is 0.0311
  instead of 0.031 it prints a different one (`f7de68708bc9c1ce`).
- `a_selftest_log_with_a_command_it_does_not_know_is_refused_at_its_line`
- `the_cli_refuses_a_selftest_for_an_aircraft_with_no_log`

**Watched to fail:** adding the wall clock to the hash failed the run-to-run
test; hashing the simulation time alone failed the physics-change test.

**Verified locally:** `linux-release` passes 41 of 41, and the sanitized
`linux-debug` passes 41 of 41 in 182 seconds, up from 97; the selftest flights
under the sanitizers are most of the difference.

### State capture and set/resume, 2026-09-17

**Proved on every platform.** CI run 35221060150 (commit `7050a3f`) passed all
37 tests in every preset, the tracking tests among them. A restore settles
JSBSim's hidden states over two simulated seconds rather than setting them, so
it is close, not exact, and costs about 240 steps of the flight model each
time.

**The key technical risk of the design is answered: an aircraft can be put into
a captured state and flies on as the original does.** Restored into a fresh
instance in every phase the figure checks fly, and then flown alongside the
original on the original's controls:

| Phase | After 1 s | Worst over 10 s |
| --- | --- | --- |
| Static run-up | 0.002 ft, 0.013°, 0.003 kt | 0.006 ft, 0.074°, 0.066 kt |
| Take-off roll | 0.0003 ft, 0.0001°, 0.0004 kt | 0.012 ft, 0.000°, 0.001 kt |
| Full-throttle climb | 0.002 ft, 0.002°, 0.002 kt | 0.099 ft, 0.008°, 0.004 kt |
| Cruise | 0.011 ft, 0.011°, 0.013 kt | 0.557 ft, 0.020°, 0.031 kt |
| Glide, engine stopped | 0 | 0 |
| Stall approach, flaps up | 0.123 ft, 0.424°, 0.062 kt | 10.8 ft, 1.27°, 0.57 kt |
| Stall approach, full flaps | 0.087 ft, 0.341°, 0.046 kt | 8.9 ft, 1.17°, 0.37 kt |
| Level turn, 30° | 0.029 ft, 0.027°, 0.037 kt | 1.36 ft, 0.057°, 0.070 kt |

**The tolerance** is 0.5 ft, 1° and 0.25 kt after one second, and 25 ft, 3° and
1 kt over ten — about twice the worst measured, which is approaching a stall,
where small differences grow fastest. One second is what reconciliation needs:
it resets to the server's state and replays the inputs the server has not yet
seen, a quarter of a second of flying at 200 ms of latency. Ten seconds is a
margin beyond it. An aircraft that has flown on for five seconds with other
controls and is put back to the snapshot stays within 0.24 ft, 0.022° and
0.006 kt of a fresh restore over the next ten.

**How.** JSBSim has no snapshot call. `Aircraft::capture()` reads the rigid-body
state relative to the Earth (position, attitude quaternion, body velocities and
rates), each engine's running flag and propeller RPM, and every property that is
both readable and writable — 286 for the Cessna — except the run's settings,
the atmosphere and the rigid body. `Aircraft::restore()` starts a fresh instance
at the snapshot's position, then for two simulated seconds re-applies the
snapshot before every step so the hidden states converge, applies it once more,
recomputes the forces with no time passing, and seeds the integrators from them.

**What each part is for, found the hard way and each watched to fail:**

- **The Earth's rotation.** JSBSim integrates in an inertial frame whose angle
  to the Earth grows with time, so an inertial state from one instance means
  somewhere else in another. The first prototype put the copy 25,000 ft away
  after twenty seconds. Restore rebuilds the inertial position, orientation and
  velocity in the instance's own frame. Leaving out the position or the
  orientation fails both tracking tests, in at least five phases each; leaving
  out the velocity fails all eight phases.
- **Instance-relative property names.** Two instances in one process sit at
  different indices in the property tree; the prototype's writes went to the
  wrong one and silently changed nothing.
- **Readable and writable only.** `propulsion/set-running` is write-only and
  reads back as 0; restoring it stopped the engine.
- **The settling.** Restoring only what can be read left the engine 60 hp adrift
  and the copy 80 ft and 3.6° away after thirty seconds of manoeuvring. Without
  it the stall approaches and the turn fail.
- **The properties.** Without them — controls, flaps, fuel, mixture — six of the
  eight phases fail.
- **The propeller RPM.** Without it the stall approaches fail.
- **A sensible start.** A first version started a fresh instance at an altitude
  estimated from the Earth's mean radius — 37,000 ft underground at 34°S — and
  JSBSim's start left NaN in the engine; the snapshot now carries the geodetic
  position for that start.
- **One path for the rigid body.** Position, attitude and velocity properties
  are not restored, because their setters move the aircraft as a side effect.
  With both paths, removing the explicit inertial position changed no test; with
  one, removing it fails both tracking tests.

`src/sim/test_pilot.hpp` now holds the test pilot, shared by the figure checks
and these tests.

**Tests, 37 now:**

- `an_aircraft_restored_in_every_phase_of_flight_tracks_the_original` — eight
  phases, counted.
- `an_aircraft_put_back_to_an_earlier_snapshot_flies_on_as_a_fresh_restore_does`
- `a_snapshot_of_one_model_cannot_be_restored_into_another`

**Verified locally:** `linux-release` passes 37 of 37, and the sanitized
`linux-debug` passes 37 of 37 in 97 seconds.

### The Cessna 172P against its handbook, 2026-09-17

**Proved on every platform.** CI run 35219070877 (commit `e6b97eb`) passed all
34 tests in every preset — GCC 14.2 on Ubuntu and Rocky 9, AppleClang 17, MSVC
19.51 and clang-cl 20.1 — so all nine figures land in range on each, and the
`package` workflow (35219070739) passed with the tuned model in every package.
The handbook publishes no turn rate, so the turn is checked against physics
rather than a published number.

**JSBSim's own C172P did not fly to its handbook.** The checks below, flown on
JSBSim's untouched model at the same 2,400 lb, landed out of range on five of
nine, so the model was tuned — the question Phase 1 exists to ask. The tuning
is not hand-edited into the model: `tools/make_c172p.py` makes
`assets/jsbsim/` from the pinned files with each change listed and justified in
its docstring, and a test fails if the committed files differ from what it
makes.

| Figure | Handbook | Range | Stock JSBSim | glideslope |
| --- | --- | --- | --- | --- |
| Static RPM, full throttle | 2300 to 2420 | 2300 to 2420 | **2538** | 2316 |
| Take-off ground roll | 890 ft | ±10% | 940 | 922 |
| Climb, sea level, 76 KIAS | 700 ft/min | ±10% | **978** | 742 |
| Cruise, 8000 ft, 2650 RPM | 121 KTAS | ±3 kt | 123.1 | 119.7 |
| Glide, 65 KIAS, engine off | 9.1:1 | ±10% | 8.38 | 9.38 |
| Stall, flaps up | 51 to 52 KCAS | 49.5 to 54 | **54.4** | 51.1 |
| Stall, flaps 10 | 48 to 49 KCAS | 46 to 51 | **51.0** | 47.6 |
| Stall, flaps 30 | 46 KCAS | 44 to 48 | **48.8** | 45.6 |
| Turn, 30° bank | g·tan(bank)/V | ±3% | 98.8% | 99.0% |

The figures, their sources and their conditions are in
`assets/figures/c172p.xml`: every number is from the Cessna Model 172P Pilot's
Operating Handbook of 12 May 1981, by section and figure, with indicated
airspeeds converted to calibrated by its figure 5-1. The ranges are this
project's.

**What each change does**, measured by leaving it out and flying every check:

| Left out | Static RPM | Ground roll | Climb | Cruise | Glide |
| --- | --- | --- | --- | --- | --- |
| nothing | 2316 | 922 | 742 | 119.7 | 9.38 |
| propeller power and thrust factors | 2538 | 697 | 1147 | 121.5 | 9.38 |
| thrust boost below advance ratio 0.5 | 2316 | 1216 | 725 | 119.7 | 9.38 |
| drag changes | 2316 | 935 | 661 | 121.3 | 7.89 |

The stall speeds moved from 54.4, 51.0 and 48.8 KCAS to 51.1, 47.6 and 45.6 with
the lift-curve and flap-lift changes, which barely move anything else.

**How it was found.** A first prototype flew each figure and showed the
propeller over-revving (2,800 RPM in a full-throttle climb, past the 2,700 RPM
redline) and the stall limited by elevator travel rather than by the wing: the
elevator reached its stop at 16.5° angle of attack, at a lift coefficient of
1.45. The handbook's static RPM range, its airspeed calibration table and its
stall table then gave targets that separate the propeller from the airframe.
JSBSim's two other C172 models were flown too and were no closer: `c172x` glided
at 13:1 and `c172r` climbed at 905 ft/min against its handbook's 720.

**The flights** are in `src/sim/figures.cpp`, flown by a small test pilot — a
pitch-attitude hold with slower speed and altitude loops above it, a wing
leveller and a rudder that holds the sideslip at zero. It is not the Phase 4
autopilot and is not meant to fly like a person. A first version of its rudder
let the aircraft slip half a degree in the turn, which turned 2% slower than its
bank demanded; an integral took the slip out.

- **Static RPM:** brakes on, full throttle, mixture leaned in steps; the most
  RPM any mixture reaches.
- **Take-off:** flaps 10, full throttle against the brakes, released; ground
  distance until no wheel is on the ground, rotating at 55.6 KCAS (51 KIAS).
- **Climb:** full throttle at 75.4 KCAS (76 KIAS); the climb over forty seconds
  through sea level, after thirty to settle.
- **Cruise:** level at 8,000 ft, mixture leaned to the most RPM, then throttle
  holding 2,650 RPM; average true airspeed over the last fifty seconds of three
  minutes. It fails if the RPM cannot be held.
- **Glide:** mixture cut off, propeller windmilling, 66 KCAS (65 KIAS); ground
  distance over height lost across three minutes.
- **Stalls:** power off, from 70 KCAS the target speed falls one knot a second
  and the pilot follows it with the nose; the slowest calibrated airspeed.
- **Turn:** 30° bank level at 3,000 ft; the turn rate as a percentage of
  g·tan(bank)/true airspeed, both averaged over thirty seconds after a minute.

Every flight loads the aircraft as the figures file says and fails unless JSBSim
then weighs it at the file's 2,400 lb.

`glideslope::sim::Aircraft` gained `load()` and `property()`. The CLI gained
`glideslope_cli figures NAME [FIGURE]`, which prints each measurement against
its range and exits 1 if any is out. `assets/` is new: `assets/jsbsim/` (the
made model) and `assets/figures/`, copied to `data/` beside the programs; a
change to any file there now reconfigures, which `CONFIGURE_DEPENDS` alone had
not done — the first tuning run changed the assets and measured nothing new.

**Tests, 34 now:** one per figure —
`a_cessna_172p_at_full_power_climbs_near_its_published_rate` and eight like
it — plus
`every_published_figure_has_a_flight_and_every_flight_a_figure`,
`the_committed_cessna_172p_is_what_its_tuning_script_makes` (skipped, not
passed, where there is no Python 3), and CLI tests for one figure, a figure the
aircraft does not have, and an aircraft with no figures file.

**Watched to fail, six ways:** a hand edit to the committed model failed the
script check; writing the climb figure as 500 failed the climb test, naming the
measurement, the range and the source; a figure in the file with no flight, and
a flight in the code with no figure, each failed the coverage test; a loading
that came to 2,330 lb failed with the weight; and the stock propeller factors
failed the static RPM, take-off and climb tests.

**Verified locally:** `linux-release` passes 34 of 34, and the sanitized
`linux-debug` passes them too, the nine flights taking 44 of its 67 seconds.

### A fixed 120 Hz step, 2026-09-17

**Proved on every platform.** CI run 35216463701 (commit `1d2649c`) passed all
20 tests in every preset — GCC 14.2 on Ubuntu (sanitized and release) and Rocky
9, AppleClang 17 (sanitized and release), MSVC 19.51 (debug and release) and
clang-cl 20.1 — so the five differently-chunked flights ended in identical
states on each. Before that run it had been proved on Linux only.

`glideslope::sim::FixedStep` turns elapsed time into whole 120 Hz steps.
**The steps taken depend only on the total time elapsed**, never on how it was
divided: time is held in whole nanoseconds, and the steps due are computed from
the total each time, split into whole and part seconds so nothing overflows for
the 292 years an int64 of nanoseconds lasts. `alpha()` is how far the time since
the last step has got towards the next, for interpolation.

`glideslope::sim::Aircraft` now also takes initial conditions (position,
altitude, terrain elevation, heading, calibrated airspeed, engine running),
controls (elevator, aileron, rudder, throttle, mixture, flaps, brakes), steps
once per call with JSBSim's time step set to exactly 1/120 s, and reports its
state (time, position, attitude, body velocities and rates, calibrated
airspeed, climb rate, engine RPM). Positive elevator is stick back; JSBSim's own
command is the other way round, and the wrapper turns it over.

**A unit-test harness**, `tests/unit/harness.hpp`, is new: a registry of named
test functions in one executable, `glideslope_tests`, run one test per ctest.
`every_compiled_unit_test_is_registered_with_ctest` compares the executable's
`--list` with the names given to ctest, both ways, because a test compiled and
never registered would never run and nothing would say so.

**Tests, 20 now:**

- `the_fixed_step_counts_steps_from_the_total_time_alone` — 11 ways of dividing
  time (1 µs, 1 ms, 16 ms, 33 ms, a 60 Hz and a 144 Hz frame rounded to the
  nanosecond, one step less a nanosecond, 100 ms, 1 s, uneven chunks of 0 to 50
  ms, and all at once), each over 1 s and over 10 s. After *every* advance the
  steps taken must equal the steps in the time fed so far, as computed by the
  standard library's own ratio arithmetic, and the advance must have returned
  the difference.
- `the_fixed_step_reports_how_far_it_is_into_the_next_step` — 10.004 s is
  1200 steps and 0.48 of the next; 4,333,333 ns more is a third of a nanosecond
  short of completing it, and one nanosecond more completes it.
- `the_fixed_step_refuses_time_running_backwards`.
- `the_fixed_step_counts_a_century_without_overflowing` — 36,525 days,
  3.156e18 ns, whose steps would overflow if multiplied naively.
- `a_flight_fed_its_time_in_any_chunks_ends_in_the_same_state` — a Cessna over
  Sydney at 3,000 ft and 100 kt, flown for ten seconds by a script that is a
  function of the step number alone (throttle, a slow elevator sine, an aileron
  and a rudder input), with its time arriving in 1 ms, 16 ms, 100 ms and uneven
  chunks and all at once. All five must take 1200 steps and end in exactly the
  same state, compared field by field with `==`. The test also requires the
  flight to have rolled and the engine to be running, so the flights cannot
  agree by all doing nothing.
- `the_elevator_pulled_back_pitches_the_nose_up` — and pushed forward, down.
- `every_compiled_unit_test_is_registered_with_ctest`.

**Watched to fail, five ways.** Counting steps by adding up floating-point
fractions of a step failed the fixed-step test ("16 ms: after 400000000 ns, 47
steps taken, 48 due") and the flight test ("16 ms: took 1199 steps"). The first
version of the fixed-step test checked only the final count and **passed** that
same bug; it was changed to check after every advance and to include 16 ms and
33 ms chunks, and only then failed it. Setting the controls once per chunk of
time instead of once per step failed the flight test ("16 ms: ended in a
different state from 1 ms"). Reversing the elevator's sign failed the elevator
test. Leaving a compiled test out of the ctest list failed the registration
test.

**Verified locally:** `linux-release` and the sanitized `linux-debug` each pass
20 of 20. The flight test's five ten-second flights take about 0.04 s in
release and 1.9 s sanitized.

### JSBSim, pinned and built, 2026-09-17

**Proved on every platform.** JSBSim builds and the aircraft tests pass in every
preset (CI run 35216463701). The first run (35215481035) failed only in
`windows-clang`: enabling C left that preset with clang-cl for C++ and MSVC for
C, which CMake refuses, and commit `b234322` names clang-cl for both. The
`package` workflow (runs 35215392807 and 35216463589) passed on every platform,
running `glideslope_cli aircraft c172p` out of each unpacked package.

**What exists.** `ext/jsbsim` is JSBSim v1.3.1 (`3b25f25`), built by
`cmake/Jsbsim.cmake` from its `src/` directory only, statically, with its
headers as system headers and the sanitizers applied in the sanitized presets.
The project now enables C as well as C++, for JSBSim's bundled expat.
`ext/README.md` says why each of those choices was made.

- `glideslope_sim` links `libJSBSim`. `glideslope::sim::Aircraft` loads a model
  through `FGFDMExec`, keeping JSBSim's headers out of its own, and reports the
  model's figures. JSBSim's own output is turned off unless `JSBSIM_DEBUG` asks
  for it; it still prints to standard error when a model fails to load.
- `glideslope_platform` is new: `data_directory()` finds `data/` beside the
  running program, through `/proc/self/exe`, `_NSGetExecutablePath` or
  `GetModuleFileNameW`.
- The Cessna 172P's files — `aircraft/c172p/`, `engine/eng_io320.xml` and
  `engine/prop_75in2f.xml` — are copied at configure time into `data/jsbsim/` in
  the build tree, and installed into packages. Which aircraft are copied is a
  CMake list for now, which Phase 5 replaces with data.
- `glideslope_cli aircraft c172p` prints the model's name, description, wing
  area (174.0 sq ft), wingspan (35.8 ft), chord (4.9 ft), empty weight
  (1500.0 lb) and engine count (1). An aircraft with no files exits 1 naming
  it.
- Packages now carry the aircraft data and `licenses/JSBSim.txt`,
  `licenses/expat.txt` and `licenses/GeographicLib.txt`, and nothing else of
  JSBSim's: our install rules are the `glideslope` component, and CPack packages
  that component alone. The `package` workflow now also runs
  `glideslope_cli aircraft c172p` out of each unpacked copy, and checks the
  Linux tarball's licences.

**Tests, 13 now:**

- `the_cli_prints_the_figures_jsbsim_read_from_the_cessna_172_files` — reads
  the wing area, span, chord, empty weight and engine count straight out of
  `c172p.xml`, independently of JSBSim, requires each element's unit to be the
  one the CLI prints in, and compares all five with the CLI's output.
- `the_cli_refuses_an_aircraft_it_has_no_files_for` — exit 1, naming it.

**Watched to fail:** reading wing area from the tail's property failed the
figures test ("the CLI says 21.9, the file says 174"); making the refusal exit
0 failed the refusal test.

**Verified locally:** `linux-release` and the sanitized `linux-debug`, with
JSBSim sanitized too, each pass 13 of 13, with no sanitizer reports. JSBSim
builds in about 19 seconds of wall time. The layering check still passes with
`src/sim/` including JSBSim's headers, and a local package made with
`cpack --preset linux-release` unpacks to a folder whose
`glideslope_cli aircraft c172p` works.

### The living documents, honest, 2026-09-17 — Phase 0 complete

Every living document was re-read in full against the repository at the end of
Phase 0. What that pass changed:

- this file: a summary of the finished phase; the compiler gate's CI evidence
  moved from the warnings entry, where it had landed, to the gates entry; a
  hand check in the packaging entry relabelled as what came before the workflow;
- `RELEASES.md`: what a package is and what each operating system will say. It
  had said there was nothing to download; the workflow now keeps a package per
  platform as an artifact. What macOS and Windows say about a *downloaded*
  copy is stated as expected, not as seen, because no downloaded copy has been
  opened yet;
- `CLAUDE.md` and `README.md`: how to make a package.

`TRANSPORT.md`, `THREATS.md` and `GUIDE.md` still say their subject does not
exist, which is still true. `ASSETS.md` still says nothing third-party is used,
and `ext/README.md` that nothing is pinned; both true. `FEATURES.md` marks
nothing `DONE`, because nothing on it is a thing a player could use yet.

**The verification**, taken literally: a fresh agent with no other context was
given this file alone, told not to open anything else, and asked what works
today, what does not, how the working parts were proved, and what comes next.

**Its answer was right on every point** when checked against the repository and
the CI runs: the CLI's three behaviours and the version-only library; no flight
model, state restore, terrain, renderer or server; each item's proof by platform
and compiler, including that most deliberate breakages were made locally and
that the sanitizer claim rests on one small GCC program; no downloaded package
opened yet; and Phase 1 next.

It also named what it could not tell from the file alone, and this pass fixed
each: the summary listed six things beside "7 of 7"; the CI table had five rows
under "four jobs"; the package workflow's five jobs were not named; "gearstick",
"the brief", "presentation" and "tails" were not explained; and it was not
always clear which proofs were local and which were CI. The corrected file has
not been re-read by a fresh agent.

### Packaging, 2026-09-17

**Proved on every platform.** The `package` workflow's first run on `main`
(35199775868, commit `4b94984`) passed all five jobs — build the Linux tarball,
run it on Rocky 9, run it on Ubuntu 24.04, build and run the macOS tarball,
build and run the Windows zip. Each unpacked
`glideslope_cli --version` printed `glideslope_cli 0.1.0`, matching its file
name, and each checksum verified:

- the Linux tarball, built on Rocky 9, in stock `rockylinux/rockylinux:9` and
  `ubuntu:24.04` containers;
- the macOS tarball, whose program depends on `/usr/lib/libc++.1.dylib` and
  `/usr/lib/libSystem.B.dylib` only;
- the Windows zip, whose program depends on `KERNEL32.dll` only.

CI on the same commit (35199775840) was green in all four jobs.

**Watched to fail.** A throwaway branch removed the static runtime line and the
workflow was run on it by hand (35199809443): the Windows job alone went red,
with `dumpbin` listing `VCRUNTIME140.dll` and the step saying the packaged
program depends on the Visual C++ redistributable. The branch was deleted.

`cpack --preset linux-release` (and `macos-release`, `windows-release`) makes
`glideslope-0.1.0-<platform>.tar.gz` — `.zip` on Windows — with a SHA-256 file
beside it. It holds one folder of the same name with `glideslope_cli`,
`LICENSE` and `README.md` side by side. On Windows the C++ runtime is linked
into the program (`CMAKE_MSVC_RUNTIME_LIBRARY`), so a zip unpacked on a machine
without the Visual C++ redistributable still runs.

`.github/workflows/package.yml` runs on demand and when the build files change:

- **Linux:** built on Rocky 9 with gcc-toolset-14, then run in a stock
  `rockylinux/rockylinux:9` container and a stock `ubuntu:24.04` container,
  neither with a toolchain.
- **macOS:** built, unpacked into a different directory, and run.
- **Windows:** built, unpacked into a different directory, run, and read with
  `dumpbin /dependents`, which must not list `vcruntime` or `msvcp`.

Every run checks the checksum file, and requires `glideslope_cli --version` to
print exactly the version in the package's file name.

Before the workflow first ran, the same was checked by hand on Rocky Linux 10:
the tarball is 28 KB, unpacks to the one folder with its three files, and the
unpacked `glideslope_cli --version` prints `glideslope_cli 0.1.0` and exits 0.

### The simulation links no presentation, 2026-09-17

**Proved on every platform.** CI run 35199507565 (commit `c88b7f0`) passed all
11 tests in every preset, including the binary check reading `otool -L` on
macOS and `dumpbin /dependents` under both MSVC and clang-cl. The check fails
when it finds no dependencies at all, so a pass means each reader understood its
tool's output. Before that run it had been proved on Linux only.

`cmake/Layering.cmake` holds three checks.

**Includes, at configure time.** `glideslope_check_layering` reads every source
under `src/sim/` — `.h .hh .hpp .hxx .inl .ipp .c .cc .cpp .cxx`, recursively —
line by line, and refuses an `#include` of SDL's umbrella or entry-point headers
(3), its video headers (23), its input headers (12) or its audio header, taken
from `include/SDL3` at release-3.4.16, or of anything under `gfx/`, `ui/`,
`platform/` or `frontend/`, with or without leading `../`. The refusal names the
file, the line number and the line, for example
`src/sim/version.cpp:2: #include <SDL3/SDL_gamepad.h>    <- includes SDL
presentation (SDL_gamepad.h)`. SDL's non-presentation headers — threads,
atomics, timers — are not refused by this check. The text is read with `;`,
`[` and `]` swapped out first, because CMake lists split on the first and stop
splitting inside the other two, and C++ is full of all three.

**Links, at configure time.** `glideslope_check_sim_links` walks everything
`glideslope_sim` links — `LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES`,
through aliases and `$<LINK_ONLY:...>`, recursively — and refuses any SDL3
target, library name or library file. It is scheduled with
`cmake_language(DEFER)` so a `target_link_libraries` anywhere later in
`CMakeLists.txt` is still seen. This is stricter than the include check: SDL is
one library, and linking any of it links its video, input and audio code.

**The binary, at test time.** `tests/cmake/binary_dependencies.cmake` reads
the dynamic dependencies recorded in `glideslope_cli` itself and refuses any of
26 Linux, 16 macOS or 18 Windows windowing, graphics, input and audio libraries,
SDL included. It fails, rather than passing, if it finds no dependencies at all,
because that means the reader stopped understanding the tool's output. On Linux
today the CLI depends on `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1` and
`libc.so.6`.

**Tests:**

- `every_forbidden_include_in_the_simulation_fails_the_configure` — 64 cases,
  derived from the lists and counted: every forbidden SDL header (39); every
  layer as `"layer/x.hpp"`, `"../layer/x.hpp"` and `<layer/x.hpp>` (12); every
  scanned extension (10); a subdirectory, `#  include`, and an include after a
  line holding an unclosed `[` and a `;` (3). Each must be refused naming the
  file, line 3, and the include.
- `includes_that_only_look_like_presentation_are_allowed_in_the_simulation` —
  allowed SDL headers, `sim/` and `world/` headers, `gfxtools/`, `uikit.hpp`,
  `platforms/`, a commented-out forbidden include, one in a block comment, and
  one inside a string, all in one file that must be accepted.
- `sdl_linked_into_the_simulation_by_any_route_fails_the_configure` — 7 cases
  in `tests/layering_link/`: SDL linked directly, through a public dependency,
  through a private one, through an interface library, by plain library name,
  and by file path must each be refused; a simulation linking only non-SDL
  targets must be accepted.
- `the_cli_links_the_simulation_and_nothing_presentational` — the binary check
  on `glideslope_cli`.

**Watched to fail, seven ways**, all locally. In the real tree: a
`#include <SDL3/SDL_gamepad.h>` added to `src/sim/version.cpp` stopped the
configure naming `src/sim/version.cpp:2`; a `target_link_libraries` of
`SDL3::SDL3` into `glideslope_sim` appended to the end of `CMakeLists.txt`
stopped it naming the route; and linking the CLI against ALSA failed the binary
test naming `libasound.so.2`. In the checkers: matching only `::SDL3` failed the
library-name and file-path cases; removing the recursion failed the three
indirect cases; removing the `../` normalisation failed all four `../layer`
cases; and removing the `[` escaping failed the unclosed-bracket case.

Verified on Rocky Linux 10 with GCC 14.3.1: `linux-debug` and `linux-release`
each pass 11 of 11 tests, and every configure prints
`src/sim/ includes no presentation (2 files)` and
`glideslope_sim links no SDL`.

### Warnings as errors, 2026-09-17

**Proved on every compiler.** CI run 35198828479 (commit `d437341`) passed the
test below in every preset: GCC 14.2.0 on Ubuntu, GCC 14.2.1 on Rocky 9,
AppleClang 17.0.0, MSVC 19.51 (narrowing failing as C4244, the sign probe
excluded) and clang-cl 20.1.8 (both probes failing, through the conversion
warnings added for it). Before that run it had been proved with GCC and Clang
only.

`cmake/CompilerWarnings.cmake` gives every first-party target, through
`glideslope_configure`:

- GCC and Clang: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wdouble-promotion -Wold-style-cast -Wnon-virtual-dtor
  -Woverloaded-virtual -Wimplicit-fallthrough`, and `-Werror`.
- MSVC: `/W4 /utf-8`, and `/WX`. clang-cl gets the same, plus
  `-Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`, because its
  `/W4` stops at `-Wall -Wextra`.

`GLIDESLOPE_WERROR` defaults to on and is defined in that file, not in
`CMakeLists.txt`, so anything including the file gets the project's default.
Nothing in the flags depends on the build type.

**The test** —
`a_narrowing_or_sign_conversion_fails_the_build_in_every_build_type` —
configures the small project in `tests/warnings/`, which includes the real
`CompilerWarnings.cmake`, with this build's own generator and compiler, once
for each of Debug, Release, RelWithDebInfo and MinSizeRel. In each it builds
three probes: one with explicit casts that must build, one narrowing a double
to a float that must fail naming `float-conversion` (C4244 on MSVC), and one
converting int to unsigned that must fail naming `sign-conversion`. That is 12
probe builds, counted. The sign probe is excluded under MSVC itself, and named
as excluded, because MSVC has no default warning for it.

**Watched to fail, four ways**, locally with GCC 14.3: dropping `-Wconversion`
failed the narrowing probe in all four build types; making `-Werror` apply to
Debug only failed both probes in the other three; dropping `-Wsign-conversion`
failed the sign probe in all four; and a syntax error in the clean probe failed
it in all four.

Verified on Rocky Linux 10: `linux-debug` and `linux-release` with GCC 14.3.1,
and a plain build with Clang 21, each pass 7 of 7 tests, and the project's own
code builds with no warnings under the full set.

### The 64-bit and compiler gates, 2026-09-17

`cmake/Platform.cmake` runs straight after `project()`. It refuses a toolchain
with pointers smaller than 8 bytes, and a compiler older than its floor —
GNU 14, Clang 19 (clang-cl included, which CMake reports as Clang),
AppleClang 16, MSVC 19.39 — with a message naming what it found and what it
needs; for GCC it also says how to enable gcc-toolset-14. A platform outside
Linux x86_64, Windows x64 and macOS arm64, or a compiler with no floor, is
warned about rather than refused. Every configure prints the resolved line, for
example `glideslope: linux-x86_64, GNU 14.3.1 (C++20)`.

The gate is written against `CMAKE_SYSTEM_NAME` and does no `try_compile`, so
it runs in CMake script mode against a toolchain described by `-D` flags. That
is how it is tested, because no machine here has a 32-bit toolchain or an old
compiler to hand:

- `a_32_bit_toolchain_is_refused_on_every_platform` — 3 cases: Linux, Windows
  and macOS, each with 4-byte pointers, each refused with "64-bit only" and
  "4-byte pointers" in the output.
- `every_compiler_is_refused_below_its_floor_and_accepted_at_it` — 8 cases,
  derived from the floor table rather than listed in the test: each compiler one
  version below its floor is refused with the found and required versions named,
  and at its floor is accepted with the resolved line printed. The test counts
  the cases it walked against twice the size of the table.

**Watched to fail, four ways**, locally. Relaxing the pointer check to `LESS 4`
failed all three pointer-size cases. Comparing floors with `VERSION_LESS_EQUAL`
failed every at-floor case. Downgrading the refusal to a warning failed every
below-floor case. Adding a compiler to the table with no platform for the test
to run it on failed with "walked 8 cases, expected 10".

**And against a real old compiler:** CI's Rocky 9 job configures once with the
system's own `g++` and requires the configure to fail with "required: GNU 14 or
newer" before building with gcc-toolset-14. In CI run 35198316459 (commit
`7921839`, green in all four jobs) that compiler was `g++ (GCC) 11.5.0`, refused
with `found: GNU 11.5.0`.

The same commit makes the later preset steps in each CI job run even when an
earlier one failed, closing the gap the deliberately red run showed.

Verified on Rocky Linux 10 with GCC 14.3.1: `linux-debug` and `linux-release`
each pass 6 of 6 tests.

### CI, and every preset on its own platform, 2026-09-17

`.github/workflows/ci.yml` runs every preset on the platform it belongs to, in
four jobs — Ubuntu, Rocky 9, macOS and Windows; the Windows job builds with two
compilers, which is why the table has five rows. Its first run on `main` (run
35197837005, commit `f5e3890`) was green in all four, each preset passing 4 of 4
tests:

| Job | Presets | Compiler |
| --- | --- | --- |
| Ubuntu | `linux-debug`, `linux-release` | GCC 14.2.0 |
| Rocky 9 container | `linux-release` | GCC 14.2.1 (gcc-toolset-14) |
| macOS 15 | `macos-debug`, `macos-release` | AppleClang 17.0.0 |
| Windows | `windows-debug`, `windows-release` | MSVC 19.51 |
| Windows | `windows-clang` | clang-cl 20.1.8 |

`linux-debug` runs on Ubuntu only, not in the Rocky job: the preset is the
same, and Rocky 9's job exists for its compiler and its distribution.

**CI was watched to fail.** A throwaway branch changed the expected version
string in one test and CI was run on it by hand (run 35198071126): all four
jobs went red, each naming the failed test and the expected string. The branch
was deleted afterwards. That run also showed a gap: a failing preset step
skipped the presets after it in the same job, so a debug failure hid whether
release passed.

`windows-*` steps run under `cmd`, where a failing command does not stop the
script; each command ends in `|| exit /b 1`, which is what made the red run red.

### The build and its presets, 2026-09-17

When this landed, only the Linux presets had run; the macOS and Windows presets
were first run by CI, in the entry above.

**What exists.** A C++20 CMake project, `LANGUAGES CXX` only, with Ninja presets
`linux-debug`, `linux-release`, `macos-debug`, `macos-release`,
`windows-debug`, `windows-release` and `windows-clang`, and a test preset for
each that fails when it finds no tests. C++20 because Cesium Native's own build
requires it.

- `glideslope_sim` — a static library holding `glideslope::sim::version()`,
  which returns the project version compiled in. It is the simulation in name
  only until JSBSim arrives.
- `glideslope_cli` — `--version` prints `glideslope_cli 0.1.0` and exits 0;
  `--help` prints usage to standard output and exits 0; no arguments, or any
  other argument, prints usage to standard error and exits 2.
- The debug presets on Linux and macOS build with
  `-fsanitize=address,undefined -fno-sanitize-recover=all`, from
  `cmake/Sanitizers.cmake`. The no-recover flag is the important one: a small
  signed-overflow program built with GCC 14 without it prints the
  undefined-behaviour report and still exits 0; with it, exits 1.
  `windows-debug` does not sanitize.

**Tests, four, covering every way the CLI can be called today:**
`the_cli_reports_the_project_version`, `the_cli_prints_its_usage_when_asked`,
`the_cli_with_no_arguments_prints_its_usage_and_fails` and
`the_cli_refuses_an_argument_it_does_not_know`. Each runs the CLI through
`tests/cmake/expect_run.cmake`, which checks the exit code *and* the output —
ctest's own `PASS_REGULAR_EXPRESSION` ignores the exit code. Every kind of check
the script makes was watched to fail: a wrong version string, a wrong exit code,
standard error not matching, and standard output not matching each end the
script with an error.

**Verification run.** On Rocky Linux 10 in WSL with GCC 14.3.1, CMake 3.31 and
Ninja 1.11: `linux-debug` and `linux-release` each configure, build and pass
4 of 4 tests. The sanitizer flags appear in every compile command of
`linux-debug` and in none of `linux-release`. The same tree also builds and
passes with Clang 21. `ldd glideslope_cli` lists the C++ runtime, libm, libgcc
and libc, and nothing else.

### The living documents, 2026-09-17

`CLAUDE.md`, `docs/FEATURES.md`, `docs/COMPLETION_PLAN.md`, this file,
`docs/TRANSPORT.md`, `docs/THREATS.md`, `docs/ASSETS.md`, `docs/GUIDE.md`,
`docs/RELEASES.md` and `ext/README.md` now exist, following gearstick's shapes.
The plan's phases are the ones `docs/REQUIREMENTS.md` section 7 starts from,
with the decisions of 2026-09-17 folded into their items. Five tails were
written down on the way: runways on the DEM, free buildings, macOS signing, a
single Linux download, and a hosted public server.

The executables are named `glideslope`, `glideslope_cli` and
`glideslope_server`, after the project as gearstick's are; the brief's
`flightsim_*` names were examples, and `REQUIREMENTS.md` now uses the real ones.

The documents whose subject does not exist yet — the transport, the threat
model, the player's guide, the releases — say so in one paragraph each rather
than describing plans as if they were facts.

The Phase 0 item for these documents stays unticked until the rest of Phase 0
has landed and the documents have been re-read against it.

### Requirements and licence, 2026-09-17

`docs/REQUIREMENTS.md` is the design brief in Markdown, with section 9 recording
the decisions closed and still open. `LICENSE` is the unmodified GPL-3.0 text;
the project is GPL-3.0-or-later.

---

## Known risks

- **JSBSim state set/resume.** Reconciliation needs to put an instance into a
  full captured state and have it fly on cleanly. Proved or disproved in
  Phase 1, before any networking is built on it.
- **The Cesium-to-SDL_GPU glue.** The biggest rendering risk; Phase 2.
- **Reading the Copernicus DEM directly.** Cesium Native streams only
  quantized-mesh terrain and 3D Tiles, so the open-data provider depends on a
  GeoTIFF reader this project writes; Phase 2.
- **Runway surfaces.** The DEM is a surface model sampled every 30 m, so
  runways carry bumps that are not there. Open in `REQUIREMENTS.md`.

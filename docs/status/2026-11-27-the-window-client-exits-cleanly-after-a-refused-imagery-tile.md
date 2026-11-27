### The window client exits cleanly after a refused imagery tile, 2026-11-27

**What is still not true first.**
- **The fix is still ours, around Cesium Native's bug, not in it**: the 504
  layer of 2026-10-30 (`gfx/fetches.cpp`, `docs/cesium-issues.md` Issue 4,
  drafted, not posted). Nothing new was changed in the client.
- **Measured on Linux alone.** The live runs below are linux-release on the
  development machine; the new tests run on every platform's CI.

**The 2026-10-10 crash is now shown to be the 2026-10-30 one.** That item
was left open because the link was taken, not proved. It is proved here, on
the 2026-10-10 command itself - `glideslope --headless --gpu-driver vulkan
--size 960x540 --aircraft A --at -33.9461,151.1772,60 --on-ground --view left
--shot-at 240`, at Sydney, live Sentinel-2 imagery - over a copy of the
development machine's Cesium cache with every entry made stale (3,310
entries, 643 of them EOX tiles):

| build | Cessna 172P | Learjet 35A | tiles "given up, unfinished" a run |
|---|---|---|---|
| today's `main` | 10 of 10 exit 0 | 10 of 10 exit 0 | 2 to 8, every run |
| `main` less the 504 layer | 10 of 10 exit 139 | 10 of 10 exit 139 | 1 to 5, the crash at the first stale one |

Each crashed run wrote its frame first, then segfaulted at exit as the first
stale tile's revalidation was given up: the 2026-10-10 report exactly. So
the cause is Cesium Native's `CachingAssetAccessor` reading the status of a
revalidation that came back with no response (v0.64.0, line 267), and the
2026-10-30 fix, which answers such a fetch with a 504, is what fixed it. The
week-long `max-age` of EOX's tiles is why it came and went: a cache over a
week old crashed, a new one did not.

**Searched before concluding** (2026-11-27): Cesium Native's caching
accessor and null responses on revalidation; Tileset destruction order,
`getAsyncDestructionCompleteEvent` and `dispatchMainThreadTasks` at
shutdown; the community threads on crashes destroying a tileset mid-load and
v0.55.0's fix of an overlay destroyed before its last tile. Nothing pointed
at another cause, and the teardown (`~TerrainTiles`: transfers given up,
the destruction event waited on with main-thread tasks dispatched, the
workers drained and joined) predates both crashes unchanged.

**The test, the item's verification.**
`a_shot_of_the_c172p_on_the_ground_with_its_stale_imagery_refused_writes_its_frame_and_exits_0`
and its `learjet35a` twin (`tests/cmake/imagery_stale_refused.cmake` with
`AIRCRAFT`): the 2026-10-10 shot, each aircraft standing on the ground seen
from the left, its frame at tick 240, at Mount Taranaki (whose DEM the tests
download), its imagery asked of a port nothing listens on. A first shot
names the tiles it asked for; each is stored stale in its cache; the second
asks for them again, is refused, and must write its frame and exit 0, and
must have asked again for at least one (locally 27 of 27). Both aircraft the
item names, two of two. **Seen red** with the 504 layer taken out: both
"the second shot exited Segmentation fault", as did the existing terrain
shot's test. Reverted.

A refusal and a give-up reach the cache alike - each is a fetch with no
response - and `a_stale_cached_tile_whose_fetch_gets_no_answer_is_answered_as_a_failure_and_ends_nothing`
already holds the chain to both. A window-client test of a give-up was
tried and not built: a fetch held open keeps the open provider's shot
waiting for ever, so the give-up can only come from a signal, not from the
shot ending as here.

**CI costs are estimates**: each table prices the two at eight times the
existing stale shot's figure, the ratio measured locally (16.2 and 17.7 s
against its 2.2, linux-release, `ctest -j4`). They should be measured again
from a green run.

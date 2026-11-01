### A tileset the provider refuses is said to be refused, 2026-11-02

Phase 5b's new item, ticked: the Google terrain test's intermittent local
failure, "google via ion-token drew no terrain at all", is found and fixed
at its cause. It was never a wait too short.

**The cause.** Google's Photorealistic 3D Tiles reached through Cesium ion
(asset 2275207) are fetched with ion's own Google project's key, and that
project's root requests are limited to 150 a minute (Google's 429,
RESOURCE_EXHAUSTED, quota limit `ThreeDTilesRootTileSetPerMinutePerProject`,
value 150, seen with curl on 2026-11-02) - shared by every ion user in the
world, so when it is refused depends on nobody here. Cesium Native then
reports "Received status code 429 for tileset" through its log and its
`loadErrorCallback`, and loads nothing, ever. The shot waited out its 45 s
for tiles, drew an empty sky and exited 0, and the test read "terrain of 0
tiles". It passed alone and failed in full suites because a full suite
makes more root requests in a minute (each terrain test, and the other
working copies' suites on the same machine), not because of load on the
machine. The way in with the user's own Google key is the user's quota and
was never refused.

**What changed.**
- `TerrainTiles` (src/gfx/terrain_tiles.cpp) keeps the tileset's refusal
  from `loadErrorCallback`, with its HTTP status read from Cesium Native's
  message (its `statusCode` is left at 200 for the root, cesium-native
  v0.64.0) and nothing else of it - the message names the URL, and a URL to
  Google carries a key. The streamed provider's settling ends on that event
  rather than running out its 45 s, and a shot of a refused tileset is not
  drawn: the run fails with "could not download the google terrain's
  tileset: the provider answered 429, its quota of requests spent for now".
  Only a 429 or a 503 is worded so - the two Google's Map Tiles API error
  handling page says to try again later. Any other failure of the root (a
  401 or 403 for a key not taken, a 404, a root that could not be read, or
  a status that could not be read from the message) fails the run as "the
  ... terrain's tileset failed to load", which no test skips on. The open
  provider, whose tileset is built here, is not held to this.
- tests/cmake/terrain_provider.cmake matches that exact refusal text, in the
  main shot and in each way in, and the DEM's own download failure
  (copernicus-dem-*, or no answer) apart from it: bare "could not download"
  no longer skips. A way in to Google that the provider
  refused is reported, the other way in still run and held to everything,
  and the test then reports itself **skipped**, with the refusal printed -
  never passed, as before it would have been with one way in skipped.
- glideslope_ion_stall gains `quota`: a stand-in for ion and Google on the
  loopback that answers asset 2275207's endpoint with itself and its root
  with Google's 429.

**Verification.**
- `a_google_terrain_shot_refused_for_quota_draws_no_frame_and_says_so_on_vulkan`,
  against that stand-in, built on purpose. **Seen red**: with the
  terrain_tiles.cpp change reverted it failed "refused for quota, the run
  still exited 0 and drew a frame", having taken 45 s and drawn "terrain of
  0 tiles" - the failure as the suites saw it. With the change it passes in
  0.5 s, the stand-in having refused one request. It skips only on the
  DEM's own download failing; a refusal worded any other way fails it.
  **Seen red** the other way too: the stand-in made to answer 403 instead,
  the test failed in 0.4 s, the run saying "the google terrain's tileset
  failed to load: the provider answered 403"; reverted.
- The real tests, ten rounds of `the_google_terrain_*`, `the_ion_terrain_draws_*`
  and the new test at -j3, beside another working copy's full linux-release
  suite at -j4 (load average 9 to 20): the Google terrain test passed 8
  times, 241 tiles through each way in, and **twice ion's Google root was
  refused with a 429 for real** - the fifth round of each set of five - and
  the test reported itself skipped, printing the refusal. No round failed.

**Still not true.** Cesium Native's own log still prints a refused or failed
tile's URL to standard error, and a Google URL carries the key it was asked
with - the user's own, on the Google-key way in. A tail in Phase 14b.

Sources: Google's Map Tiles API error handling (429: quota exhausted) and
usage pages; Cesium community, "Lots of 'Received status code 429 for tile
content' errors" and "Google Photorealistic 3D Tiles - Randomly not
showing", where ion's Google quota is said to be shared by all ion users.

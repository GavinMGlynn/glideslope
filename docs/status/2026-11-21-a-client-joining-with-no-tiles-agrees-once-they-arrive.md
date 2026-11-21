# 2026-11-21: a client joining with no tiles cached agrees once they arrive

Phase 12a, "One end fetching a tile the other has differs by the lift
there": done.

**What is still true first:** until the tile arrives, the two ends still
differ by the lift there. This item measures that window and pins agreement
after it. It does not shorten the window, and nothing waits for the tile.
Over the test's hills, fetching the one 20.9 MB Copernicus tile from its S3
bucket took 4.9 s here. The client had the tile 2.9 s after its first update
was compared. Over those 73 updates its median error was 0.067 m, against
0.035 m once the tile was had. The window lasts as long as the fetch, and a
slow link makes it longer.

## What now works

A predicting client (`glideslope_cli connect --predict`) says what its air's
ground fetched and when: how many tiles, how long the fetches took, the
session's time at the first update compared once every tile asked for had
arrived, and its median error before that moment and after it. The line is
"the air's ground: N tiles fetched in S s, F failed, W waiting". The moment is
`note_ground_had`: at least one tile fetched and none still waiting
(`world::TilesAhead::waiting()`, `fetching_s()`).
`--until-compared-on-ground N` keeps it until N updates have been compared
after that moment, so a test waits on the event and never on a time.

**The DEM's tiles may be fetched from a stand-in, for a test**
(`GLIDESLOPE_DEM_SOURCE`, `platform::dem_source()`), as the runways already
could be (`GLIDESLOPE_RUNWAYS_SOURCE`). The stand-in takes the bucket's scheme
and host and keeps the same path. **What is guaranteed, and what is not:** the
stand-in must be on this machine: http or https to 127.0.0.1, [::1] or
localhost (`world::dem_source_allowed`); any other host is refused, by name.
A tile from it must arrive with a plain MD5 as its ETag, and the bytes must
match it. A missing ETag is refused, and so is a dashed one, which S3's
multipart form uses and the bucket's own fetch passes over unchecked. That
ETag comes from the stand-in itself, so it shows the tile arrived whole, not
that it is Copernicus's: a stand-in serves whatever tile it is given. So
every program says so as it prepares its ground: "the DEM's tiles are fetched
from GLIDESLOPE_DEM_SOURCE, ..., not the Copernicus bucket". The client
prints it as it prepares its ground, the server after its key, and the
window client as it builds its flight. `glideslope_http_stub --etag TAG`
serves files with an ETag, as S3 does.

## Verification

- `a_client_joining_over_hills_with_no_tiles_cached_flies_the_servers_ground_lift_within_the_seas_bound_once_they_arrive`
  (`tests/cmake/server_tiles_arrive.cmake`) flies server_ground_lift.cmake's
  scene: the Cessna at 1,200 ft over the hills north of the Hawkesbury in a
  50 kt southerly, two predicting clients each through 250 ms relays. The
  first client's cache is made new for each run, with the geoid and no tile.
  Its tile comes from the stub on the loopback, which serves the downloaded
  copy once with its MD5 as the ETag and refuses everything else. The test
  checks that the client fetched one tile and failed none. The client then
  stays until 300 updates have been compared after the tile arrived. Over
  those updates its median error must be within the sea's 0.25 m and under
  0.5 of the second client's, which flies the same air over no ground. Here
  (debug build) the tile took 0.21 s over the loopback and arrived before the
  first comparison. The median after was 0.038 to 0.041 m over 300 updates,
  against 0.096 to 0.114 m over no ground: 0.36 to 0.40 of it. **Seen red** with the client's ground lift thrown
  away after its tile was had (the tile still fetched, the lift never flown):
  0.074 m against 0.114 m, 0.65 of it. That failed the 0.5 share and would
  have passed server_ground_lift.cmake's 0.7 (0.070 m, 0.61, the first try),
  so this test holds 0.5. Reverted.
- **Against the real bucket, by hand** (the same script, with
  `GLIDESLOPE_DEM_SOURCE` set to `https://copernicus-dem-30m.s3.amazonaws.com`
  and the stub's count not checked): the tile took 4.94 s. The client had it
  at 6.48 s on the session's clock, 2.92 s after its first update was
  compared. The median error was 0.067 m over the 73 updates before it and
  0.035 m over the 300 after; 0.114 m over no ground. The test does not use
  the network for this. The downloads fixture already fetches the tile, and a
  fetch over the internet would make the window's length the network's.
- `a_dem_tile_may_be_fetched_elsewhere_for_a_test_and_is_still_held_to_its_etag`:
  with the source unset, a tile comes from the bucket. Set to the loopback
  (127.0.0.1, [::1] and localhost, with and without a port), the stand-in is
  asked for the bucket's path. A tile that does not match its ETag is
  refused, and one that matches is kept. From a stand-in, a dashed ETag and a
  missing one are each refused, and neither tile is kept. Every host off the
  loopback, over https too, is refused by name (example.org over http and
  https, a look-alike of the bucket, `127.0.0.1.example.org`, 10.0.0.1).
  **Seen red** before the fix (from the review of #395): `https://example.org`
  was accepted, and so was a dashed ETag from the stand-in. A missing ETag was
  already refused before, so that case was never seen red.
- The new test also checks that the fetching client said its tiles came from
  the stand-in.

`test_server_timeouts.cmake`'s totals: 75 uses, 63 long (the new test's
server).

**Built on Windows** before each push: `tools/windows_build.sh windows-debug`
(under `flock /tmp/glideslope-windows.lock`) built the tests, the cli, the
server and the window client with MSVC, after the review's fixes too.

**Found on the way:** windows-clang's shards were already close to full, and
the new test pushed a night's run over the 75% limit.
`every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
failed on draft run 44894429415 with 33.9 of the 45 minutes in each of the
5 shards. windows-clang now has 6 shards (`.github/workflows/ci.yml`).

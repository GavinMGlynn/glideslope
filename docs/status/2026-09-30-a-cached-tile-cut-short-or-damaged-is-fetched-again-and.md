### A cached tile cut short or damaged is fetched again and read whole, 2026-09-30 — tail done

**What is not covered first.** A tile that is damaged yet still decodes is
not found: a DEM tile has no digest kept beside it, so what is checked is
that it decodes - its header and blocks inside the file, and each block's
zlib stream passing its Adler-32 - and a change the checksum misses (1 in
2^32) or one in an uncompressed tile (the buckets publish none) is read as it
is. A tile's blocks are checked as they are read, not all at once, so a
damaged block is found by the first query that needs it. **A tile bad at the
bucket** (a multipart upload's, which no ETag checks) is fetched again once
per Dem - once per program that reads the ground, and once more for each
Dem it makes - and then fails every query of it until that Dem is gone.
**Two processes that find one file wanting at once**: each takes away only
the file it read, by its identity (device and file number), so a good copy
the other has put in place since is left there; what remains is the moment
between looking at the name and renaming it, in which a copy put in place
is taken away too - and fetched again, the cost a fetch. Nothing can
simulate the power cut itself: that a file reaches the disk before its name
is read from the code, not from a test. The directory's sync on POSIX is
asked and not insisted on.

**What changed.**
- **A download is durable before it is moved into place.** `put_in_place`
  writes through new `write_durably` (`world/byte_source`): the file made,
  written, and flushed through the system's cache - `fsync` on Linux,
  `fcntl(F_FULLFSYNC)` on macOS (whose `fsync` leaves the data in the drive's
  own cache; `fsync` where the filesystem cannot), `FlushFileBuffers` on
  Windows - before `move_into_place_unless_there` gives it its name. The move
  is durable too: `MOVEFILE_WRITE_THROUGH` on Windows; on POSIX the directory
  is synced after the link or rename, best effort, since some filesystems
  refuse to sync a directory, and a name lost to a power cut is a file
  fetched again, never a file cut short. A Windows write that writes nothing
  and says no error says so in its own words.
- **A tile or water mask that cannot be read whole is taken away and fetched
  again, once per tile.** `DemTiles` gains `take_away` and
  `take_away_water_mask` (false by default: a directory's tiles have nowhere
  to come from anew); `DownloadedTiles` takes the file out of the cache and
  says true. The Dem asks it when a tile it opened does not read - its layout
  refused, as a file empty or cut short is - and when a block does not
  decode, as a damaged one does not; then it lets the file go, forgets every
  block read from it, opens the tile anew (fetching it) and reads it again.
  **The Dem remembers every tile it has taken away**: one found wanting again
  - in the same query, by its layout or a block, or in any later one - fails
  at once, saying it was had anew as it was, with the first reason; it is
  never fetched again by that Dem. A tile that could not be had at all (a
  failed fetch) is not taken away.
- **Taking away frees the name at once, and takes only the file read**: new
  `take_away` compares the identity of the file at the name with the one
  read (`FileSource::identity`, new `identity_of`: `st_dev` and `st_ino`, or
  the volume serial and file index) and leaves another alone; then renames
  the file to a name of its own beside it and removes that, so on Windows a
  reader still holding it (FileSource shares deletion) cannot leave the name
  delete-pending for the fetch that follows. On Windows the rename waits out
  a refusal that passes, as every other move there does.
- **A pinned file is checked against its pin each time it is loaded**
  (`fetch_pinned`: the geoid, the runways): read whole and hashed, and one
  that is not what was pinned - empty, cut short or damaged - is taken away,
  if it is still the file read, and fetched again. The cost, measured on the
  development machine: 0.09 s for the geoid's 17 MB at -O2, 0.43 s in the
  sanitized debug build, each time the geoid is loaded - a flight
  (`Flight::Flight`) and the terrain (`Ground`, `client/terrain.cpp`) each
  load it - beside the geoid's own inflating.

- **The runways' source can be named for a test**:
  `GLIDESLOPE_RUNWAYS_SOURCE` (`platform::runways_source`, read by
  `world::runways_host`), a scheme and host put in place of
  `https://raw.githubusercontent.com`, allowed as a weather service's
  stand-in is - https, or http to the loopback - and the file still held to
  its pinned SHA-256 wherever it comes from. #62's
  `an_ai_aircraft_whose_runways_cannot_be_read_flies_the_plan_file_and_the_server_goes_on`
  built "OurAirports unreachable" from a damaged cached copy alone, which is
  now fetched again: on CI it fetched the real runways and planned the
  aircraft ("the aircraft was not said to be unplannable", every platform,
  on e216e07). It now points the runways at `http://127.0.0.1:1`, where
  nothing listens, beside the damaged copy, and expects "could not download
  ... 127.0.0.1:1" in the message; its five refused tries wait about 30 s
  between them, so it takes 31 s, and its CI costs say 32.

**Verification.** Tests in `tests/unit/test_download.cpp`, each building its
fault explicitly and a fake bucket, with no network:
- `the_runways_may_be_fetched_elsewhere_for_a_test_and_are_still_held_to_their_pin`:
  unset, GitHub; set to the loopback, the fetch goes there and a file that
  is not the pinned one is refused and not kept; plain http to another
  machine is refused by name. With the variable ignored (a deliberate bug)
  it failed, "the fetch goes where the test says: https://raw.githubusercontent.com/...",
  and so did the server test, as on CI: "the aircraft was not said to be
  unplannable ... AI 1 planned by openai". Reverted, both pass (Linux
  debug; the server test 31 s).
- `a_cached_tile_cut_short_or_damaged_is_fetched_again_and_read_whole`:
  both datasets (glo30 at Sydney, glo90 in Armenia, each in its own cache
  directory), both layers, and three faults - empty (the commonest power-cut
  result), cut short at its last block's first byte (both checked to be
  refused as they are opened), and a byte of the first block changed
  (checked to open and have that block refused) - 12 cases, counted: each
  query answers the whole file's value, the file is fetched once from its
  own URL, is whole in the cache after, and nothing set aside or
  half-written is left.
- `a_tile_bad_where_it_is_fetched_from_is_fetched_again_once_however_often_it_is_asked_for`:
  each of the three faults in the cache with a bucket serving it cut short or
  damaged under a multipart ETag - 6 cases, counted, among them damaged in
  the cache and cut short at the bucket, where one query finds a block bad
  and then the fresh copy's layout; two queries each, both failing "had anew
  as it was", and the bucket asked once. Left out, and named in the test: a
  bucket answering an empty 200, which `fetch_with_retries` itself asks
  again.
- `a_tile_that_cannot_be_fetched_is_not_taken_away_or_fetched_twice` (a 404:
  asked once).
- `a_bad_tile_in_a_directory_fails_its_query_and_is_left_as_it_is` (three
  faults, counted: the reason given, no retry, the file untouched).
- `a_file_put_in_place_by_another_since_it_was_read_is_not_taken_away`.
- `a_cached_pinned_file_cut_short_or_damaged_is_fetched_again_and_read_whole`
  (three faults, counted, and a whole one is not fetched again).

Linux debug (sanitized): all six pass, and the inflate, GeoTIFF, zip, geoid,
DEM, digest, download and ground tests around them (ctest `-I 305,365`
without the survey and visual-model tests, 62 tests) pass. **Seen to fail**,
each deliberate bug introduced and reverted: the Dem not taking away a tile
that does not open (the tile test: "a height tile cut short was not fetched
again ... a read past the end of the file"); not taking away one whose block
does not decode ("... fails its Adler-32 checksum"); the per-call guard of
the first version gone (recursed until it died, 26 s); the Dem not
remembering the tiles it took away ("a tile empty in the cache and cut
short at the bucket: fetched again once over two queries, not 2 times");
the guard for a tile never opened gone ("asked once, not 2 times"); the
tiles' default saying yes, as if a directory could have a tile anew ("a
tile empty in a directory fails its query with its reason, not: ... had
anew as it was"); `take_away` not comparing identities ("the other's copy
is not taken away"); glo90 put in the 30 m directory ("glo90 height tile
empty is whole in the cache after"); `fetch_pinned` not checking a cached
file ("a pinned file cut short is fetched again, once").

**Windows debug** (`tools/windows_build.sh`, the development machine, at
b6c6334, the review's fixes): the six tests pass with the ten download tests
around them (delete-pending, unshared, put-in-place, the refusal that stays,
many at once) - 16 of 16 on the third run. The first two runs each had one
test program crash on its way out after printing that it passed (0xc0000409,
a thread Windows started after exit; `a_dem_tile_is_fetched_once...` and
`a_bad_tile_in_a_directory...`), and several take 37 s to end: the open tail
"Windows debug test programs crash on their way out on the development
machine", which is on `main` as well. **Flushing costs**:
`many_fetches_and_reads_of_one_tile_at_once_all_read_it_whole` took 11.3 s
there on `main` (0c39005) and 13.1-13.7 s with every write flushed (6.1 s on
Linux debug). Its CI costs are raised to the development machine's 14 s for
windows-debug and a safe 10 s for windows-release and windows-clang (CI
measured 3-4 s before; neither is measured on CI since), to be measured
again with `tools/ci_test_costs.py`.

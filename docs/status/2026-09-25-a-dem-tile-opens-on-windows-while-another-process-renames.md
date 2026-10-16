### A DEM tile opens on Windows while another process renames it into place, 2026-09-25 — tail done

**What is not covered first.** Only files read through `FileSource` open
sharing deletion. On `main` those are the only files fetched and then read -
DEM tiles, water masks and the geoid - and every other reader
(`std::ifstream` of plans, coverage lists, key files, the aircraft catalogue)
reads a file nothing renames into place, so none of them can meet this. Any
file fetched into the cache and read while another process may be renaming it
must be read through `FileSource` or share deletion itself: the runways file
on the unmerged #18 has this shape and is being fixed there. The stress test
races threads in one process, not processes: Windows checks sharing per open
handle, not per process, and the threads reproduced CI's failure exactly.

**Found by CI**: `cannot open ...S34_00_E151_00_DEM.tif` on Windows.
**Cause.** A fetched file is written under a name of its own and renamed into
place. A rename opens the file it moves with DELETE access until the move is
done, and on Windows an open that does not share deletion fails against it
with a sharing violation. `std::ifstream` on MSVC opens through `_wfsopen`
with `_SH_DENYNO`, which shares reading and writing but not deletion - so a
reader that saw the file appear and opened it that moment was refused.
`fetched` checks the file exists, then opens it, which is exactly that moment.

**Now** `FileSource` opens with `CreateFileW` sharing read, write and delete,
and reads with a positioned `ReadFile`; on POSIX, `open` and `pread`. Reads
say their own offset, so the mutex a shared stream needed is gone. The writer
is unchanged: it still leaves a file already in place alone, and a rename that
cannot replace a file a reader holds still counts as done when the file is
there.

**Verified** first by `a_tile_held_open_for_deletion_as_a_rename_holds_it_is_still_read_whole`,
which builds the moment rather than waiting for it: it holds a tile open with
DELETE access, sharing everything, as a rename does, and requires
`FileSource` to open it and read it whole. On POSIX it reports itself
skipped, since POSIX has no sharing modes. **Seen to fail on Windows** with
the old `std::ifstream` opening (`cannot open ...tile.tif`), and pass with the
fix. Then, as a second layer,
`many_fetches_and_reads_of_one_tile_at_once_all_read_it_whole`:
200 rounds, each from an empty cache, of sixteen threads started together on a
barrier - four fetching the tile as the terrain does, four fetching it as a
pinned file as the geoid is, to the same path, and eight waiting for it to
appear and opening it that instant. Every one reads the whole 256 KiB and
compares it; the test counts 800 + 800 + 1600 reads and fails if any kind
falls short. **Seen to fail on Windows** (windows-debug, the development
machine) before the fix: 1396 of the 3200 threads failed, the first with CI's
own message, `cannot open ...Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif`.
Passes with it on Windows and Linux. On Linux it never failed, as expected:
POSIX has no sharing modes. A reader gives up only if every fetcher had
finished before it last looked for the file, so a fetcher finishing between
the look and the count cannot fail it spuriously.

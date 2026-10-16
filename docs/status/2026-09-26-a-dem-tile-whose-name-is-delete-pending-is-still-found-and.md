### A DEM tile whose name is delete-pending is still found and read on Windows, 2026-09-26 — tail done

**What is not covered first.** Only the cached downloads - DEM tiles, water
masks, the geoid, the runways - wait out a refusal that passes; the other
files the programs read (plans, coverage lists, key files, the catalogue) are
not fetched into place while they are read, and still use
`std::filesystem::exists` and `std::ifstream`. A name still refused after
`transient_refusal_wait` (3 s) - a file somebody keeps open after deleting it
the old way, or one its ACL denies - still fails, and the message gives the
Windows error, the tries and the time. On a POSIX filesystem with neither
hard links nor an exclusive rename, a second writer replaces the first's
file - harmless there, and not built on Windows. A cached file cut short by a
power cut is not re-fetched: a new tail (done 2026-09-30, above).

**Found by CI** on windows-clang, 2026-09-26: 1 of 3200 threads in
`many_fetches_and_reads_of_one_tile_at_once_all_read_it_whole` failed with
`exists: Access is denied.: "...\copernicus-dem-30m\Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif"`,
after the fix of 2026-09-25 below.

**Cause.** Two fetchers that both found no file both renamed their copy into
place, and `std::filesystem::rename` on Windows is `MoveFileExW` with
`MOVEFILE_REPLACE_EXISTING`: the second replaced the first's file. A file
replaced, or deleted, while a handle to it is open is *delete-pending*, and
until the last handle closes its name answers every look and every open with
`STATUS_DELETE_PENDING`, which Win32 reports as `ERROR_ACCESS_DENIED` -
and `std::filesystem::exists`, which only takes "not found" for an answer,
throws it. The window is short, which is why one thread in 3200 met it.
(Windows 10's POSIX rename and delete semantics free the name at once, but
nothing obliges `MoveFileExW`, another process, or a virus scanner to use
them.)

**Now**, two things, in `world/byte_source` beside `FileSource`.

- **The writer never replaces** where the filesystem can refuse to.
  `put_in_place` (was `write_whole`, now in `world/download.hpp`) moves its
  own temporary file with `move_into_place_unless_there`: on Windows
  `MoveFileExW` without `MOVEFILE_REPLACE_EXISTING`; on POSIX `link` then
  `unlink` of the temporary name, and a failed `unlink` is said, not
  ignored. Where `link` is refused as a filesystem without hard links refuses
  it (EPERM, EOPNOTSUPP, ENOTSUP, ENOSYS: vfat, exFAT, SMB, FUSE), the
  exclusive rename - `renameat2` with `RENAME_NOREPLACE` on Linux,
  `renamex_np` with `RENAME_EXCL` on macOS; where that is refused too (the
  same, or EINVAL), plain `rename`. A second writer finds the name taken in
  the same step and leaves the first's file alone; the look before the move
  is gone.
- **Waiting out a refusal that passes.** `world::file_is_there` replaces
  `std::filesystem::exists` for every cached download (`DownloadedTiles`,
  `fetch_pinned`, `DirectoryTiles`). It, `FileSource`'s open and the move ask
  again on `ERROR_ACCESS_DENIED`, `ERROR_SHARING_VIOLATION` and
  `ERROR_DELETE_PENDING`, a millisecond's sleep apart, until
  `transient_refusal_wait` - **3 s, by `steady_clock`** - has gone by. Bounded
  by time, not tries: a millisecond's sleep is 15.6 ms on Windows' default
  timer, and **measured on windows-debug a refusal that stays is asked 194
  or 195 times and given up on after 3000 to 3019 ms**, by each of the look,
  the open and the move. A move onto a delete-pending name is answered
  `ERROR_ALREADY_EXISTS`, so the move then asks `file_is_there`, which waits
  the name out, and moves again if it came free. So another process - an
  older build that did replace, a scanner, an indexer - cannot fail them
  either. `transient_refusals_waited()` counts the refusals waited out, all
  three together, for the tests to wait on.

**Verified**, every Windows state built rather than waited for: a handle
holds it, the call is started, and the handle is let go only once
`transient_refusals_waited` says the call is waiting.

- `a_cached_file_whose_name_is_delete_pending_is_waited_for_then_fetched_and_read_whole`:
  for each of a DEM tile, its water mask and a pinned file, an old file at
  the name is marked for deletion (the old `FileDispositionInfo`, not
  POSIX), the name is asserted to refuse a look with `ERROR_ACCESS_DENIED`,
  and the fetch must download once and read the file whole; 3 of 3 covered.
  **Seen to fail on Windows** with the fetchers looking with
  `std::filesystem::exists` again, with CI's own message.
- `a_file_another_holds_open_without_sharing_is_opened_once_it_lets_go`: a
  file held with no sharing; `FileSource` must wait, then read it whole.
  **Seen to fail** with the open asked once (`Windows error 32 after 1 try`).
- `a_file_put_where_a_name_is_delete_pending_waits_for_the_name_then_takes_it`:
  `put_in_place` onto a delete-pending name must wait, then put the file
  there, whole, leaving no temporary name. **Seen to fail** with a refused
  name taken for a taken one ("the move waited while the name was
  delete-pending").
- `a_file_held_unshared_while_it_is_moved_into_place_is_moved_once_let_go`:
  the file to be moved held without sharing deletion, which refuses the move
  with `ERROR_SHARING_VIOLATION`; the move must wait, then move it. **Seen to
  fail** with the move asked once (`Windows error 32 after 1 try`).
- `a_refusal_that_stays_is_given_up_on_after_its_stated_wait`: with a name
  delete-pending throughout, each of the look, the open and the move gives
  up, no sooner than the stated wait, saying the tries and the time, and
  prints how long it took (the figures above); 3 of 3 covered.
- `where_the_filesystem_has_no_hard_links_a_file_is_still_moved_into_place`
  (POSIX): `link` refused with each of the 4 codes, the exclusive rename
  real - it moves the file, and refuses to move a second over it; and with
  each of 5 refusals of the exclusive rename too, plain `rename` moves it;
  and `link` refused with EACCES is a failure, said: 25 of 25 covered.
  **Seen to fail on Linux** twice: with no fallback from `link` ("link:
  Operation not permitted"), and with none from the exclusive rename ("an
  exclusive rename: Invalid argument"). Skipped on Windows, which needs no
  hard links.
- `a_file_put_in_place_never_replaces_one_already_there`, on every platform.
  **Seen to fail on Linux** with `rename` in place of `link`.
- `many_fetches_and_reads_of_one_tile_at_once_all_read_it_whole` then ran
  20 times over on windows-debug, 64,000 threads in all: 20 of 20 passed.
  Of the 32 download, DEM, geoid and runway tests on windows-debug, 30 pass
  and 2 are skipped (the POSIX-only one, and one that needs the network).

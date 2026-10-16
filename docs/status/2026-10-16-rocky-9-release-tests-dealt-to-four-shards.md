### Rocky 9's release tests dealt to four shards, 2026-10-16 — item done

**What is still not true first.** Rocky's shards are dealt by linux-release's
cost table, measured on Ubuntu's runners, so they are uneven (8.9 to 12.5
minutes); a Rocky table would even them. Nothing else is known short.

**Seen on CI** (run on 57dac75f): the four shards' tests took 8.9, 9.9, 10.4
and 12.5 minutes, every one under 20. Three had taken 18.4, 14.6 and 10.0
(run 38805735233), the first near 20 with #210's six-minute serial test to
come, so four. That run's only failures were the heavy F-35B STOVL test on
Windows, main's red tree then, fixed by #214.

The Rocky 9 job built linux-release and ran its whole suite in one job, four
tests at a time: in run 38777694219 the tests took 42 minutes 29 s of the
job's 49, the longest job of the run (Ubuntu's two release shards took 27
minutes each). The owner asked for it to be sharded. Now:

- `rocky-build` (Rocky 9 linux-release - build) configures and builds as
  before, refuses Rocky 9's own GCC 11 as before, writes the cross-platform
  flights, and packs the build tree as Ubuntu's build does
  (`tree-rocky-linux-release`).
- `rocky-test` runs four shards in the same `rockylinux/rockylinux:9`
  container, each dealt by `tools/ci_shard.cmake` from linux-release's cost
  table (measured on Ubuntu's runners; `tools/ci_test_costs.py` matches one
  word of platform, so Rocky's shard names are not read into it).
- What a Rocky job installs is one list, `.github/actions/rocky-setup`, the
  same for the build and its shards. git is installed first by itself, since
  the checkout needs it for the submodules and the action is in the checkout.
- As Ubuntu's, the shards do not run on a push to main. The downloads cache
  had been saved by the Rocky job on a merge that changed a pinned list; it
  is saved now by shard 1 each night, and a pull request in between restores
  the older entry and fetches what it lacks. The build no longer restores
  the 3 GB of downloads it never read.
- **Old caches are pruned every night too**, not only after a merge. That
  merge-time save was the only thing that ever let `prune-caches` delete old
  downloads entries, so the nightly 3 GB entries had already piled up (three
  on main on 2026-10-16, about 9 of the repository's 10 GB); now each night
  keeps its own and deletes the older ones.
- `CI passed` needs `rocky-build` and `rocky-test`; `rocky-test` skipped on a
  push is a pass, as Ubuntu's, macOS's and Windows's test jobs are.

**The shards test checks it per job.** `tests/cmake/ci_shards.cmake` read the
matrix rows by preset, and linux-release is now sharded by two jobs, two
shards on Ubuntu and four on Rocky. It reads each job's matrix (its
`include:` block) separately now, and checks each job's dealing covers every
test once. Run here over linux-debug's 1,326 tests: 8 job-and-preset
dealings, 36 shards, every test in exactly one. **Seen red**: Rocky's third
row changed to a second shard 2 (with three shards), it fails with "2_linux-release: the matrix
has shards 1;2;2, not 1 to 3 once each"; restored, it passes.

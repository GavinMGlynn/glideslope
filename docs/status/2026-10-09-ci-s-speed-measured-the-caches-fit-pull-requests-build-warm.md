### CI's speed measured: the caches fit, pull requests build warm, macOS debug off pull requests; vcpkg fetched from the runner's own copy; main's runs finish, 2026-10-09 — four items still open

**What is still wrong first.** A pull request's run took far longer than its
jobs when several were in flight, because the account has five macOS
runners; the owner's answer (below, macOS debug off pull requests) is in,
but no pull request's run has yet been timed with it, so "A CI run takes
90-120 minutes" stays open. A failure only macOS debug finds now shows
after the merge, as a red main. No new Windows
compiler has come since 2026-09-30, so vcpkg's one-rebuild-per-image is
still not seen. The Windows configure's 3-minute bound needs a week on main
after this lands. The AI separation test is still 15-22 minutes in Linux
debug; its cause is found, in the simulation, and not changed here.

**Measured** (tools: `gh cache list`, the Actions jobs API for step times,
and the jobs' logs; scripts kept out of the tree).

- **The caches**: 12 entries, 7.0 GB of the repository's 10, every one
  main's and one generation of each kind: eight ccaches (416-659 MB), three
  vcpkg archives for Linux and macOS (18-20 MB), the downloads (3.0 GB). The
  pruning of 2026-10-07 works.
- **Pull requests build warm.** Every build job of four green pull-request
  runs of 2026-10-09 (37872093478, 37866144089, 37864280099, 37863487354)
  restored main's ccache (`ccache-*-c4d5a05...`): 487-533 of 565-567
  compiles hits on Linux and macOS, 750-790 of 831 on Windows - 86 to 95% -
  and build steps of 1.4 to 4.5 minutes, against 17 cold.
- **A run's time from push to result**: main's run 37884168001, 47 minutes
  (builds only; the Rocky job's build and tests, 44 minutes, set it). Pull
  request run 37872093478, 148 minutes with five other pull requests' runs
  in flight: every job but macOS's done at 47 minutes; macOS's release build
  started 62 minutes after the push and its last test shard at 123, each
  waiting for one of the account's five macOS runners. A pull request's run
  holds ten macOS jobs (two builds, seven test shards, the package), about
  150 runner-minutes, so two runs at once already queue. "A CI run takes
  90-120 minutes where its jobs need about 40" meets its verification's
  letter, but a pull request's run is still 148 minutes, so it stays open
  with the macOS wait named in it. Ticking it and moving the wait to an
  item of its own would split one item to tick the easy half.
- **The Windows configure**: main's week (2026-10-02 to 10-09), 152
  configures, median 107 s, 2 over 3 minutes (199 and 203 s, run
  37655616752). Every ci.yml run since 2026-10-07 00:00: 520, median 108 s,
  95th percentile 167 s, 16 over 3 minutes, the longest 334 s (job
  112924784697). Every one restored all 39 packages from NuGet. Where the
  slow ones' time went, from their logs: fetching vcpkg from GitHub, 19-60 s
  (44 s in the 334 s one), vcpkg's NuGet restore 27-60 s, and SDL's compiler
  checks.
- **vcpkg's new-image rebuilds**: the feed's newest upload is still
  2026-09-30 07:06 (abseil, `gh api /user/packages/nuget/.../versions`), so
  no Windows job has met a new compiler since. On Linux and macOS, main
  rebuilt every package on 2026-10-06 (configures of 950-1178 s on Ubuntu,
  763 and 818 s on macOS, runs 37421308785, 37528944095, 37530731073): their
  logs say "Cache not found" even for the image's prefix, on an image
  (macos15 20260907.0337.1) a month old - the archives had been evicted
  from the overflowing 10 GB, not invalidated. None since the pruning.
- **The AI separation test**: on this machine's linux-debug build its
  server flew 12,000 of the test's 108,000 steps in 41 s, one core.
  Forty stack samples of it (eu-stack, 30,000 steps): 33 in
  `FGGroundCallback::GetAGLevel`, 29 of those in `Dem::height_above_geoid`,
  and 22 reached through `Aircraft::state`/`Aircraft::value` reading a
  JSBSim property whose getter asks the ground's height again. The ground
  under an aircraft is looked up afresh by every read of its height above
  ground, through the sanitizers. The fix is in `src/sim` or `src/world`, so
  it is not in this pull request; the plan item says so.

**What changed.**
- **vcpkg is fetched from the runner's own copy** (`cmake/Vcpkg.cmake`).
  GitHub's runners carry vcpkg's whole history at VCPKG_INSTALLATION_ROOT;
  when that is a git repository with the pinned commit, the depth-1 fetch is
  made from it (`file://`, so `--depth` holds), and from GitHub otherwise -
  the same commit either way, so the same tree. The configure now prints
  where it fetched from and how long the fetch, checkout and bootstrap took.
  Checked with `cmake -P` (a copy with its file lock made per-process): from
  a local clone holding the commit, "fetched from <it>"; from a repository
  without it, "could not give the commit (128: ... not our ref ...);
  fetching from GitHub" and then "fetched from https://github.com/...";
  with none, from GitHub; bootstrapped every time. Because ci.yml's Linux,
  Rocky and macOS vcpkg cache keys hash `cmake/Vcpkg.cmake`, main's first
  run after this saves a new 20 MB entry for each (restores fall back to
  the older key and get every package).
- **Main's runs are not cancelled part-way.** `cancel-in-progress` is true
  for pull requests only. A merge's run waits for the one in progress
  (GitHub keeps one waiting, the newest) instead of cancelling it: seven of
  main's runs were cancelled on 2026-10-09 before 06:00 UTC, five of them
  20-31 minutes in, and a run cancelled after vcpkg rebuilt for a new
  Linux or macOS image saves nothing, so the next rebuilds too. The cost:
  a main run no longer cancelled holds its runners, two macOS ones
  included, to the end - about 15 macOS runner-minutes a merge. And a
  merge's run that is still waiting is replaced by a newer merge's, as
  before: with merges close together a commit on main can be built by no
  run of its own, only within the newer one's tree; the nightly run, every build and test
  job on main's head, is the backstop.

**Seen on CI** (this pull request's run 37892705684, on its head
1396fe03 - before the macOS change, with `cmake/Vcpkg.cmake` as it is now
but for the fallback's message): windows-debug
"vcpkg: fetched from C:/vcpkg in 4 s, checked out in 16 s", "bootstrapped
in 2 s", 39 packages from NuGet in 24 s, a 110 s configure; macos-release
"fetched from /usr/local/share/vcpkg in 0 s, checked out in 2 s". On head
b18c605 (run 37925545173, with the macOS change), macos-release again
"fetched from /usr/local/share/vcpkg in 1 s, checked out in 2 s". The
runners' clones hold the pinned commit.

**macOS debug off pull requests** (the owner's decision, 2026-10-09). On a
pull request macOS builds release only and runs its two test shards; the
macOS debug build and its five shards run after a merge to main, nightly
(ci.yml's schedule, which runs every build and test job; the package, there
as everywhere, only when the `changes` job finds a file it depends on
changed) and on a dispatch. A pull request's macOS jobs go from ten (about
150 runner-minutes) to four at most - three when the package is not
needed. In ci.yml:
`macos-build`'s matrix is release alone on a pull request; `macos-test`
keeps release's rows (not after a merge, as before); the new
`macos-debug-test` holds debug's rows and runs on every event but a pull
request, with macos-test's steps repeated, because
`tests/cmake/ci_shards.cmake` reads each test job's rows and dealing from
the file (a matrix built by `fromJSON` was tried first; the check would no
longer have found macOS's rows). macOS debug's tests do not save the 3 GB
downloads after a merge (nightly only). **CI passed** needs
`macos-debug-test`: skipped is a pass only on a pull request, and after a
merge it must pass. The aggregator's filter, taken from the file, run with
jq against made-up results: pull request with debug skipped and the rest
green, pass; pull request with release's tests skipped, fail; merge with
debug green and the other tests skipped, pass; merge with debug failed,
fail; merge with debug skipped, fail; nightly with debug skipped, fail;
nightly all green, pass. `ci_shards.cmake` run on the new file: 7 presets,
33 shards, every test in exactly one.

**To watch.** A pull request's run timed from push to result with this in
place, for "A CI run takes 90-120 minutes" to close; main's first run after
the merge, its five macOS debug shards green. A week of main's Windows configures, every one under 3
minutes, for that item to close; and the first new Windows image: one
rebuild, uploaded, and restores after it.

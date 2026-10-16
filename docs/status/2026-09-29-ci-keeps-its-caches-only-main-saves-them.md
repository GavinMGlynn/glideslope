### CI keeps its caches: only main saves them, 2026-09-29 — tail still open

**What is still missing, first**: this is verified only after it lands - a
pull request saves no cache, so its own run cannot show a warm one. The
verification is main's next run saving its caches and the pull requests after
it restoring them with ccache hits; until then the tail stays open. Windows
builds have no compiler cache at all (MSVC; 17-19 minutes each), and a pull
request still queues for macOS runners behind the others'. A pull request that
changes vcpkg's inputs or the downloads' pins restores the nearest older cache
and fetches the rest on every push until it lands.

**What was wrong.** A CI run took 90-120 minutes from push to result though its
longest chain of jobs - a build and a test shard - is about 40. Two causes:
- **Builds compiled everything from nothing.** Of the Linux and macOS builds in
  runs 36553559954, 36543443571 and 36552196259, all but one had 0% ccache hits
  ("Cache not found" for ccache, and for vcpkg's binary cache as often as not),
  20-35 minutes a build. The repository's Actions caches were at 10.7 GB of
  GitHub's 10: every pull request saved its own ccache for each preset and its
  own downloads (2.5 GB a platform, three platforms on main alone), and
  eviction took main's - the only caches a pull request can read.
- **Queueing**, worst on macOS (up to 50 minutes a job), which a slower build
  lengthens for everybody.

**What changed** (`.github/workflows/ci.yml`, `nightly.yml`, `package.yml`):
- Every cache save is skipped on a pull request; main (and the nightly and a
  run started by hand) saves, and pull requests restore main's.
- The downloads are one cache for every platform (`enableCrossOsArchive`),
  restored at the start of a test job and saved at its end. Its paths are the
  directory's children, `.downloads/*`, so that the exclusions of the Cesium
  tile caches the tests write there (`cesium-*.sqlite`) and of `test-scratch`
  take effect: `!` patterns do nothing against a directory named whole
  (actions/toolkit#713). Each run on main saves it under a new key ending in
  the run's number, restoring the newest before it (`restore-keys`), so the
  set grows by what each run fetched instead of being fixed by whichever
  shard first saved it.
- Rocky's job installs `zstd`. Without it, its caches were gzip, a different
  version of the same key, so it never restored the others' and saved its own.
- A new job on main, "Old caches pruned", deletes every older entry of a
  kind - each preset's ccache, and the downloads - once this run's entry of
  that kind is there, and lists what is left. A kind whose save failed (a
  warning, not a red job) keeps what it had.
- By hand on 2026-09-29, before this landed: the caches saved by pull
  requests (1.95 GB) and three ccache entries on main superseded by newer
  ones (0.75 GB) were deleted.

### The shard check is one test a preset, 2026-11-28

Phase 12c, "The shard check is one test a preset" (found 2026-11-28).

**What is still not true, first.**
- **The new tests' figures in `tests/ci_costs` are estimates**: the old
  test's figure in each table split eight ways (the header says so) until
  `tools/ci_test_costs.py` measures them from a run.
- **A dealing is still quadratic in the number of tests**: CMake's
  `string(JSON)` parses the whole listing on every call, so taking each test
  out of it costs as much as the listing is long. Now done once a test, not
  twice; a linear dealing would need the listing split another way.

## What changed

- **`every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
  is gone**, replaced by one test a preset,
  `every_<preset>_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
  for linux-debug, linux-release, macos-debug, macos-release, windows-debug,
  windows-release and windows-clang, running side by side. In one test every
  preset's dealings took 1,063 to 1,382 s on Windows CI, and 2,061 s in run
  46106569461 - past the 45-minute shard's limit for its longest test, so
  #434, which measured it, could not deal windows-debug at all.
- **`tests/cmake/ci_shards.cmake` takes `-DPRESET=`**: it reads the whole of
  ci.yml and nightly.yml as before (so which jobs, ways and "first of its
  kind" each preset is dealt are the same whichever test runs), and does and
  reads back only that preset's dealings - every job's rows of it, both ways
  its runs leave tests out (the night's way sized on its own system's
  hosts), its debug sweep's seven days, and the nightly's repeat job for
  linux-debug - counting each shard file as it is read back and checked,
  and failing unless that is every shard file its workflows ask for
  (linux-debug 81, linux-release 24, macos-debug 37, macos-release 11,
  windows-debug 69, windows-release 16, windows-clang 14). The old count of
  shards dealt against the matrix rows, which the rows check already pins,
  is gone.
- **`every_preset_ci_deals_has_a_shard_test_of_its_own_and_every_test_job_deals_its_own_row`**
  (`-DPRESETS=`, the list tests/CMakeLists.txt makes the tests from) deals
  nothing: it keeps every check of the workflows themselves - each job
  dealing its own row, the gates, the nightly label named with its reason
  ("4 nightly test ...; 11 of 11 test jobs leave it out of pull requests"),
  the seven days being every gust_sweep test once, every preset sharded
  swept, every way a run leaves tests out dealt by some preset - and fails
  unless the presets with a test are exactly the presets the workflows deal,
  naming any missing or extra.
- **`tools/ci_shard.cmake` takes each test out of ctest's listing once.** It
  did so twice a test (for the longest test, then for its cost), and
  profiled, those two `string(JSON GET)` lines were 18 of a dealing's 19 s
  over 1,681 tests; now 10 s, with byte-identical shard files.

## Verification

- Local (linux-debug, configured only; the tests list a build, they run
  nothing of it), `ctest -j4`: linux-debug 53 s, linux-release 71 s,
  macos-debug 22 s, macos-release 18 s, windows-debug 24 s,
  windows-release 19 s, windows-clang 16 s, the workflow test 0.5 s.
- **Seen red**: with `tools/ci_shard.cmake` made to drop shard 2's last test,
  all seven preset tests failed ("... dealt, 1687 distinct; missing: 1687")
  and the workflow test passed; with windows-clang left out of the list in
  tests/CMakeLists.txt, the workflow test failed ("dealt, with no test:
  windows-clang"). With shard 2's read-back skipped in every way, the
  preset tests failed on the count ("windows-debug: 68 of its 69 shard files
  read back", "linux-release: 19 of its 24"). `-DPRESET=linux-foo` fails "no
  workflow deals it". Each reverted.
- Windows (tools/windows_build.sh, windows-debug on the development machine),
  all eight green, 325 s between them: linux-debug 296 s, windows-debug
  253 s, linux-release 177 s, macos-debug 157 s, windows-release 108 s,
  windows-clang 104 s, macos-release 72 s, the workflow test 20 s - each well
  inside its half hour, where the one test took 1,063 to 2,061 s on CI.

### main only by pull request, when every platform is green, 2026-09-24

**Why it changed.** The project owner asked why CI broke with nearly every
change. Four reasons, all process: every commit went straight to `main`, so
CI was the first place Windows and macOS saw the code (the document's client
had never been compiled with Winsock; macOS shrinks a window to its small
screen); some code was built nowhere at all (the server-only configuration
and its Docker image); tests written on a fast machine assumed its speed; and
each push cancelled the run before it, so failures surfaced late and stacked.

**What changed.**

- `main` is protected on GitHub, admins included: nothing is pushed to it.
  An item goes on a branch, every commit pushed at once, and into `main` by
  pull request when the one required check, **CI passed**, is green - a last
  job in `ci.yml` that needs every other job to have succeeded, so that a job
  added or split into shards is covered without editing the protection. The
  branch must be up to date with `main`, and history is linear. Proved: a
  direct push was refused ("protected branch hook declined").
- `tools/windows_build.sh` builds the current, pushed branch in the Windows
  working copy from WSL, with MSVC through vcvarsall, refusing if either copy
  has uncommitted tracked changes.
- **No slow-runner imitation.** A `tools/slow_ctest.sh` was written - tests
  pinned to a busy CPU - and taken out the same day: at a CPU and a half, and
  at one CPU alone, it passed the four-player test that CI had failed; at one
  busy CPU it reproduced that failure, and ran the fixed, event-driven tests
  past half an hour each. CI's runners are slow in more ways than the CPU - a
  cold disk, a server building its terrain - and no setting imitated them
  both ways. What stands instead is the rule in CLAUDE.md (a test waits on
  events, never the clock) and the nightly repeats on real runners.
- **Packaging is in the gate**: `package.yml` is called from `ci.yml` when the
  build, the assets, the dependencies or a frontend changed (a `changes` job
  decides), and CI passed counts it - a pull request that breaks a package
  fails its own gate, not `main` after it.
- **A pull request from the first push**: `tools/start_item.sh` makes the
  branch, pushes it and opens a draft pull request, because CI runs on pull
  requests and a branch pushed without one is tested by nothing.
- **Hooks in the working copy** (`.githooks/`, turned on with
  `git config core.hooksPath .githooks`): pre-commit refuses a debugging
  marker, a tracked file changed and not staged, a key or token, and a plan
  tick without PROJECT_STATUS.md; pre-push refuses `main`, builds linux-debug
  and runs every test ctest last measured under five seconds, and reminds of
  `tools/windows_build.sh` for Windows or Apple code. Plain shell scripts, not
  lefthook: the project prefers no dependency. `the_pre_commit_hook_refuses_an_unfinished_commit_and_nothing_else`
  walks all eight cases; its first draft compared a macro's argument as a
  variable and checked nothing - seen to pass with the hook broken, fixed, and
  then seen to fail with the marker check removed.
- **Nightly** (`nightly.yml`): the tests that run programs against each other,
  five times each on `main`, so that a test that only sometimes passes is
  found there. Three of them were already doing it: a server stopped by the
  clock rather than by its clients - a fixed thirteen seconds on a slow runner
  ended before the fourth player had flown - and a client that gave up on a
  server after five seconds, less than a debug server takes to build its
  terrain. The server now takes `--until-empty` (a test flag: stop once every
  client that joined has gone), the client waits a minute, and a departing
  player's aircraft says how far it banked as it goes.
- CI builds the server's container image (`deploy/Dockerfile`, the server-only
  configuration) and a server started from it must accept a client and refuse
  `--window` as built without one. The image is now Rocky Linux 10, as the
  server will be hosted: Rocky 9's CMake is 3.26 where the project needs 3.28,
  and its libsodium is in EPEL, which was not enabled - neither was known,
  because the image had never been built.

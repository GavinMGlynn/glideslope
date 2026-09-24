# Working conventions

## The shape of the thing

Glideslope is a **multiplayer flight simulator**: real flight dynamics and live
wind, flown over real-world terrain streamed from the internet, where any
aircraft can be handed to an AI pilot and taken back. `docs/REQUIREMENTS.md`
holds the design and every decision made about it, closed and open;
`docs/FEATURES.md` is the menu.

It is **not a scored or competitive game.** There is no rollback, no
deterministic simulation and no server verification of results. A proposal that
needs any of them is a proposal to change that sentence first.

**The server owns every aircraft; clients send inputs, not state.** An aircraft
is a JSBSim instance plus a controller, and a controller is either a person's
input or an AI pilot, so switching between the two is a controller swap and
nothing more. Clients predict their own aircraft and reconcile against the
server. They will drift, because JSBSim is floating point and the machines
differ; that drift is measured and bounded, not treated as a bug.

**The simulation steps at a fixed rate; the presentation does not.** Physics
runs in fixed 120 Hz steps and frames interpolate between states. Physics per
frame is not allowed.

## Discipline

- **The simulation links no presentation.** `src/sim/` may not include SDL
  video, input or audio, nor `gfx/`, `ui/`, `platform/` or a frontend, and may
  not link an SDL target. Checked at configure time by `cmake/Layering.cmake`,
  not by review. `glideslope_cli` linking the simulation and nothing
  presentational is the proof, and the server links the same simulation to run
  AI aircraft.
- **World positions are double precision, Earth-centred and Earth-fixed.**
  Floats exist only relative to the camera, at the floating origin, and nowhere
  in the simulation.
- **Warnings are errors, in every build type.** `-Wconversion` and
  `-Wsign-conversion` included: double-to-float narrowing at the floating origin
  is exactly the silent bug they catch. First-party targets only; vendored code
  in `ext/` is untouched.
- **Verify on the real output.** A frame written by `--shot`, a `ctest` run,
  flight numbers from `glideslope_cli` — not a proxy, and not "it should work
  now".
- **The tripwire is a same-machine hash plus cross-platform tolerances.**
  `glideslope_cli selftest` flies a fixed input log and prints a state hash that
  must not move run to run on one build. A change that moves it is deliberate,
  with a note in `docs/PROJECT_STATUS.md` saying why. Across platforms the hash
  is not expected to agree; the flight figures are, within stated tolerances.
- **Content is data, not code.** Aircraft, sessions, regions and AI flight plans
  live in files. A hard-coded aircraft is a prototype and must be replaced
  before anything is built on it.
- **One item at a time, landing with its test.** Keep `ctest` green; a red tree
  is the stop-everything condition. Name tests as sentences stating the fact
  they pin: `a_cessna_at_full_power_climbs_near_its_published_rate`.
- **A test nobody has seen fail is not trusted.** Where a verification says a
  deliberate bug turns it red, the bug was introduced, the failure watched, and
  the bug reverted.
- **A test that only sometimes tests its rule is worse than no test**, because
  the green tick is not evidence. Build the situation explicitly.
- **Cover every scenario, not a sample.** Every control, every aircraft, every
  setting, every state a screen can be in. If exhaustive looks too slow, fix the
  step, not the claim.
- **Coverage is asserted, not believed.** A test that walks a space states how
  big the space is and how much of it it covered, and fails when those differ.
  Anything left out is named in the test, with its reason.
- **Each item on its own branch, into `main` by pull request when CI is green
  everywhere.** `main` is protected on GitHub: nothing is pushed to it
  directly, by anybody. Work an item on a branch named for it, commit as it
  goes, and push every commit to that branch straight away - a commit only
  local has not landed: the Windows and Linux working copies meet on GitHub and
  nowhere else. Open its pull request with the first push; CI runs on every
  push to it. It merges (`gh pr merge --rebase`) only when the one required
  check, **CI passed**, is green - every job on every platform - with the
  branch up to date with `main`. A red run is fixed on the branch. Rewriting
  history that is already pushed needs the project owner's say-so.
- **Start an item with `tools/start_item.sh BRANCH "title"`**: the branch from
  an up-to-date `main`, pushed, with its draft pull request open - CI runs on
  pull requests, so a branch without one is tested by nothing. Merge with
  `gh pr ready && gh pr merge --rebase --delete-branch` when CI passed is green.
- **The hooks are on in every working copy**: `git config core.hooksPath
  .githooks`. pre-commit refuses a debugging marker, a tracked change left
  unstaged, a key, and a plan tick without PROJECT_STATUS.md; pre-push refuses
  `main`, builds, and runs the quick tests. `--no-verify` only with the reason
  in the commit message.
- **Before pushing code that is not Linux's alone** - sockets, windows, file
  paths, anything under `#ifdef _WIN32` or Apple's - build it on Windows first:
  `tools/windows_build.sh` builds this branch in the Windows working copy from
  WSL. Code that only CI compiles is code nobody has compiled.
- **A test that takes time takes simulated time.** Count steps, not seconds,
  and bound what a slow machine could delay. Before pushing a test that runs
  programs against each other, run it slowed: `tools/slow_ctest.sh -R NAME`.
- **Every commit that lands an item updates both living docs in that same
  commit**: `docs/PROJECT_STATUS.md` (what now works, with its verification) and
  `docs/COMPLETION_PLAN.md` (tick the item, add any tails found on the way).
  Re-read both in full at every phase boundary.
- **`[x]` means 100% of the item.** An item with anything unimplemented stays
  `[ ]`, with the missing part named in its text. Splitting an item to tick the
  easy half is not allowed, and an item without a named verification cannot be
  ticked.
- **`COMPLETION_PLAN.md` is a user document. Keep it a summary.** One or two
  sentences per item and a verification in plain English. Implementation detail,
  test-name inventories and design rationale go in `PROJECT_STATUS.md`.
- **`FEATURES.md` has no implementation in it** — no data structures, formats or
  function names. It is the menu at the altitude of "what would the player
  notice".
- **Never describe a partial module as working.** Report the missing part first:
  "terrain streams and draws; nothing collides with it yet", never "terrain
  works".
- **Prefer no dependency to a small one.** Pin every dependency: submodules
  under `ext/` by tag or SHA, non-git sources (the SQLite amalgamation,
  datasets) by URL plus SHA-256. No Git LFS.
- **`assets/jsbsim/` is made, not written.** The flight models there come from
  `ext/jsbsim` through `tools/make_c172p.py`, whose docstring lists every change
  and what it answers. Change the script and run it; a test fails if the
  committed files differ from what it makes.
- **No keys or tokens in the repository, ever.** Cesium ion tokens and Google
  Maps Platform keys belong to the user and are read at run time. A test that
  needs one reports itself skipped when none is present — skipped, never passed.

## This project's specifics

- **Collision terrain is always the open DEM**, at a pinned dataset version and
  hash, and never the visual mesh — so the server and every client's prediction
  agree on where the ground is. Visual and collision terrain may disagree; the
  size of that disagreement is measured, not assumed.
- **The whole world is in play.** Nothing may assume a session lives in one
  region; the server loads terrain around each aircraft wherever it is.
- **The LLM plans; the controllers fly.** Nothing a language model produces
  reaches a control surface except as a flight plan the autopilot executes.
- **At most four players on a server** (`--players N`, 1 to 4). How many AI
  aircraft a server runs is a server setting, default 4.
- **Whichever terrain provider is drawing, its attribution is on screen.**
- **The transport is gearstick's**, with its own magic value, and
  `docs/TRANSPORT.md` describes it byte for byte, including what it does not
  claim.

## Building

```sh
cmake --preset linux-debug                # or linux-release, macos-*, windows-*
cmake --build --preset linux-debug
ctest --preset linux-debug
cpack --preset linux-release              # a package; macos-release, windows-release
```

The development machines are native Windows 11 for rendering work and WSL
(Rocky Linux 10) for Linux builds and headless work. In WSL, keep the working
copy on the Linux filesystem, not under `/mnt/c`.

The first configure of a machine builds Cesium Native's dependencies through
vcpkg (`cmake/Vcpkg.cmake`, `ext/README.md`): most of an hour, then kept in
vcpkg's binary cache. On Linux that needs Perl's `IPC::Cmd`, NASM, make, and
autoconf, autoconf-archive, automake and libtool (libsodium's port).
**Cap the build's parallelism in WSL** - `cmake --build --preset linux-debug -j8`,
`ctest -j4`, and one build at a time: the sanitized build of the client links a
binary of hundreds of megabytes, and ninja's default of every core at once has
run WSL out of memory.

## Layout

```
src/sim/        JSBSim wrapper, aircraft, controllers, autopilot, AI pilot
src/world/      coordinates, floating origin, terrain streaming and queries, weather
src/gfx/        SDL_GPU renderer, shaders (GLSL, in shaders/), Cesium Native glue, HUD
src/net/        protocol, packet encode/decode, interpolation
src/platform/   paths, input, sockets
src/frontend/   one main per executable: client/, server/, cli/
cmake/          platform gate, warning set, sanitizers, layering check, dependencies
tests/          ctest tests named as sentences, and the probe projects they build
assets/         run-time data, copied to data/ beside the programs
tools/          scripts that make committed assets; shaderc/, the build's shader compiler
ext/            pinned submodules - see ext/README.md
docs/           REQUIREMENTS.md and the living documents
```

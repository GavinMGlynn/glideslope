### CI's Windows builds keep a compiler cache, 2026-09-29 — tail still open

**What is still missing, first**: windows-clang has not been measured with
nothing to compile. Its compiles are all cacheable now (792 of 792), and the
one warm run it had (after its own cache, with SDL's flags changed since) hit
525 of 792, every one outside SDL. The run that would show it all warm,
f235f7d's (36570065355), spent 28 minutes in windows-clang's configure - the
vcpkg stall below, not this change. A pull request's Windows build is
only as warm as main's last one (main's rule, below); the release package job
(`package.yml`) still builds Windows without a cache.

**What was wrong.** Linux and macOS kept a ccache and Windows kept nothing:
each of windows-debug, windows-release and windows-clang compiled all 792
objects from nothing on every push. On main's run 36542566614 (no cache) the
build steps took 15.0, 11.5 and 10.8 minutes, the jobs 17.4, 14.2 and 13.7.

**What changed** (`.github/workflows/ci.yml`, `windows-build`):
- **ccache 4.14.1**, the Windows zip from its GitHub release, pinned by
  SHA-256, as `CMAKE_<LANG>_COMPILER_LAUNCHER`. Chosen over sccache and
  buildcache because Linux and macOS already use ccache - one tool, one set of
  stats and settings - and ccache 4.x caches cl.exe and clang-cl through the
  Ninja generator. All three need the same compile flags (below).
- **Only CI's configure changes**; every flag is on its command line and the
  presets are untouched, so a developer's build is as it was:
  - `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` (/Z7): ccache refuses a
    compile that writes a PDB (/Zi). windows-debug's preset names `/Zi` in
    its own flags, for the vendored projects older than CMP0141, so CI passes
    the same flags with `/Z7`. The linker still writes each program's PDB
    from its objects, and those are what "Pack what the tests need" packs and
    "The stack of every crash" gives cdb.
  - `CMAKE_DISABLE_PRECOMPILE_HEADERS=ON`: SDL's precompiled header is not
    cacheable.
  - `CMAKE_CXX_SCAN_FOR_MODULES=OFF`, as on Linux.
  - `CMAKE_PROJECT_INCLUDE=cmake/ci_clang_cl_show_includes.cmake`: CMake 4
    has clang-cl write a gcc-style depfile through `-clang:-MD -clang:-MT
    -clang:-MF`, and ccache refuses every `/clang:` option - the first run had
    792 of 792 clang-cl compiles "unsupported compiler option". The file has
    clang-cl report its headers with /showIncludes, as cl.exe does.
  - Warnings are still errors; no first-party warning flag changed (SDL's
    documentation-command warning is off, below).
- **The cache rules of 2026-09-29's entry below**: restored from main's newest
  entry for the preset (`ccache-windows-<preset>-`), saved only on main under
  the commit, 500M a preset (a full build is 0.27 GB), and "Old caches pruned"
  keeps one entry of each Windows preset as it does the others.

**Found on the way: a Windows configure sometimes takes 30 minutes**, in
vcpkg, with or without this change. Main's run 36565671119 spent 1751 s in a
windows-release configure, and ci-caches-fit's run 36569605692 logged "vcpkg:
All requested installations completed successfully in: 28 min" after an exact
hit on vcpkg's binary cache - vcpkg on Windows sometimes rebuilds, or stalls,
what the cache holds. Not fixed here; a tail in COMPLETION_PLAN.

**Measured on CI** (this pull request, with a temporary commit - since
dropped - that let it save its own cache):
- Cold, run 36561782265 (nothing to restore): build steps 16.8 min (debug) and
  13.6 (release), jobs 19.4 and 16.6 - about two minutes more than without
  ccache, for the misses and /Z7's larger objects.
- Warm, run 36565087134 (the cold run's cache): **791 of 792 compiles hits
  (99.9%)** on both; build steps 68 s (debug) and 62 s (release), **jobs 4.1
  minutes** each, from 17.4 and 14.2.
- clang-cl: run 36565087134, 0 of 792 compiles cacheable (every one
  "unsupported compiler option", `-clang:-MD`); run 36566652130, after
  `ci_clang_cl_show_includes.cmake`, 526 of 792 cacheable and 266 not - SDL's
  `/clang:-fcomment-block-commands=threadsafety`, now preset off in CI with
  `-DHAVE_CLANG_COMMENT_BLOCK_COMMANDS=OFF`. Build step 12.8 min, job 15.7.
  Run 36568741887 (1bd8ee8), restoring that cache: **792 of 792 cacheable,
  525 hits** - everything but SDL, whose flags had just changed; build step
  2.5 min, job 6.0. Without @threadsafety, SDL's
  -Wdocumentation-unknown-command then warned 265 times, so CI turns that
  check off too (`-DHAVE_GCC_WDOCUMENTATION_UNKNOWN_COMMAND=OFF`; SDL's own
  code, never an error). Fully warm: not yet measured.
- The temporary commits (the pull request saving its own Windows cache, and
  a step printing ccache's reasons for clang-cl) are dropped; the branch
  saves nothing, as #57 says.

### A slow Windows configure was a new runner image, rebuilt by every pull request; now pull requests may publish, 2026-09-30 — tail still open

**What is still missing, first**: the plan item's verification - every
Windows configure whose vcpkg cache hit under 3 minutes, over a week of runs
on main - is not yet counted; a week has not passed. No pull request's
upload has been seen yet: this branch's runs only read (it changes
`.github/`), and the first comes with the next new image or port change on
a pull request allowed to write. And a new image will still cost more than
one rebuild while it arrives: every job that lands on it before the first
has finished and uploaded (about 22 minutes) builds too.

**The cause, from the logs, not a guess.** The ~1,370 s configures were not a
wait but a full rebuild of the 39 packages ("All requested installations
completed successfully in: 21 min", against 2-4 s after a restore; the
restore itself, which vcpkg times apart, was 22 s on this branch). GitHub was
rolling out a new Windows image, windows-2025-vs2026 20260925.250.1, with
MSVC 19.51.36260.0, beside 20260922.246.2 with 19.51.36257.0; the compiler is
in every package's ABI, so a job on the new image found none of its packages.
Of ci.yml's 64 Windows configures on 2026-09-30 after 03:00, every one over
200 s is in the table below or is one of main's three at 03:35, which first
populated the feed. Of the 40 whose logs were read, all 8 on 20260925.250.1
before 07:10 rebuilt, both on it after 07:10 restored, and the 27 on
20260922.246.2 after 03:35 restored (installs of 1.7-4 s):

| run | branch | preset | image | configure | vcpkg |
|---|---|---|---|---|---|
| 36670245478 | stall-recovery-two-part | release | 250.1 | 1379 s | 22 min |
| 36672081323 | a-model-per-ai-aircraft | clang | 250.1 | 1374 s | 21 min |
| 36672182492 | copilot-flies-with-you | debug | 250.1 | 1360 s | 22 min |
| 36672183998 | cached-tile-cut-short | debug | 250.1 | 1365 s | 21 min |
| 36674576086 | slow-frames-lose-no-ticks | clang | 250.1 | 1377 s | 22 min |
| 36676611593 | rl-landing-agent | debug | 250.1 | 840 s | cancelled |
| 36680141695 | **main** | release | 250.1 | 1312 s | 21 min, uploaded 07:06-07:09 |
| 36680523613 | cached-tile-cut-short | debug | 250.1 | 812 s | 12 min |
| 36686484430 | this branch | clang | 250.1 | 96 s | 2.1 s |
| 36686508000 | rl-landing-agent | release | 250.1 | 113 s | 2.3 s |

**Why it repeated.** Only main wrote to the feed; a pull request only read,
by analogy with the Actions cache. So each pull request's job that landed on
the new image rebuilt and threw the result away - about one job in seven,
from 04:46 until 07:10 - until one of main's own three jobs happened to land
there (run 36680141695) and uploaded the new versions (abseil's, for one, at
07:06). Every job on that image since has restored. The one of 812 s began
while main's job was building and uploading; what it restored its log does
not say, which is what the logging below is for.

**The fix.** A pull request's Windows jobs have write access to the feed
too, unless its diff could change how a package builds. The Actions cache's
reason for read-only pull requests - one 10 GB for the repository, which
pull requests' saves crowded main out of - does not hold for public
packages, which are free and have no limit. But the ABI hash does not cover
everything (review found this): it covers the port files, triplet, cl.exe's
hash, toolchain file, CMake and PowerShell versions, helper scripts,
dependencies and VCPKG_ENV_PASSTHROUGH, not vcpkg's own binary, the rest of
the MSVC toolset and SDK, tools on PATH, or variables kept by
VCPKG_KEEP_ENV_VARS (`_CL_=/Od`, say); and a NuGet version is written once,
first writer wins. A port change uploads under a hash main never asks for,
but a pull request that changed the workflow or the CMake around vcpkg could
publish packages main would restore for ever. So ci.yml's `changes` job
gives the three Windows builds and package.yml's Windows zip `write` - true
on main (push, schedule), and on a pull request only when its diff against
its base touches none of `.github/`, `cmake/`, the top-level
`CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`,
`vcpkg-configuration.json`, `.gitmodules` or `ext/cesium-native`; otherwise,
or when the diff cannot be taken, false. A bad version is deleted by hand
(the action's header says how). A fork's pull request has no `VCPKG_PAT`
and builds from source, as before.

**The guard, checked dry**: the step's script, taken from ci.yml, run
against real commits - a pull request touching only `src/`, `tests/` and
`tests/cmake/` (1a6c8f5..2753af1) writes; one touching the top-level
`CMakeLists.txt` (0c39005..1a6c8f5) and this branch (`.github/`, `cmake/`)
only read; a base that is missing or not fetched reads; a push or the
schedule on main writes; a dispatch on another branch reads.

**Now visible in every job's log.** `cmake/Vcpkg.cmake` prints vcpkg's
summary lines - the compiler it found, what it restored and from where, each
package it built with its time, each upload, and an upload refused ("Pushing
NuGet to ... failed", NuGet's status code; checked with `cmake -P` on sample
output) - not only the last line; CI
keeps `vcpkg-install.log` from each Windows configure as the artifact
`vcpkg-install-<preset>` (14 days); and the feed's step prints the runner
image. On this branch's run 36686994405, windows-clang on 20260922.246.2
printed its compiler (MSVC 14.51.36231's cl.exe), "Restored 39 package(s)
from NuGet in 22 s", and "All requested installations completed
successfully in: 2.3 s": the packages are public, and restore on CI with
the feed as the only binary source. A restore prints one "Elapsed time"
line of milliseconds per package; those are left out, and a build's, in
seconds or minutes, kept.

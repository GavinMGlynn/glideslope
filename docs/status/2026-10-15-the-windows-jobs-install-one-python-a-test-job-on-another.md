### The Windows jobs install one Python: a test job on another runner image from its build, 2026-10-15 — item done

**What is still not true first.** **Only Windows is changed**: the Linux
and macOS test jobs also run a tree configured by another job, and have not
been seen to fail this way; their Python is not pinned or checked.

**Verified on CI**, run 38637622313 on this branch: the jobs landed on both
images, and each of the three builds' new check printed "the configure found
the pinned Python: C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe"
- two builds on 20260925.250.1, windows-debug's on 20261007.264.1. Across
images: windows-clang built on the old image and its tests 3 of 3 ran on the
new one - the shard whose script checks failed in #203's run - and passed
443 of 443, every committed-model and script check among them;
windows-debug built on the new image and its shards 2, 3, 5 and 6 ran on the
old one, and passed.

**What happened.** #203's CI run 38624857649 failed five Windows test jobs,
in debug, release and clang: in each, every check that runs a Python script
failed in a hundredth of a second with an empty message - the committed
models, the DEM coverage, the runway strips, the visual models, all of them,
and none of it #203's. The two that print their exit said it:
`alpha_rate_terms.py exited no such file or directory`. GitHub was part way
through a new Windows image, 20261007.264.1 beside 20260925.250.1, and the
jobs split on it exactly: every failing job ran on the new image, every
passing one on the old, the builds on the old. A rerun of the failed jobs
passed the three that landed on the old image and failed the two that did
not. The build's configure writes the Python it found
(`find_package(Python3)` in tests/CMakeLists.txt) into the tests' command
lines, and the test job runs that tree on whatever image it gets. Left to
itself FindPython3 took the newest Python the image registered: the build
tree's CMakeCache.txt has
`C:/hostedtoolcache/windows/Python/3.14.7/x64/python3.exe`. The two images'
readmes differ only in the tool cache, 3.13.15 and 3.14.7 become 3.13.16 and
3.14.8 - a patch release was enough. The Python first on the path, 3.12.10,
was the same on both and was never used.

**The fix.** `.github/actions/windows-setup`, which every Windows build and
test job runs "so that a test job finds exactly the machine its build was
made on", now pins Python 3.12.10 with `actions/setup-python@v6`, which sets
`Python3_ROOT_DIR`; FindPython3 searches that before the registry (CMP0094,
the LOCATION strategy). 3.12.10 is in both images' tool cache, and
setup-python fetches it to the same path where it is not. **The build checks
it**: after the configure, `tools/ci_python_pinned.cmake` fails the job
unless the Python in its CMakeCache.txt is under `Python3_ROOT_DIR`, so a
green run shows the fix took, whichever images its jobs landed on. Seen red
and green here on a CMakeCache line each: the old tree's 3.14.7 against the
pin ("the configure found .../3.14.7/x64/python3.exe, not the pinned Python
in .../3.12.10/x64"), the pin unset, and a cache with no Python, each refused;
3.12.10 accepted. And the four script checks - committed_assets.cmake,
dem_coverage.cmake, runway_strips.cmake and python_test.cmake - now say
which Python ran which script and how it exited; seen with a Python that is
not there: `/no/such/python3 tools/make_j3cub.py exited no such file or
directory`.

**From the review of #204.** The cause was first written as the path's
Python moving; the review read the build tree's CMakeCache and the image
readmes and found the registry's 3.14.7 (above). It asked for the check in
the build, so that the green tick does not rest on GitHub's choice of image,
and for python_test.cmake to name its Python too.

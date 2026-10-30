### macOS's tests run on the pinned Python, as Windows' do - 2026-10-30

**What failed.** #273's full run 40986292025: macOS release's test shards 2/3
and 3/3 failed every "the committed ... is what its script makes" check in
0.01 s each: "/opt/homebrew/bin/python3.15 tools/make_c172p.py exited no such
file or directory". The build job's configure found Homebrew's newest Python
and wrote it into the tests' command lines; the test jobs unpack that tree on
other macOS runners, which did not have it. Windows failed the same way on
2026-10-14 (run 38624857649) and was fixed by pinning.

**What now works.** `.github/actions/macos-setup` pins Python 3.12.10 with
`actions/setup-python` (the same version Windows pins), which sets
`Python3_ROOT_DIR`, the place FindPython3 searches first. The macOS build job
runs `tools/ci_python_pinned.cmake` after its configure, so a build that found
any other Python fails there, before its tree reaches a test runner. Both the
build and test jobs use macos-setup, so the test runners have the same Python
at the same path.

**Verification.** The macOS build's new step passes (the configure found the
pinned Python) and macOS's test shards pass on this pull request's full run.
The check itself was seen red on Windows when it was written (2026-10-14).

**Not covered.** The Linux jobs run in their own containers, whose Python is
the image's and the same for build and test; nothing here changes them.

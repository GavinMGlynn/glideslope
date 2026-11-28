### The pre-push hook runs its quick tests from a file, 2026-11-28

Phase 12c, a tail found on 2026-11-28.

**What is still not true, first.** Nothing is missing from this item; the hook
still builds and runs only on Linux debug, and a machine without
`build/linux-debug/Testing/Temporary/CTestCostData.txt` still runs no tests.

**What now works.** `.githooks/pre-push` used to join every quick test's name
into one `ctest -R` regular expression. After a full suite run there are
thousands, and ctest answered "Expression too big" (and, past that, the
command line itself overflowed), so an agent had to push with
`GLIDESLOPE_PUSH_QUICK=0`. The hook now writes the names, one per line, to
`build/linux-debug/Testing/Temporary/pre-push-quick.txt` and runs
`ctest --tests-from-file` on it, keeping `-LE live`, `-j4` and the dropped
`GLIDESLOPE_LIVE_MODEL`.

**Verification.** `the_pre_push_hook_runs_thousands_of_quick_tests_and_not_the_live_one`
(`tests/cmake/githooks_pre_push_quick.cmake`) makes a scratch CMake project of
3000 trivial tests with long names, a cost file naming them all quick, and one
live-labelled test that fails if run; it runs the hook and requires "100% tests
passed ... out of 3000". Seen red against the old hook (it exited 1: `env:
Argument list too long`), green with the file.

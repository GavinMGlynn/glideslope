### The registered-tests check reads its names from a file: it had stopped starting on Windows, 2026-10-02 — fix

`every_compiled_unit_test_is_registered_with_ctest` failed `BAD_COMMAND` on all
three Windows test jobs - on main since about c81c252 and on every branch since:
ctest printed "Process not started ... [name too long]", because the 515 unit
test names were handed to `cmake -P` as one `-DREGISTERED=` argument and passed
Windows' 32,767-character command line. The names are now written at configure
time to `registered_unit_tests.txt` in the build's `tests/`, a name a line, and
the script reads that file, refusing an empty one. A docs-only run on main skips
the tests, which is why main's last green run did not show it.

*Verification:* linux-debug, the check passes with "515 compiled tests, each
registered"; with the file's first line deleted it went red ("compiled but not
registered with ctest: every_correction_small_enough_to_hide_is_taken_up_without_a_step"),
and green again with the file restored. Windows: CI on this pull request.

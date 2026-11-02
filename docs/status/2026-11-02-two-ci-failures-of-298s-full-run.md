### Two CI failures of #298's full run, 2026-11-02 — both answered

**What is still not true first.**
- **The F-35B mid-bank join is not fixed, only moved.** Her median prediction
  error still goes past 60 mm on a busy Windows debug runner; the cause, her
  jet-borne law's state drifting from the server's, is the open Phase 12a
  busy-machine item. The 60 mm bound is unchanged.
- **The torn line was not reproduced in the multi-process test itself.** What
  was reproduced is its cause, in the programs' own binaries: a line longer
  than 128 bytes written to standard error as two writes (below).
- **Windows is unchanged.** Its C runtime takes line buffering as full
  buffering, so the helper leaves its streams alone there; that its printf to
  an unbuffered standard error makes one write a call is the UCRT's
  behaviour, relied on as impair's own relaying already did, not tested here.

**1. `a_client_joining_an_f35b_mid_bank_predicts_her_from_the_servers_law_state`
runs nightly.** Windows debug, job 125124862223 of run 41397416052: "the
median prediction error was 61 mm, over 60 mm". Its first CI failure was run
41284303752 (64 mm), recorded in the Phase 12a busy-machine item, which said a
second would move it to nightly. By CLAUDE.md's rule it is labelled nightly
and named in `tests/cmake/nightly_tests.cmake` with its reason and both runs
(`NIGHTLY_COUNT` 4 to 5); the plan item says so, and its nightly result is
reported there.

**2. Standard error's lines go out whole.** Linux release, job 125123367141:
`the_client_with_the_window_refused_by_a_forger_while_its_session_is_quiet_goes_back_to_it`
failed at client_forged_refusal.cmake:94 although the forger did its work -
impair's summary was torn by the client's line about a tile fetch: "...after
3.0 s; 0 dropped glideslope: https://tiles.maps.eox.at/...jpg: ... given up
before it began", the rest of the summary after it.

*Why the writes interleave.* Standard error is unbuffered, and glibc since
2.37 formats a printf to an unbuffered stream through a 128-byte stage
(`PRINTF_BUFFER_SIZE_TO_FILE_STAGE`), writing each 128 bytes as it fills. Any
formatted line longer than that is two writes or more, and another process's
write can land between them. Both lines were longer: impair's summary (one
fprintf) and the client's `glideslope: <url>: <url>: given up before it began` (gfx/fetches.cpp, one fprintf, a tile-fetch thread). Neither
program writes a line in pieces of its own making; the C library did.
A literal format with no conversions goes out in one write, which is why
impair's usage line never showed it.

*The fix.* `src/platform/whole_lines.hpp`: `whole_lines_to_stderr()`, called
first thing in main() beside `outlive_closed_pipes()` in every program that
calls that - the client, the server, the command-line client, impair,
datagram_check, hold_open and cache_collision - gives standard error a 4096-byte
line buffer, so a line goes out in one write at its newline; the stream's lock
is held across each call, so threads' lines are not mixed either. A no-op on
Windows. Lines without a newline wait for one, or for exit(), which flushes.

**Verification.**
- `a_line_written_to_a_stream_made_whole_lined_goes_out_in_one_write`
  (tests/unit/test_whole_lines.cpp): a stream whose every write the C library
  makes is counted (fopencookie, funopen on macOS), made whole-lined by the
  helper; every line length from 1 to 4096 bytes, coverage asserted, is one
  write of exactly that line; then four threads write 500 lines each of 129 to
  1128 bytes, and each of the 2,000 writes is one thread's whole line. Skipped
  on Windows, with its reason. Seen to fail: with the helper's setvbuf made
  `_IONBF` (standard error as it was), the 129-byte line went out as two
  writes and the test was red; reverted, green.
- On the real binary, under strace: `glideslope_impair 1 <a 150-byte host>`
  ("impair: not an address: ...") wrote it as a write of 128 and one of 65
  before the change, and as one write of 193 after.
- Plan line "the command-line forger test once failing after 300 s" (Phase 14b)
  is not this cause: a torn line fails the match at once, not after the
  test's time; it stays as it is.
- The linux-release suite, `ctest -LE nightly -j4`: see the pull request.

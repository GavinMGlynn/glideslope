### A multi-process test's programs never die of a closed pipe, 2026-09-29 — tail done

**The flake.** macOS release CI failed twice with a program killed by SIGPIPE
in runs that had otherwise gone well:
`prediction_interpolation_and_the_player_limit_hold_at_100_ms_with_jitter_and_loss`
(run 36513635725, exit codes `1;0;SIGPIPE;0;0`) and
`a_player_takes_over_an_ai_aircraft_with_no_step_at_200_ms_and_a_players_is_refused`
(run 36286521609, `0;0;SIGPIPE;0;0`).

**The cause.** Both scripts run their programs as one `execute_process`
pipeline, each one's standard output the next one's standard input. The third
program in each is the client straight to the server that leaves last. Its
goodbye is what empties the server, which `--until-empty` then ends; the
client then prints what it did (`said goodbye`, `stayed ...`), flushed at exit
into the pipe to the server that has just gone. With SIGPIPE at its default
action that write killed it. Which gets there first is a race, lost now and
then on macOS. Linux never lost it only by accident: libcurl's setup
(`platform/http_curl.cpp`) ignores SIGPIPE, and it had run in every such
client; the macOS backend (NSURLSession) leaves it be, and Windows has no
SIGPIPE. The exit code 1 in the first run is the client one too many, refused
as full, which is what the test asks of it - not a failure. The same race was
in every script where a client comes before the server (some twenty of them).

**The fix.** `platform/closed_pipes.hpp`: `outlive_closed_pipes()`, called
first thing in the main of `glideslope`, `glideslope_server` and
`glideslope_cli`, and in `glideslope_impair`, `glideslope_cache_collision` and
`glideslope_datagram_check`, ignores SIGPIPE, so a write to a pipe nobody reads
fails with EPIPE, as on Windows, and the program goes on. `glideslope_doc_client`,
`glideslope_http_stub` and `glideslope_ion_stall`, which link nothing of the
project, do the same themselves (ion_stall did it only when serving). What
SIGPIPE had also done was fail `server_impaired`/`server_take_over` if the
server was still running when the relay gave up at 290 s; the relay now says
that itself: with `--until-input-ends`, stopping for the time instead exits 1
(`impair: gave up after S s with its input still open`).

**Verification.**
- `every_program_a_test_pipes_into_another_outlives_the_reader_going`
  (`tests/cmake/closed_pipes.cmake`, `tests/tools/closed_pipe.cpp`; not on
  Windows, which has no SIGPIPE): each program is run once with its output read
  - it must write something and exit as it should - and once into a pipe whose
  reading end was closed before it started, with SIGPIPE put back to its
  default in it, so every write is to no reader and nothing is left to a race;
  it must exit the same. Each program's first write is the one tested, and
  that suffices: ignoring SIGPIPE holds for the whole process from main() on,
  and nothing sets it back. Coverage is asserted: the script reads every
  `execute_process` with more than one COMMAND in `tests/cmake` (33 pipelines,
  85 commands), every COMMAND wherever it is on its line; a literal program
  fails it, and each `${VAR}` is resolved per script from the
  `-DVAR=$<TARGET_FILE:...>` of the tests in `tests/CMakeLists.txt` that run
  that script (101 of them), so CHECK and CLIENT are each whatever they are
  there, and a variable no test resolves fails it. The targets found must be
  its table of 9 exactly; `CMAKE_COMMAND` is left out, not the project's, and
  so is `glideslope_hold_open`, whose outliving a closed pipe is what
  `impair_gives_up.cmake` asserts. Seen to fail before the fix: all 9 killed
  by signal 13; seen to fail on a literal program, a second COMMAND on a line,
  an unresolved variable, and a program taken out of the table. It found
  `glideslope_http_stub`, which a hand grep had missed.
- `the_relay_fails_when_it_gives_up_with_its_input_still_open`
  (`tests/cmake/impair_gives_up.cmake`): input held open by
  `glideslope_hold_open`, which writes a newline every tenth of a second and
  exits only when a write fails - when the relay has gone - so the relay's
  `--seconds 1` runs out with its input open however late a slow machine
  starts it: exit 1. Input ended at once by `cmake -E true`: exit 0. Seen to
  fail with the old exit of 0.
- Both at 100 ms, `server_impaired` and `server_take_over`, pass on Linux.

# A window client told why it waits says first that it has no aircraft

**A red tree, fixed.** `the_window_client_joining_on_final_behind_a_pa28_waits_told_why_and_is_at_the_gate_thirty_seconds_after_she_starts`
failed on Ubuntu linux-debug in ci.yml's scheduled run 43513776356 on main
(3c35ac0d), after 214.5 s. The window client printed "the server says:
waiting for the aircraft ahead: ..." before "the server has given it no
aircraft yet: waiting for one". The test wants the second line first. This is
the first failure of its kind in the 80 most recent failed ci.yml runs. The
two earlier failures of this test, on #359's branch on 2026-11-11, were the
gate distance, which was fixed there.

**Cause.** The server tells a waiting session why it waits as a reliable
`TRAFFIC_INSTRUCTION`. That message goes out at the same broadcast as the
session's first state update, just before it on the wire. `Online::join`
prints the "no aircraft yet" line when it takes a state update saying
`no_aircraft`, and prints the reason when it takes an instruction.
- When one poll takes in both, the update is printed first.
- On a slow runner the reason arrived in a poll of its own, ahead of the
  update, and was printed first.

Which line came first therefore depended on the machine. A reason to wait
already tells the client it has no aircraft.

**#362.** #362 (on-final-turns-complete) does not change this. It keeps the
same print order in `Online::join`, only adding a `waiting` callback, and
keeps the same ordering assertion in the test. It would fail the same way.

**Fix.**
- `Online::join` prints "the server has given it no aircraft yet" before the
  first reason it is told while it has none, if no update has said so first.
  The order is now the same whatever order the messages arrive in.
- The server has a new test flag, `--test-first-update-lost`. It sends no
  session its first state update, as though that update were lost, and says
  "a session's first state update is lost, as asked".
- The window run of the test now uses the flag and checks that the server
  said it. Its client therefore always hears the reason before any update:
  the CI case is built on every run, not left to the machine.
- `server_flags.cmake` lists the new flag.

**Verification.** Built on linux-debug. Run locally with
`LSAN_OPTIONS=exitcode=0`, because this machine's Vulkan driver makes the
window client report a 112-byte leak on exit even on main. CI's runs of this
test do not report it.
- With the flag and the old client, the test fails as CI did: "the server
  says: waiting for the aircraft ahead ..." comes before "no aircraft yet".
- With the fix, it passes. The three take-turns runs and
  `every_flag_the_server_prints_in_its_usage_is_one_it_takes` pass twice each
  under `-j4`.

**With #362.** #362 (on-final-turns-complete) lands first. This branch
conflicts with it in `tests/cmake/server_players_take_turns_on_final.cmake`,
on the window run's server command. #362 changes `--players-on-final
YSSY/16R` to `--players-on-final ${_spec}`, and this branch adds
`--test-first-update-lost`. Keep both:
`--players-on-final ${_spec} --test-first-update-lost`. In `Online::join`,
#362 adds a `waiting` callback (the window's `show_waiting`), which runs once
`said_waiting` is set. Both changes stay there too. With this fix, a reason
heard before any update sets `said_waiting`, so the window shows why she
waits from that moment. Without the fix it would show nothing until the
first update. Not yet rebased onto #362.

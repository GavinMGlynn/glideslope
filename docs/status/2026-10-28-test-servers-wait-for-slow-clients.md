### A test's server does not let a slow client go for a few seconds' silence, 2026-10-28

**What was wrong.** About half the multi-process tests started
`glideslope_server` with `--timeout 3` or `--timeout 5`, the wall-clock
silence after which it lets a client go. On a loaded runner a debug client
goes quiet that long (preparing its ground, say) and is let go, and the test
fails for a reason it was not testing: CI run 40660270759, Ubuntu linux-debug
tests 3/11, `a_players_copilot_glides_its_aircraft_on_a_server_when_the_engine_stops_as_recorded`
(server logged "let go ... after 5.0 s of silence" at 66.9 s, the test saw one
glide line, not two), and a STOVL first-input test run beside 40 busy loops,
whose client was silent 5 s preparing its ground. A client that leaves says
goodbye and is let go at once (2026-09-26), so a long timeout costs a test
nothing when its clients leave normally.

**What changed (test scripts and one new check only; no product code).**
Every `--timeout` not about the silence is now 30 s, the value several tests
already used. Read test by test, not by name.

| Timeout | Files | Kind |
|---|---|---|
| 5 raised to 30 | client_in_server_weather, client_let_go_of_a_nan, client_starved_f35b, prediction_let_go_of_a_nan, server_engine_prediction, server_first_input, server_hand_over_model, server_hand_over_planner, server_copilot, server_ground, server_paced_prediction, server_stovl_at_once, server_stovl_landing, server_stovl_vertical_landing, server_weather, server_weather_join, starved_f35b_placed, starved_prediction | (b) not about silence |
| 3 raised to 30 | client_rides_along (review round: none of its six tests checks silence past 3 s; client_kept_while_away holds that rule), client_copilot, client_hands_over, client_learnt_landing, client_loading, client_model_chosen_in_flight, client_on_server, server_aircraft_choice, server_drop_keeps_out, server_initiation_elsewhere, server_take_over (two servers), server_started_again (three servers) | (b) not about silence |
| kept short | client_joins_again (3), client_kept_while_away (2), server_behind (3), server_initiation_again (3), server_initiation_replayed_while_live (6), server_joins_again (3), server_stale_rejoin (3), server_timeout (0.5), server_leave (1, the silence way; 60 for goodbye) | (a) the test is the silence |
| kept short | tests/CMakeLists.txt `--timeout 2.5` and `0` with `--dry-run` | (a) arguments parsed, nothing listens |
| already 30 or more | server_ping, server_state, server_forged_refusal, client_forged_refusal, client_online, server_duplicate, server_one_key_two_addresses (30); client_online_window (60, 120), server_drop_every_session (60), server_restart_full (60), server_goodbye, server_doc_client, server_stranger_full (120) | (b) |

Why each kept short needs it: client_joins_again, server_joins_again and
server_stale_rejoin wait to be let go for silence and join again;
client_kept_while_away is a window kept away longer than the timeout and must
not be let go; server_behind runs the server at a
quarter of real time and asserts nobody is let go before flying, so a long
timeout would pass it for nothing; server_initiation_again and
server_initiation_replayed_while_live need a session let go for silence and
its initiation replayed; server_timeout pins the timeout itself; server_leave's
silence way is a client that leaves with no goodbye. No raised test ends by a
client vanishing without goodbye (`--no-goodbye` and `--lose-goodbyes` appear
only in the kept and already-long ones; server_started_again's lost mode
drops the client by the operator's drop, which needs no timeout).

**Made asserted.** `every_test_servers_silence_timeout_is_thirty_seconds_or_more_unless_the_test_is_about_the_silence`
(tests/cmake/test_server_timeouts.cmake) reads every `--timeout` outside
comments in tests/cmake/*.cmake and tests/CMakeLists.txt (a `${variable}` is
read through the numbers the file sets it to) and fails on any under 30 s whose
file is not named in its list with a reason; an exception with no short
timeout left fails too. Review round: the exceptions are per use (file, value, times), so a new short
timeout in an excepted file fails; every `--timeout` in any form is counted
and must be a use read or a named non-value occurrence (server_flags' list of
flags, the usage pattern and refusal pattern in tests/CMakeLists.txt, a
failure message in server_timeout); the totals are exact. It states: 60
timeouts, 48 at 30 s or more, 12 under it, each named exactly in 10 files;
63 occurrences in all, 4 of them not timeouts. Red: `--timeout=5` added to
server_ground failed it as unreadable; a second `--timeout 3` added to
server_joins_again failed it (named once, has two); both reverted. The six
client_rides_along tests, before at 3 s and after at 30 s (s): 84.2, 85.2,
85.2, 86.0, 51.9, 54.0 and 80.0, 80.6, 80.8, 80.8, 53.1, 50.2.

**Verification.**
- Red: before the edit the check named all 29 files; after, with
  `server_ground.cmake` put back to 5 it failed ("server_ground.cmake: gives a
  server --timeout under 30 s (5) and is not a named exception"); reverted,
  green.
- The 68 tests that run the 29 changed scripts, linux-debug, `-j4`, no
  display: before (main's scripts) and after, times in the logs below.
  Eight tests ask a live model and are skipped without keys, both times.
  Result: ctest reported 68 of 68 both times, of which 8 (the asking-a-live-model ones) were
  skipped for want of keys: skipped, never passed. Total test time 2485 s
  before, 2280 s after (the sum of the tests' own times 4552 s then 3989 s,
  load differing run to run). The slowest increase of any test was 12.3 s,
  `the_client_with_the_window_joins_its_server_started_again_and_flies_the_aircraft_it_gives`
  (60.1 to 72.4 s under `-j4`); run alone twice afterwards it took 56.4 s and
  56.1 s, so it was the load, not the timeout. The next largest was 3.9 s.
- The new check passes: 60 timeouts, 48 at 30 s or more, 12 under it in 10
  named exception files.

**What is still not true.** Only linux-debug was run here; the other
platforms' runs are CI's. Tests that wait on a silence the timeout does not
reach (a client frozen for more than 30 s) would still be let go, which is the
rule for 30 s and not a claim of none.

### The window client tested reordered, dropped unheard and restarted under; M chooses its hand-over model in flight; one model of a display, 2026-10-06 — five tails done

**What is not done** (still open in the plan): A pressed during a
take-over's round trip can still hand over the aircraft just left (the
server says nothing of a take-over it refuses, so the client cannot tell a
take-over pending from one refused); the window client's corrections are not
shown to stay bounded when built through the client itself, nor the steps CI
saw explained; and the slow-frames bound.

- **Reordered across a take-over.** `--late-update-after-take-over` on the
  window client, as on the command-line one: the last update naming the
  aircraft given up is kept (`Online::heard`) and heard again in the same
  `hear()` as the take-over, after it. The take-over test
  (`the_client_with_the_window_takes_over_the_ai_aircraft_it_rides_along_in`)
  passes it and holds that the client says the late update was heard and
  took over exactly once. **Seen to fail** with the newest-word guard taken
  out of the take-over branch: "took over aircraft 0", then "4" - "the
  client took over 2 times, not once"; reverted.
- **Dropped with every goodbye lost.** Server test flag `--lose-goodbyes`:
  the operator's drop sends no `LEAVING`, as though every copy were lost.
  `the_client_with_the_window_dropped_with_its_goodbyes_lost_is_refused_joining_again_and_says_so`
  (`tests/cmake/client_server_gone.cmake`, MODE lost): the client hears
  nothing, believes the server's `BAD_HANDSHAKE` after 3 s, joins again, is
  refused `DROPPED`, says "the server ended this session ...; not joining
  again", never says it joined again, exits 1, admitted once. A keeper
  (`glideslope_cli connect`, not flying) holds the `--until-empty` server
  open until the window client's new `--done FILE` exists - written as the
  program ends, however it ends. **Seen to fail** with `DROPPED` not ended
  on in `ClientSession::join_again` ("did not say it was dropped"; the client
  knocked for a minute); reverted.
- **Its server started again under it.** Server test flag
  `--stop-once-flown S`: stop, telling nobody, once a player's input has
  been flown and S simulated seconds have gone.
  `tests/cmake/server_started_again.cmake` runs one server with
  `--stop-once-flown 20` and then the same one again on its port with its
  key from `--store`, `--until-empty`, saying what both said on standard
  error. `the_client_with_the_window_joins_its_server_started_again_and_flies_the_aircraft_it_gives`
  (MODE restart, client `--shot-once-joined-again`, which holds the shot as
  `--stall-after` does until it has joined again and been flown): let go,
  joined the new server, flown by its inputs at the shot, admitted twice.
  The client now says the clock it joined again on - "joined again at X s on
  the server's clock, the old session's newest word at Y s" - and the test
  holds X < Y: the new aircraft taken from a clock started again, not waited
  out. **Seen to fail** with `reconciled_s_` not reset on joining again:
  "joined again at 20.0 s ... not before the old one's 20.0 s"; reverted.
  (Without the 20 s the bug passed: the first server stopped 3 s in.)
- **M chooses the hand-over model in flight**: none, Claude (`anthropic`),
  ChatGPT (`openai`) and round, each said ("a hand-over is planned by ...
  (M chooses)"); the copilot is made again with it, and a model with no key
  is refused at once, said. With `--copilot-provider` given, M says that
  chooses the model and changes nothing. Test flag `--next-model-after S`
  (what M does; may be given again). A shot after `--hand-over-after`, not
  taken back, now waits (the usual minute) for the server to say the AI has
  it. `the_client_with_the_window_pressing_m_{1,2,3}_times_in_flight_chooses_...`
  (`tests/cmake/client_model_chosen_in_flight.cmake`), with no key anywhere:
  each press's choice in turn, the whole cycle over the three; Claude and
  ChatGPT refused for want of a key, "planned by no model (X was refused:
  ...): the AI holds its course", and the server says the AI has it; none
  again asks nothing. **Seen to fail** with the copilot not made again after
  a choice (`copilot_made` left set): two presses, "openai with no key was
  not refused"; reverted.
- **One model of a display.** `OwnShown` moved to `src/frontend/shown.hpp`
  (namespace `glideslope::frontend`, presentation-free: positions and
  times) and `glideslope_cli`'s `Predicting` now shows, blends and measures
  its own aircraft through it, in place of its own copy: `seen()` for every
  other aircraft drawn, `taken_over()`, `switching()` at a hand-over or a
  take-back (new: a switch before the source changes - a take-back is two),
  `frame()`, and its counts for the report; `last_frame_s()` (new) for
  `--long-frame-after-switch`. One difference in behaviour, the window
  client's: a take-over of an aircraft
  never drawn as another blends from nothing, where the command-line copy
  blended from the aircraft left behind. `a_switch_through_long_frames_is_blended_without_a_step`
  (`tests/unit/test_shown.cpp`): a hand-over, a take-back and a take-over,
  each at frames of 1/60 s, 0.1, 0.25 and 0.4 s around it, 12 of 12 cases,
  each switch counted (a take-back as two) and its frames as long as said;
  unblended each steps 6 m, past 5 m (2.5 m at a take-over); the nearest
  its bound 0.296 m. **Seen to fail** with the blend not started at a
  switch: "a hand-over at 17 ms frames: 6.000 m"; reverted. The network
  checks that read the command-line client's display - the take-over at 100
  and 200 ms and `prediction_interpolation_and_the_player_limit_hold_*` -
  pass through it.
- `every_fixed_test_port_...` passes with the new ports (24707, 24736,
  24729, 24761, 24767 - the block has no free pair left, so these tests use
  no relay); `every_flag_the_server_prints_in_its_usage_is_one_it_takes`
  knows the two new server flags, and `--lose-goodbyes` without
  `--drop-once-flown` is refused (`the_server_refuses_lost_goodbyes_without_a_drop`,
  seen to fail with the refusal taken out).
- **On Windows**: `tools/windows_build.sh` built the branch (windows-debug,
  MSVC) and ran
  `the_client_with_the_window_dropped_with_its_goodbyes_lost_is_refused_joining_again_and_says_so`
  there: passed.

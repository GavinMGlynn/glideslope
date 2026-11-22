# The tests labelled nightly: run every night on main, on no pull request.
# (ci.yml: a pull request's ctest says -LE nightly.) A test is here when it
# failed on CI twice for the same cause that is not yet fixed; it comes out
# when the cause is fixed. tests/cmake/ci_shards.cmake fails unless the tests
# labelled nightly are exactly the ones named here, each with its reason and
# the CI runs that put it here.
#
# Each entry: "test name|reason|runs" (no semicolon in the text: it is a list).
set(NIGHTLY_TESTS
    "a_window_client_whose_imagery_is_refused_in_whatever_order_still_draws_its_frame_on_the_dems_colours|Its imagery refused, the client drew coarser than without imagery, cause not found (the open Phase 10b item)|Rocky 9 linux-release 40733120388 (24 tiles to level 7 against 26), Ubuntu linux-release 40811196271 (21 to level 6 against 26 to 7)"
    "a_client_joining_an_f35b_mid_bank_predicts_her_from_the_servers_law_state|Her median prediction error past its 60 mm bound on Windows debug, her jet-borne law's state drifting from the server's on a busy runner (the Phase 12a item bringing the F-35B tests moved under the busy-machine item back)|windows-debug 41284303752 (64 mm), #298's 41397416052 (61 mm)"
    "a_client_joining_an_f35b_mid_bank_with_the_first_datagrams_lost_still_begins_on_the_servers_law_state|Her jet-borne law's state, which no word carries, drifts from the server's while the joining client's first inputs have the selector off: pitch past its 0.5 degree bound (the Phase 12a item bringing the F-35B tests moved under the busy-machine item back)|ubuntu linux-debug 40800355368 (0.592), #279's 40992479828 (0.624) and 41006534168 (0.572), train 41009739851 (main's model)"
    "a_predicting_f35b_whose_client_stops_three_seconds_between_flying_and_sending_is_placed_within_four_steps_travel|On a loaded runner the server's words are placed steps off: past four steps' travel (the Phase 12a item bringing the F-35B tests moved under the busy-machine item back)|ubuntu linux-debug 40575068951 (10 steps off), windows-clang 41009739851 (6.893 m across the stall against 5.51)"
)
set(NIGHTLY_COUNT 4)

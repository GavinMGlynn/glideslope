# The tests labelled nightly: run every night on main, on no pull request.
# (ci.yml: a pull request's ctest says -LE nightly.) A test is here when it
# failed on CI twice for the same cause that is not yet fixed; it comes out
# when the cause is fixed. tests/cmake/ci_shards.cmake fails unless the tests
# labelled nightly are exactly the ones named here, each with its reason and
# the CI runs that put it here.
#
# Each entry: "test name|reason|runs" (no semicolon in the text: it is a list).
set(NIGHTLY_TESTS
    "a_client_predicting_against_a_server_held_up_unevenly_every_second_is_within_a_metre_for_ninety_five_updates_in_a_hundred|On Windows its 95th percentile went past its 1 m bound, from the server's word placed two steps off now and then (Windows' 15.6 ms timers, the open Phase 12a item)|windows-release 40743161009 (1,143 mm), 40673705998 (3.3 m worst)"
)
set(NIGHTLY_COUNT 1)

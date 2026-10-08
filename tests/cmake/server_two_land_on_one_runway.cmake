# server_two_land_on_one_runway.cmake - two AI aircraft on plans ending at
# one runway both land, the second once the first has left it.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DSTEPS=<steps to fly> -P server_two_land_on_one_runway.cmake
#
# **Built, not hoped for.** A server with two AI C172Ps flying the arrival
# plan (`sydney-arrival.plan`, ending in a landing on 16R), the first put on
# final to 16R three miles out (`--ai-on-final YSSY/16R`), the second flying
# the plan from over Chatswood - so the first is on the runway, or leaving
# it, before the second is down. Flown for STEPS steps as fast as they go:
# simulated time.
#
# **What is checked**: both are landed by the learnt landing (the server says
# where each touched down and stopped); each then leaves the runway and stops
# beside it (`has left`); the first has left before the second touches down;
# nothing is wrecked; and every two AI aircraft were kept 500 ft or 1.5 nm
# apart while both were in the air. Any go-around the server says is counted
# and shown. Before, the first stopped on 16R and stayed, and the second
# landed into it.
#
# Without the DEM's tiles or the runways it reports itself skipped (exit 77).

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}" --ai-on-final YSSY/16R
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --ai 2 --headless --data "${DATA}"
            --plan "${DATA}/plans/sydney-arrival.plan" --ai-on-final YSSY/16R
            --steps ${STEPS}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server exited ${_rc}:\n${_out}\n${_err}")
endif()
if(_out MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was wrecked:\n${_out}")
endif()

# Each landed, and where in the output: the touch, and the leaving.
string(REGEX MATCHALL "aircraft [0-9]+: the learnt landing touched down on [^\n]*" _touches "${_out}")
list(LENGTH _touches _touched)
if(NOT _touched EQUAL 2)
    message(FATAL_ERROR "${_touched} of the two AI aircraft were landed:\n${_out}")
endif()
string(REGEX MATCHALL "aircraft [0-9]+, [^\n]*, has left [^\n]*" _lefts "${_out}")
list(LENGTH _lefts _left)
if(NOT _left EQUAL 2)
    message(FATAL_ERROR "${_left} of the two AI aircraft left the runway, not both:\n${_out}")
endif()
list(GET _touches 0 _first_touch)
list(GET _touches 1 _second_touch)
list(GET _lefts 0 _first_left)
string(REGEX MATCH "^aircraft ([0-9]+)" _ "${_first_touch}")
set(_first ${CMAKE_MATCH_1})
string(REGEX MATCH "^aircraft ([0-9]+)" _ "${_second_touch}")
set(_second ${CMAKE_MATCH_1})
string(REGEX MATCH "^aircraft ([0-9]+)" _ "${_first_left}")
if(NOT CMAKE_MATCH_1 EQUAL _first OR _first EQUAL _second)
    message(FATAL_ERROR "the first down, aircraft ${_first}, was not the first to leave the "
                        "runway:\n${_out}")
endif()
string(FIND "${_out}" "${_first_left}" _at_left)
string(FIND "${_out}" "${_second_touch}" _at_second)
if(_at_left GREATER _at_second)
    message(FATAL_ERROR "the second touched down before the first had left the runway:\n${_out}")
endif()

if(NOT _out MATCHES "kept apart: ([0-9]+) pairs of AI aircraft over ([0-9]+) steps, separation lost for ([0-9]+) steps")
    message(FATAL_ERROR "the server did not say how its AI aircraft were kept apart:\n${_out}")
endif()
if(NOT CMAKE_MATCH_1 EQUAL 1 OR NOT CMAKE_MATCH_2 EQUAL ${STEPS} OR NOT CMAKE_MATCH_3 EQUAL 0)
    message(FATAL_ERROR "the two were not measured over every step and kept apart:\n${_out}")
endif()
string(REGEX MATCHALL "goes around from [^\n]*" _rounds "${_out}")
list(LENGTH _rounds _rounds)
message(STATUS "${_first_touch}")
message(STATUS "${_first_left}")
message(STATUS "${_second_touch}")
message(STATUS "both landed on one runway, the second once the first had left it; "
               "${_rounds} go-around(s)")

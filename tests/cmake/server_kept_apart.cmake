# server_kept_apart.cmake - every AI aircraft a server runs, planned or not,
# is kept apart from every other along the whole of its route.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DRECORDINGS=<dir of cbd-orbit-*.jsonl> -DSTEPS=<steps to fly>
#         -P server_kept_apart.cmake
#
# **Every AI aircraft a server runs**: four - the first two planned by
# OpenAI's model and Anthropic's, each played back from its recording of
# "take off, climb to 3,000 ft and orbit the CBD", both from 16R, the second
# 90 s after the first; the other two flying the plan file, the tour of
# Sydney Harbour, from off Bondi past the Harbour Bridge - a kilometre or two
# north of the orbits - to Olympic Park and back to the airport. Flown for
# STEPS steps as fast as they go: simulated time, not the machine's.
#
# **What is checked**: the server's account of every two AI aircraft - how
# close they came in a straight line, how close in height while within
# 1.5 nm of each other, and for how long they were within both 500 ft and
# 1.5 nm at once, separation lost (sim/separation.hpp). It must be never,
# for every pair. **Coverage is asserted**: four aircraft make six pairs,
# each aircraft is in three of them, the server measured at every one of the
# STEPS steps, and each pair at every step both were flying - all but the
# first 90 s for a pair with the second planned aircraft, which is not in the
# sky until then: so every pair over at least STEPS less three minutes. Both planned aircraft must have reached
# their orbits and flown round them, so that the run is the one the tail
# was found in - the second climbing to its height under the first's orbit
# - and nothing may be wrecked.
#
# Without the DEM's tiles or the runways it reports itself skipped (exit 77),
# unless GLIDESLOPE_REQUIRE_NETWORK is set (client.cmake).

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

execute_process(
    COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 4
            --ai-planner 1=openai:gpt-5.4-mini-2026-03-17
            --ai-playback "1=${RECORDINGS}/cbd-orbit-openai.jsonl"
            --ai-planner 2=anthropic:claude-haiku-4-5-20251001
            --ai-playback "2=${RECORDINGS}/cbd-orbit-anthropic.jsonl"
            --steps ${STEPS}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server did not fly its AI aircraft (exit ${_rc}):\n${_out}\n${_err}")
endif()
if(_out MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was wrecked:\n${_out}")
endif()
if(NOT _out MATCHES "ran 4 AI aircraft")
    message(FATAL_ERROR "the server did not run its four AI aircraft:\n${_out}")
endif()
foreach(_provider openai anthropic)
    if(NOT _out MATCHES "planned by ${_provider}; took off from [^,]+, handed over [0-9]+ ft above it; round [A-Za-z0-9_]+ ([0-9.]+) turns")
        message(FATAL_ERROR "the aircraft planned by ${_provider} did not reach its orbit:\n${_out}")
    endif()
    if(CMAKE_MATCH_1 LESS 1)
        message(FATAL_ERROR "the aircraft planned by ${_provider} went round its orbit only "
                            "${CMAKE_MATCH_1} times")
    endif()
endforeach()

# The whole: how many pairs, over how many steps, and separation lost for how long.
if(NOT _out MATCHES "kept apart: ([0-9]+) pairs of AI aircraft over ([0-9]+) steps, separation lost for ([0-9]+) steps")
    message(FATAL_ERROR "the server did not say how its AI aircraft were kept apart:\n${_out}")
endif()
set(_pairs "${CMAKE_MATCH_1}")
set(_measured "${CMAKE_MATCH_2}")
set(_lost "${CMAKE_MATCH_3}")
if(NOT _pairs EQUAL 6)
    message(FATAL_ERROR "${_pairs} pairs of AI aircraft were measured, not the six four make")
endif()
if(NOT _measured EQUAL ${STEPS})
    message(FATAL_ERROR "${_measured} steps were measured, not the ${STEPS} flown")
endif()

# Each pair, by name.
string(REGEX MATCHALL "apart: [^\n]*" _lines "${_out}")
list(LENGTH _lines _count)
if(NOT _count EQUAL 6)
    message(FATAL_ERROR "${_count} pairs were reported, not six:\n${_out}")
endif()
foreach(_n 1 2 3 4)
    set(_in 0)
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "\\(AI ${_n}[,)]")
            math(EXPR _in "${_in} + 1")
        endif()
    endforeach()
    if(NOT _in EQUAL 3)
        message(FATAL_ERROR "AI ${_n} is in ${_in} of the pairs reported, not three:\n${_out}")
    endif()
endforeach()
set(_failed "")
math(EXPR _least "${STEPS} - 180 * 120")
foreach(_line IN LISTS _lines)
    message(STATUS "${_line}")
    if(NOT _line MATCHES ", over ([0-9]+) steps, " OR CMAKE_MATCH_1 LESS _least)
        message(FATAL_ERROR "a pair was measured over fewer than ${_least} steps: ${_line}")
    endif()
    if(NOT _line MATCHES "separation lost for 0\\.00 s$")
        string(APPEND _failed "\n  ${_line}")
    endif()
endforeach()
if(NOT _failed STREQUAL "" OR NOT _lost EQUAL 0)
    message(FATAL_ERROR "AI aircraft came within 500 ft and 1.5 nm of each other, "
                        "for ${_lost} steps in all:${_failed}")
endif()
string(REGEX MATCHALL "is held (below|above) [-0-9]+ ft" _held "${_out}")
list(LENGTH _held _held)
message(STATUS "six pairs of four AI aircraft kept 500 ft or 1.5 nm apart over ${STEPS} steps; "
               "an aircraft was held off its height ${_held} times")

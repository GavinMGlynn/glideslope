# server_players_join_before_takeoff.cmake - two players who join while a
# planned AI aircraft is still on the ground are each given a layer of their
# own, clear of the plan-file AI aircraft's and of each other's.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DPLANNED=<dir of cbd-orbit-*.jsonl>
#         -P server_players_join_before_takeoff.cmake
#
# **Built, not hoped for.** A server with four AI aircraft: two planned by
# OpenAI's model and Anthropic's, played back from PLANNED, and two on the
# plan file, stacked by their AI number at the plan's start, +2,000 and
# +3,000 ft. The second planned aircraft waits 600 s for its turn
# (`--ai-spacing 600`), so it is on the ground all the time both players
# join, whenever the first takes off: the plan-file layers reach higher
# than the count of aircraft in the sky. Two clients join at once and stay
# 20 s of the session's clock, untouched. Counted over the sky alone, a
# player joining was put on the top plan-file aircraft's layer; counted over
# the plan-file layers alone, both players were put on the same one. Either
# way, wrecks at once. **Checked**: each client was told of at least five
# aircraft (both players and three AI aircraft), neither heard of a wreck,
# nothing in the server's account is a wreck, and all three programs ended
# as they should.
#
# Without the DEM's tiles it reports itself skipped (exit 77).

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake).
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/join-before-takeoff.sqlite")
set(_heard_a "${WORK}/heard-a.txt")
set(_heard_b "${WORK}/heard-b.txt")
file(REMOVE "${_store}" "${_heard_a}" "${_heard_b}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

# The clients first and the server last, so that what the server prints is
# what comes out of the pipeline; it stops once both have gone.
execute_process(
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 20
            --heard "${_heard_a}"
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 20
            --heard "${_heard_b}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --players 2 --headless
            --data "${DATA}" --store "${_store}" --ai 4 --ai-spacing 600
            --ai-planner 1=openai:gpt-5.4-mini-2026-03-17
            --ai-playback "1=${PLANNED}/cbd-orbit-openai.jsonl"
            --ai-planner 2=anthropic:claude-haiku-4-5-20251001
            --ai-playback "2=${PLANNED}/cbd-orbit-anthropic.jsonl"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")
set(_given 0)
foreach(_heard "${_heard_a}" "${_heard_b}")
    if(NOT EXISTS "${_heard}")
        message(FATAL_ERROR "a client heard nothing (exit codes ${_rcs}):\n${_out}\n${_err}")
    endif()
    file(READ "${_heard}" _said)
    if(_said MATCHES "is a wreck")
        message(FATAL_ERROR "a client heard of a wreck:\n${_said}\n${_out}")
    endif()
    # Both players there: each client told of at least five aircraft - the
    # two plan-file AI aircraft, the first planned one, and both players'.
    string(REGEX MATCHALL "aircraft [0-9]+ is c172p" _told "${_said}")
    list(REMOVE_DUPLICATES _told)
    list(LENGTH _told _count)
    if(_count LESS 5)
        message(FATAL_ERROR "a client was told of ${_count} aircraft, not the five or more "
                            "two players and three AI aircraft in the sky make:\n${_said}")
    endif()
    math(EXPR _given "${_given} + 1")
endforeach()
if(NOT _given EQUAL 2)
    message(FATAL_ERROR "${_given} clients were told of both players, not two")
endif()
if(_out MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was wrecked:\n${_out}")
endif()
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
message(STATUS "two players joined before the second planned aircraft took off; "
               "nothing was wrecked")

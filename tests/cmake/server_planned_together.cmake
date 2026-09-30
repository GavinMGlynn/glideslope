# server_planned_together.cmake - two planned aircraft wrecked together on
# one runway fly again one after the other, not onto each other for ever.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DRECORDINGS=<dir of cbd-orbit-*.jsonl> -DSTEPS=<steps to fly>
#         -P server_planned_together.cmake
#
# **The situation is built, not waited for.** Both recordings take off from
# 16R. Told to space their take-offs 0 s apart (`--ai-spacing 0`), the server
# stands both on the one threshold at once, and they collide on the first
# step - which this checks happened: two wrecks, each "collided". A planned
# aircraft flies again from its threshold, so before the fix both flew again
# in one step onto one point and collided again every 5 s for as long as the
# server ran. Now the first flies again and the second waits, a wreck, for
# the runway to be clear of it, and says so; then both take off and are
# handed over by the take-off autopilot, and nothing collides again.
#
# Without the DEM's tiles or the runways it reports itself skipped (exit 77),
# unless GLIDESLOPE_REQUIRE_NETWORK is set (client.cmake).

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

execute_process(
    COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 2 --ai-spacing 0
            --ai-planner 1=openai:gpt-5.4-mini-2026-03-17
            --ai-playback "1=${RECORDINGS}/cbd-orbit-openai.jsonl"
            --ai-planner 2=anthropic:claude-haiku-4-5-20251001
            --ai-playback "2=${RECORDINGS}/cbd-orbit-anthropic.jsonl"
            --steps ${STEPS}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server did not fly its planned aircraft (exit ${_rc}):\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "ran 2 AI aircraft")
    message(FATAL_ERROR "the server did not run its two AI aircraft:\n${_out}")
endif()

# The situation: both on 16R at once, and wrecked by each other.
string(REGEX MATCHALL "is a wreck: [^\n]*" _wrecks "${_out}")
list(LENGTH _wrecks _wrecked)
string(REGEX MATCHALL "is a wreck: collided with aircraft" _collided "${_out}")
list(LENGTH _collided _collisions)
if(_collisions LESS 2)
    message(FATAL_ERROR "the two planned aircraft did not collide on the runway, so the "
                        "situation this tests was not built:\n${_out}")
endif()
# And once only: flown again one after the other, not onto each other.
if(NOT _wrecked EQUAL 2)
    message(FATAL_ERROR "${_wrecked} wrecks, not the two of the one collision built - "
                        "they flew again onto each other:\n${_out}")
endif()
if(NOT _out MATCHES "waits for 16R to be clear of aircraft [0-9]+ before it flies again")
    message(FATAL_ERROR "neither aircraft said it waited for the runway:\n${_out}")
endif()

set(_flown 0)
foreach(_provider openai anthropic)
    if(NOT _out MATCHES "planned by ${_provider}; took off from 16R, handed over ([0-9]+) ft above it")
        message(FATAL_ERROR "the aircraft planned by ${_provider} did not take off again:\n${_out}")
    endif()
    if(CMAKE_MATCH_1 LESS 400)
        message(FATAL_ERROR "the aircraft planned by ${_provider} was handed over only "
                            "${CMAKE_MATCH_1} ft above the runway")
    endif()
    message(STATUS "planned by ${_provider}: wrecked once, flew again, handed over "
                   "${CMAKE_MATCH_1} ft above 16R")
    math(EXPR _flown "${_flown} + 1")
endforeach()
if(NOT _flown EQUAL 2)
    message(FATAL_ERROR "only ${_flown} of the two planned aircraft were checked")
endif()

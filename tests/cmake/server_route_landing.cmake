# server_route_landing.cmake - a copilot's route that ends in a landing is
# landed and leaves the runway; one landing on no runway is refused.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DROUTE=<route file> -DSTEPS=<steps to fly> [-DREFUSED=<regex>]
#         -P server_route_landing.cmake
#
# **Built, not hoped for.** A server with one AI C172P on the arrival plan
# (`sydney-arrival.plan`, from over Chatswood), given ROUTE at once as a
# player's copilot's route is (`--ai-route`): checked by the same checks and
# flown by the same code (fly_route_on) as a `COPILOT_ROUTE`, flown for STEPS
# steps as fast as they go - simulated time, which a player's client in a
# session could not give it.
#
# **What is checked**: the server says it flies the route, landing on
# YSSY_16R; the learnt landing touches down on YSSY 16R - the runway of the
# server's own collision ground the route named, not the route's numbers -
# and she leaves it and stops beside it; nothing is wrecked. **With
# REFUSED**: the server says the route is refused, why matching REFUSED, and
# she flies her plan file on.
#
# Without the DEM's tiles it reports itself skipped (exit 77).

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
            --plan "${DATA}/plans/sydney-arrival.plan"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --ai 1 --headless --data "${DATA}"
            --plan "${DATA}/plans/sydney-arrival.plan" --ai-route "${ROUTE}"
            --steps ${STEPS}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server exited ${_rc}:\n${_out}\n${_err}")
endif()

if(DEFINED REFUSED)
    if(NOT _out MATCHES "aircraft [0-9]+, an AI's, is refused the route: ([^\n]*)")
        message(FATAL_ERROR "the server did not refuse the route:\n${_out}")
    endif()
    set(_why "${CMAKE_MATCH_1}")
    if(NOT _why MATCHES "${REFUSED}")
        message(FATAL_ERROR "the route was refused for another reason than '${REFUSED}': "
                            "${_why}")
    endif()
    message(STATUS "refused: ${_why}")
    return()
endif()

if(NOT _out MATCHES "aircraft [0-9]+, an AI's, flies the route of 1: CENTRELINE_8NM, landing on YSSY_16R")
    message(FATAL_ERROR "the server did not fly the route to its landing:\n${_out}")
endif()
if(_out MATCHES "is a wreck")
    message(FATAL_ERROR "the aircraft was wrecked:\n${_out}")
endif()
if(NOT _out MATCHES "aircraft [0-9]+: the learnt landing touched down on YSSY 16R[^\n]*")
    message(FATAL_ERROR "the route's landing was not landed on YSSY 16R:\n${_out}")
endif()
set(_touched "${CMAKE_MATCH_0}")
if(NOT _out MATCHES "aircraft [0-9]+, [^\n]*, has left YSSY 16R, stopped beside it")
    message(FATAL_ERROR "she did not leave the runway:\n${_out}")
endif()
message(STATUS "${_touched}")
message(STATUS "${CMAKE_MATCH_0}")

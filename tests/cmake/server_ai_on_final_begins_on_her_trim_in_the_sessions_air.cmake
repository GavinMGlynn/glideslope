# server_ai_on_final_begins_on_her_trim_in_the_sessions_air.cmake - an AI
# aircraft put on final before the session's weather is had begins on the
# trim she is started with in that weather, not the one she had before it.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DPLAN=<plan naming the aircraft> -P server_ai_on_final_begins_on_her_trim_in_the_sessions_air.cmake
#
# **Built, not hoped for.** `--ai-on-final` puts the plan's AI aircraft on
# final as the fleet is made, in JSBSim's standard still air; `--metar` is
# had after it - air of 35 degrees and 1035 hPa, far from standard - and
# she, started in the air and not yet flown, is started again in it
# (Fleet::fly_in). She must say so with the trim she has then, and the last
# controls her controller says it begins on must be those, to the printed
# figure. **Seen to fail** with her controller not made again: it began on
# the standard air's trim.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}" --plan "${PLAN}"
            --ai-on-final YSSY/16R
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --ai 1 --headless --data "${DATA}" --plan "${PLAN}"
            --ai-on-final YSSY/16R --metar "YSSY 121200Z 00000KT 9999 SKC 35/20 Q1035"
            --steps 12
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server exited ${_rc}:\n${_out}\n${_err}")
endif()
set(_figures "throttle (-?[0-9.]+), elevator (-?[0-9.]+), pitch trim (-?[0-9.]+)")
if(NOT _out MATCHES "started again in the session's air: trimmed at ${_figures}")
    message(FATAL_ERROR "the AI on final was not started again in the session's air:\n${_out}")
endif()
set(_trim "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3}")
string(REGEX MATCHALL "on final, begins on ${_figures}" _begins "${_out}")
list(LENGTH _begins _n)
if(_n LESS 2)
    message(FATAL_ERROR "her controller was made ${_n} times, not again once the air was had:\n${_out}")
endif()
list(GET _begins -1 _last)
string(REGEX MATCH "begins on ${_figures}" _ignored "${_last}")
set(_began "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3}")
if(NOT _began STREQUAL _trim)
    message(FATAL_ERROR "her controller begins on ${_began}, not her trim in the session's air ${_trim}:\n${_out}")
endif()
list(GET _begins 0 _first)
string(REGEX MATCH "begins on ${_figures}" _ignored "${_first}")
set(_before "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3}")
if(_before STREQUAL _trim)
    message(FATAL_ERROR "her trim in standard air and in 35 degrees, 1035 hPa were the same (${_trim}): the test tells nothing:\n${_out}")
endif()
message(STATUS "begun on her trim in the session's air (${_trim}), not in standard air's (${_before})")

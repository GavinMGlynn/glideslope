# server_runways_slow.cmake - a server whose weather reports a runway's
# state keeps stepping at its rate while the runways that state lies on are
# still to be had.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -P server_runways_slow.cmake
#
# **A slow source, built.** As server_planned_runways.cmake does, the server
# is given a cache of its own with everything but OurAirports' runways, and
# GLIDESLOPE_RUNWAYS_SOURCE sends their fetch to http://127.0.0.1:1, where
# nothing listens: the fetch's five tries wait 2 + 4 + 8 + 16 s between them,
# about 30 s before the runways are known not to be had. The server flies a
# METAR with no runway state and, half a second in on the session's clock
# (`--metar-then`), one reporting ice on runway 16R - a change made on the
# stepping thread. It runs for 3 s of real time. **At its rate** it ends at
# most a second's worth of steps behind (a debug build on a loaded machine
# is a few tenths behind); the runways asked for on the stepping thread held
# it for the fetch's 30 s, and it ended thousands of steps behind (seen,
# 2026-10-14, before the runways were loaded off it).
#
# Without the DEM's tiles it reports itself skipped (exit 77), unless
# GLIDESLOPE_REQUIRE_NETWORK is set (client.cmake).

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_cache "${WORK}/cache")
file(REMOVE_RECURSE "${_cache}")
file(MAKE_DIRECTORY "${_cache}")
file(GLOB _entries RELATIVE "${CACHE}" "${CACHE}/*")
foreach(_entry IN LISTS _entries)
    if(NOT _entry STREQUAL "ourairports-runways.csv")
        file(CREATE_LINK "${CACHE}/${_entry}" "${_cache}/${_entry}" SYMBOLIC RESULT _linked)
        if(NOT _linked EQUAL 0)
            file(COPY "${CACHE}/${_entry}" DESTINATION "${_cache}")
        endif()
    endif()
endforeach()
set(ENV{GLIDESLOPE_CACHE} "${_cache}")
set(ENV{GLIDESLOPE_RUNWAYS_SOURCE} "http://127.0.0.1:1")

set(_dry "METAR YSSY 140600Z 18005KT 9999 FEW030 M02/M04 Q1012")
set(_icy "METAR YSSY 140630Z 18005KT 9999 FEW030 M02/M04 Q1012 R16R/790195")
string(TIMESTAMP _began "%s")
execute_process(
    COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 1 --seconds 3
            --metar "${_dry}" --metar-then 0.5 "${_icy}" --weather-blend 0
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 300)
string(TIMESTAMP _ended "%s")
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server stopped (exit ${_rc}):\n${_out}\n${_err}")
endif()
# The icy report was flown: the change came on the stepping thread.
if(NOT _out MATCHES "weather at 0\\.[0-9]+ s: [^\n]*R16R/790195")
    message(FATAL_ERROR "the report with a runway's state was never flown:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "was ([0-9]+) steps? behind at the end")
    message(FATAL_ERROR "the server did not say how far behind it ended:\n${_out}")
endif()
set(_behind "${CMAKE_MATCH_1}")
math(EXPR _took "${_ended} - ${_began}")
message(STATUS "the server ended ${_behind} steps behind, after ${_took} s")
if(_behind GREATER 120)
    message(FATAL_ERROR "the server ended ${_behind} steps behind - more than a second's "
                        "worth - while the runways were being fetched:\n${_out}\n${_err}")
endif()

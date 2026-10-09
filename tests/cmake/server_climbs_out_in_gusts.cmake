# server_climbs_out_in_gusts.cmake - an AI aircraft the server takes off on
# its model's plan climbs out half its weather's gust factor faster.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DRECORDINGS=<dir of cbd-orbit-anthropic.jsonl> -DMETAR=<report>
#         -DCLIMB=<kt expected> -P server_climbs_out_in_gusts.cmake
#
# The C172P's recorded CBD orbit, played back, flown in the METAR given
# (`--metar`): the server says the speed she climbs out at, which must be
# CLIMB - her 75.4 kt best climb plus half the METAR's gust spread
# (sim::in_gusts, world::gust_factor_kt), or 75.4 in a steady wind.
#
# Without the DEM's tiles or the runways it reports itself skipped (exit 77),
# unless GLIDESLOPE_REQUIRE_NETWORK is set (client.cmake).

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

execute_process(
    COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 1
            --ai-planner 1=anthropic:claude-haiku-4-5-20251001
            --ai-playback "1=${RECORDINGS}/cbd-orbit-anthropic.jsonl"
            --metar "${METAR}" --steps 12000
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server did not fly its planned aircraft (exit ${_rc}):\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "an AI's c172p, climbed out at ([0-9.]+) kt \\(75.4 in still air\\)")
    message(FATAL_ERROR "the server did not say the speed she climbs out at:\n${_out}\n${_err}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL "${CLIMB}")
    message(FATAL_ERROR "she climbs out at ${CMAKE_MATCH_1} kt, not ${CLIMB}:\n${_out}")
endif()
message(STATUS "climbs out at ${CMAKE_MATCH_1} kt in ${METAR}")

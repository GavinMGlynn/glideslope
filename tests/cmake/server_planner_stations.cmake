# server_planner_stations.cmake - a server's AI planner under the stations
# around it (`--weather around`) is told the runway its nearest station
# reports wet, and not a farther station's rain.
#
#   cmake -DSERVER=<glideslope_server> -DSTUB=<glideslope_http_stub>
#         -DDATA=<data dir> -DCACHE=<downloads dir> -DWORK=<scratch>
#         -DRECORDINGS=<dir of cbd-orbit-*.jsonl>
#         -DFORECAST_FILE=<recorded Open-Meteo answer> -DCASE=<near_wet|far_wet>
#         -P server_planner_stations.cmake
#
# **The stations from a stub, never the network.** glideslope_http_stub
# answers aviationweather.gov's box around the planner's airport, YSSY (the
# task's), with two stations built here: YSSY itself and YSBK, 17 km west.
# `near_wet`: YSSY reports rain and YSBK nothing falling - the planner is
# told the runways wet near YSSY. `far_wet`: the other way about - it is told
# no runway wet by YSSY's weather, YSBK's rain not told. The server says what
# its planners were told, and that is what is held here. The Open-Meteo
# forecast the server asks for once the aircraft fly is the recorded Sydney
# one, given three days of the same hours (server_stations.cmake). The model
# is played back from its recording, so no key is wanted; a request told of
# rain is not the recorded one, and that answer is refused - which is not
# what this tests.
#
# Without the DEM's tiles it reports itself skipped (exit 77), unless
# GLIDESLOPE_REQUIRE_NETWORK is set (client.cmake).
#
# With STUB_PORT_FILE, this is the inner half, started beside the stub: it
# waits for the stub's port, runs the server against it, writes what it said
# to RESULT_FILE, and stops the stub.

cmake_minimum_required(VERSION 3.28)

if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _stub LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:${_stub}")
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    execute_process(
        COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 1
                --ai-planner 1=openai:gpt-5.4-mini-2026-03-17
                --ai-playback "1=${RECORDINGS}/cbd-orbit-openai.jsonl"
                --weather around --steps 120
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    file(WRITE "${RESULT_FILE}" "=== exit ${_rc}\n=== out\n${_out}\n=== err\n${_err}\n")
    file(DOWNLOAD "http://127.0.0.1:${_stub}/stop" "${RESULT_FILE}.stop")
    return()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
file(MAKE_DIRECTORY "${WORK}")

# The box's answer: YSSY at its place, and YSBK 17 km west.
set(_rain "100600Z 29012KT 6000 -RA BKN015 14/12 Q1012")
set(_dry "100600Z 29012KT 9999 FEW030 14/08 Q1012")
if(CASE STREQUAL "near_wet")
    set(_yssy "${_rain}")
    set(_ysbk "${_dry}")
    set(_meant "planners at YSSY told by YSSY, its nearest station: the runways wet near it, and 0 runways reported contaminated")
elseif(CASE STREQUAL "far_wet")
    set(_yssy "${_dry}")
    set(_ysbk "${_rain}")
    set(_meant "planners at YSSY told by YSSY, its nearest station: no runway wet by its weather, and 0 runways reported contaminated")
else()
    message(FATAL_ERROR "CASE is near_wet or far_wet, not '${CASE}'")
endif()
set(_metars "${WORK}/metars.json")
file(WRITE "${_metars}"
     "[{\"icaoId\":\"YSSY\",\"rawOb\":\"METAR YSSY ${_yssy}\",\"lat\":-33.946,\"lon\":151.173,\"elev\":3},"
     "{\"icaoId\":\"YSBK\",\"rawOb\":\"METAR YSBK ${_ysbk}\",\"lat\":-33.924,\"lon\":150.989,\"elev\":9}]")

# **The forecast's hours, three days of them** (server_stations.cmake).
set(_had_epoch "")
if(DEFINED ENV{SOURCE_DATE_EPOCH})
    set(_had_epoch "$ENV{SOURCE_DATE_EPOCH}")
    unset(ENV{SOURCE_DATE_EPOCH})
endif()
string(TIMESTAMP _now "%s" UTC)
set(_days "")
foreach(_offset -86400 0 86400)
    math(EXPR _at "${_now} + ${_offset}")
    set(ENV{SOURCE_DATE_EPOCH} "${_at}")
    string(TIMESTAMP _day "%Y-%m-%d" UTC)
    list(APPEND _days "${_day}")
endforeach()
unset(ENV{SOURCE_DATE_EPOCH})
if(NOT _had_epoch STREQUAL "")
    set(ENV{SOURCE_DATE_EPOCH} "${_had_epoch}")
endif()
file(READ "${FORECAST_FILE}" _text)
if(NOT _text MATCHES "\"time\":\\[([^]]*)\\]")
    message(FATAL_ERROR "${FORECAST_FILE} has no hourly times")
endif()
set(_hours "${CMAKE_MATCH_1}")
set(_times "")
foreach(_day IN LISTS _days)
    string(REPLACE "\"2026-09-17T" "\"${_day}T" _these "${_hours}")
    list(APPEND _times "${_these}")
endforeach()
list(JOIN _times "," _times)
string(REPLACE "\"time\":[${_hours}]" "\"time\":@TIMES@" _text "${_text}")
string(REGEX REPLACE "\\[([^]]*)\\]" "[\\1,\\1,\\1]" _text "${_text}")
string(REPLACE "@TIMES@" "[${_times}]" _text "${_text}")
set(_forecast "${WORK}/forecast.json")
file(WRITE "${_forecast}" "${_text}")

set(_port_file "${WORK}/stub.port")
set(_result_file "${WORK}/result.txt")
file(REMOVE "${_port_file}" "${_result_file}")
execute_process(
    COMMAND "${STUB}" 404 "${_port_file}" /api/data/metar "${_metars}" /v1/forecast "${_forecast}"
    COMMAND "${CMAKE_COMMAND}" "-DSERVER=${SERVER}" "-DDATA=${DATA}" "-DCACHE=${CACHE}"
            "-DRECORDINGS=${RECORDINGS}" "-DSTUB_PORT_FILE=${_port_file}"
            "-DRESULT_FILE=${_result_file}"
            -P "${CMAKE_CURRENT_LIST_DIR}/server_planner_stations.cmake"
    RESULTS_VARIABLE _stub_rcs OUTPUT_VARIABLE _stub_out ERROR_VARIABLE _stub_said)
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the server was not run against the stub (${_stub_rcs}):\n${_stub_said}")
endif()
file(READ "${_result_file}" _ran)
if(NOT _ran MATCHES "=== exit ([^\n]*)\n=== out\n(.*)\n=== err\n(.*)\n$")
    message(FATAL_ERROR "the server's run was not written whole:\n${_ran}")
endif()
set(_rc "${CMAKE_MATCH_1}")
set(_out "${CMAKE_MATCH_2}")
set(_err "${CMAKE_MATCH_3}")
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server did not exit 0 (${_rc}):\n${_out}\n${_err}")
endif()
string(FIND "${_out}" "${_meant}\n" _at)
if(_at LESS 0)
    message(FATAL_ERROR "the planner was not told '${_meant}':\n${_out}\n${_err}")
endif()
# Its stations once, for the one planner, before it was planned.
string(REGEX MATCHALL "planners at [A-Z]+ told" _told "${_out}")
list(LENGTH _told _times_told)
if(NOT _times_told EQUAL 1)
    message(FATAL_ERROR "the planners were told ${_times_told} times, not once:\n${_out}")
endif()
message(STATUS "${CASE}: ${_meant}")

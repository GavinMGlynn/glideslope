# client_quits_mid_refresh.cmake - the flight quit while its weather refresh
# is turned away, or while it is part way through a transfer, ends at once.
#
#   cmake -DPROGRAM=<glideslope> -DSTUB=<glideslope_http_stub>
#         -DTIMER=<glideslope_exit_timer> -DDRIVER=<driver> -DWORK=<dir>
#         -DCACHE=<downloads dir> -DMETAR_FILE=<aviationweather json>
#         -DFORECAST_FILE=<open-meteo json> -DCASE=429|stalled -DWITHIN=<seconds>
#         -P client_quits_mid_refresh.cmake
#
# The flight screen is flown headless in the weather at Sydney, with the
# weather services' requests sent to glideslope_http_stub
# (GLIDESLOPE_WEATHER_SERVICE). The stub answers the flight's first fetch -
# the METAR and Open-Meteo's forecast, from files - and nothing after it
# properly: a flight fetches its weather again every 15 minutes of flying
# (weather_refresh_seconds), and that refresh is what is turned away. With
# CASE 429 it is answered 429 asking for ten seconds' wait, the most that is
# waited, so that a refresh not given up sits through 40 s of waits; with CASE
# stalled, its answer begins and never ends, which only the 60 s stall timeout
# would end, and then again for every retry.
#
# The shot is taken at tick 116000, 966 s in: a shot flown by ticks is 300
# frames, so the refresh, due at tick 108000, begins 20 frames before it. The
# client exits after writing the shot - quitting, as a person quitting does,
# with the refresh under way - and glideslope_exit_timer, reading its standard
# output, says how long after the shot's line it ended. That must be within
# WITHIN seconds, and the stub must have been asked by the refresh: turned it
# away, or held its transfer and seen it let go. A run in which the refresh
# never asked fails rather than passes.
#
# The forecast file is of 2026-09-17; its hours are given today's date, which
# a forecast is for. Without the DEM (fetched before the weather) the test is
# skipped, unless the network is required here.
#
# With STUB_PORT_FILE, this is the inner half, started beside the stub: it
# waits for the stub's port, flies, keeps what that did in RESULT_FILE, and
# stops the stub.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_shot_at 116000)
set(_said "glideslope: wrote tick")

# The inner half: the stub is listening, or about to be.
if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _port LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:${_port}")
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                            --screen flight --weather YSSY --autopilot
                            --shot-at ${_shot_at} --shot "${WORK}/quit-${DRIVER}-${CASE}.bmp"
                    COMMAND "${TIMER}" "${_said}"
                    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    file(WRITE "${RESULT_FILE}" "${_rcs}\n${_out}${_err}")
    file(DOWNLOAD "http://127.0.0.1:${_port}/stop" "${RESULT_FILE}.stop")
    return()
endif()

foreach(_needed PROGRAM STUB TIMER DRIVER WORK CACHE METAR_FILE FORECAST_FILE CASE WITHIN)
    if(NOT DEFINED ${_needed})
        message(FATAL_ERROR "client_quits_mid_refresh.cmake needs ${_needed}")
    endif()
endforeach()
if(CASE STREQUAL "429")
    set(_stub_case --retry-after 10 429)
elseif(CASE STREQUAL "stalled")
    set(_stub_case --stall 200)
else()
    message(FATAL_ERROR "CASE is 429 or stalled, not ${CASE}")
endif()

set(_dir "${WORK}/quit-mid-refresh")
file(MAKE_DIRECTORY "${_dir}")
set(_port_file "${_dir}/stub-${DRIVER}-${CASE}.port")
set(_result_file "${_dir}/stub-${DRIVER}-${CASE}.result")
set(_forecast "${_dir}/forecast-${DRIVER}-${CASE}.json")
file(REMOVE "${_port_file}" "${_result_file}")
# The forecast's hours, today.
string(TIMESTAMP _today "%Y-%m-%d" UTC)
file(READ "${FORECAST_FILE}" _text)
string(REPLACE "\"2026-09-17T" "\"${_today}T" _text "${_text}")
file(WRITE "${_forecast}" "${_text}")

# Both at once: the stub, and this script's inner half beside it. The first
# two requests answered from a file are the flight's first fetch; every one
# after is the refresh's.
execute_process(COMMAND "${STUB}" --files-for 2 ${_stub_case} "${_port_file}"
                        /api/data/metar "${METAR_FILE}" /v1/forecast "${_forecast}"
                COMMAND "${CMAKE_COMMAND}"
                        "-DPROGRAM=${PROGRAM}" "-DTIMER=${TIMER}" "-DDRIVER=${DRIVER}"
                        "-DWORK=${_dir}" "-DCACHE=${CACHE}" "-DCASE=${CASE}"
                        "-DSTUB_PORT_FILE=${_port_file}" "-DRESULT_FILE=${_result_file}"
                        -P "${CMAKE_CURRENT_LIST_FILE}"
                RESULTS_VARIABLE _stub_rcs ERROR_VARIABLE _stub_said)
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the client was not run against the stub (${_stub_rcs}):\n"
                        "${_stub_said}")
endif()
file(READ "${_result_file}" _result)
string(FIND "${_result}" "\n" _eol)
string(SUBSTRING "${_result}" 0 ${_eol} _rcs)
math(EXPR _eol "${_eol} + 1")
string(SUBSTRING "${_result}" ${_eol} -1 _ran)
list(GET _rcs 0 _rc)

if(NOT _rc EQUAL 0)
    if(_ran MATCHES "could not download" AND NOT _ran MATCHES "127\\.0\\.0\\.1" AND
       "$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "the DEM could not be had here, so this is skipped: ${_ran}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "glideslope exited ${_rc}:\n${_ran}\n${_stub_said}")
endif()
glideslope_judge_leaks("${_ran}")
if(NOT _stub_said MATCHES "and 2 with FILE")
    message(FATAL_ERROR "the flight's first fetch was not answered from the files:\n"
                        "${_stub_said}\n${_ran}")
endif()
# **The refresh asked**, and was turned away or held: otherwise nothing was
# under way to be quit.
if(CASE STREQUAL "429")
    if(NOT _stub_said MATCHES "answered ([1-9][0-9]*) requests 429")
        message(FATAL_ERROR "the refresh never asked, so nothing was quit mid-refresh:\n"
                            "${_stub_said}\n${_ran}")
    endif()
else()
    if(NOT _stub_said MATCHES "a transfer held was let go")
        message(FATAL_ERROR "no transfer of the refresh's was held and let go:\n"
                            "${_stub_said}\n${_ran}")
    endif()
    if(_stub_said MATCHES "still going")
        message(FATAL_ERROR "a transfer was still going when the client had ended:\n"
                            "${_stub_said}\n${_ran}")
    endif()
endif()
if(NOT _ran MATCHES "glideslope_exit_timer: ended ([0-9]+\\.[0-9]+) s after")
    message(FATAL_ERROR "the shot was never said:\n${_ran}")
endif()
set(_took "${CMAKE_MATCH_1}")
string(REGEX REPLACE "\\..*" "" _whole "${_took}")
if(_whole GREATER_EQUAL WITHIN)
    message(FATAL_ERROR "quit during a weather refresh (${CASE}), it took ${_took} s to "
                        "end, not within ${WITHIN} s:\n${_stub_said}")
endif()
message(STATUS "quit during a weather refresh (${CASE}), it ended ${_took} s after the "
               "shot: ${_stub_said}")

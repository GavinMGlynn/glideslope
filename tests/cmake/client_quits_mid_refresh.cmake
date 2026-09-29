# client_quits_mid_refresh.cmake - the flight quit while its weather refresh
# is turned away, or while it is part way through a transfer, ends as soon as
# a flight quit with no refresh under way does.
#
#   cmake -DPROGRAM=<glideslope> -DSTUB=<glideslope_http_stub>
#         -DTIMER=<glideslope_exit_timer> -DDRIVER=<driver> -DWORK=<dir>
#         -DCACHE=<downloads dir> -DMETAR_FILE=<aviationweather json>
#         -DFORECAST_FILE=<open-meteo json> -DCASE=429|stalled
#         -P client_quits_mid_refresh.cmake
#
# The flight screen is flown headless in the weather at Sydney, with the
# weather services' requests sent to glideslope_http_stub
# (GLIDESLOPE_WEATHER_SERVICE), twice:
#
#   - **The control**, shot at tick 600, long before the refresh a flight
#     makes every 15 minutes of flying (weather_refresh_seconds, tick 108000)
#     is due: what quitting costs on this machine, with nothing to give up.
#     It is short, as a quit's teardown does not grow with the flight, and a
#     flight of 108000 ticks is minutes of a debug build.
#   - **The quit**, shot at tick 108120, a second of flight after the refresh
#     is due. A shot flown by ticks is 300 frames of 360 ticks, so the
#     refresh begins in the shot's frame or the one before it, and it is
#     under way for 10 s (429) or 60 s (stalled) before it could end by
#     itself: frames, even a debug build's, are far shorter.
#
# The stub answers each flight's first fetch - the METAR, and Open-Meteo's
# forecast - from files (`--files-for 4`: two for each flight), and the
# refresh not at all. With CASE 429 it is answered 429 asking for ten
# seconds' wait, the most that is waited, so that a refresh not given up sits
# through 40 s of waits; with CASE stalled its answer begins and never ends,
# which only the 60 s stall timeout would end, and then again for each retry.
#
# **The situation is built, not hoped for.** At the shot the client says
# whether a weather refresh is under way; the quit must say it is, and the
# control that it is not. **Without imagery**: a shot waits for every
# terrain tile its view needs, and imagery is fetched from the network, whose
# timeouts on Windows CI's and the development machine's WinHTTP (12002) held
# the shot past the refresh's 40 s; the DEM is kept in CACHE. A machine slow enough to let the refresh end by
# itself before the shot - as a Windows debug build did, shot 21 frames after
# the refresh began - fails the test, as not having tested its rule; it never
# passes.
#
# The client exits after writing the shot - quitting, with the refresh under
# way - and glideslope_exit_timer, reading its standard output through a pipe,
# says how long after the shot's line it ended. That depends on the client
# flushing that line as it writes it (frontend/client/main.cpp). The quit must
# end within 2 s of the control's time, and within 10 s whatever the control
# took; a refresh not given up costs 30 s or more.
#
# **Whether the stalled transfer was given up is shown by the time alone.**
# The stub says it held a transfer under way, but cannot say the client let it
# go: the stub is stopped after the client has ended, when the system has
# closed the client's end of the connection either way.
#
# The forecast file is of 2026-09-17; it is given three days of the same
# hours - yesterday's, today's and tomorrow's, UTC - so that a run crossing
# midnight still finds its hour. Without the DEM (fetched before the weather)
# the test is skipped, unless the network is required here.
#
# With STUB_PORT_FILE, this is the inner half, started beside the stub: it
# waits for the stub's port, flies both, keeps what they did in RESULT_FILE,
# and stops the stub.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_said "glideslope: wrote tick")
set(_began "glideslope: fetching the weather again")

# The inner half: the stub is listening, or about to be.
if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _port LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:${_port}")
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    file(WRITE "${RESULT_FILE}" "")
    foreach(_run IN ITEMS control quit)
        if(_run STREQUAL "control")
            set(_shot_at 600)
        else()
            set(_shot_at 108120)
        endif()
        execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}"
                                --size 320x240 --screen flight --weather YSSY --autopilot --imagery off
                                --shot-at ${_shot_at}
                                --shot "${WORK}/quit-${DRIVER}-${CASE}-${_run}.bmp"
                        COMMAND "${TIMER}" "${_said}" "${_began}"
                        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        list(GET _rcs 0 _rc)
        file(APPEND "${RESULT_FILE}" "=== ${_run} exited ${_rc}\n${_out}${_err}\n")
        if(NOT _rc EQUAL 0)
            break()
        endif()
    endforeach()
    file(DOWNLOAD "http://127.0.0.1:${_port}/stop" "${RESULT_FILE}.stop")
    return()
endif()

foreach(_needed PROGRAM STUB TIMER DRIVER WORK CACHE METAR_FILE FORECAST_FILE CASE)
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

# **The forecast's hours, three days of them.** Each series is given three
# times over, and the times yesterday's, today's and tomorrow's: string
# (TIMESTAMP) reads SOURCE_DATE_EPOCH, if set, for the moment it formats.
string(TIMESTAMP _now "%s" UTC)
set(_days "")
foreach(_offset -86400 0 86400)
    math(EXPR _at "${_now} + ${_offset}")
    set(ENV{SOURCE_DATE_EPOCH} "${_at}")
    string(TIMESTAMP _day "%Y-%m-%d" UTC)
    list(APPEND _days "${_day}")
endforeach()
unset(ENV{SOURCE_DATE_EPOCH})
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
file(WRITE "${_forecast}" "${_text}")

# Both at once: the stub, and this script's inner half beside it.
execute_process(COMMAND "${STUB}" --files-for 4 ${_stub_case} "${_port_file}"
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
file(READ "${_result_file}" _ran)

foreach(_run IN ITEMS control quit)
    if(NOT _ran MATCHES "=== ${_run} exited ([0-9]+|[^\n]*)\n")
        message(FATAL_ERROR "the ${_run} was not flown:\n${_ran}\n${_stub_said}")
    endif()
    if(NOT CMAKE_MATCH_1 STREQUAL "0")
        if(_ran MATCHES "could not download" AND NOT _ran MATCHES "127\\.0\\.0\\.1" AND
           "$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
            message(STATUS "the DEM could not be had here, so this is skipped: ${_ran}")
            cmake_language(EXIT 77)
        endif()
        message(FATAL_ERROR "the ${_run}'s glideslope exited ${CMAKE_MATCH_1}:\n"
                            "${_ran}\n${_stub_said}")
    endif()
endforeach()
glideslope_judge_leaks("${_ran}")
string(FIND "${_ran}" "=== quit" _split)
string(SUBSTRING "${_ran}" 0 ${_split} _control)
string(SUBSTRING "${_ran}" ${_split} -1 _quit)

if(NOT _stub_said MATCHES "and 4 with FILE")
    message(FATAL_ERROR "each flight's first fetch was not answered from the files:\n"
                        "${_stub_said}\n${_ran}")
endif()
# **The refresh was under way when the quit came, and not in the control.**
if(NOT _control MATCHES "glideslope: a weather refresh is not under way")
    message(FATAL_ERROR "the control did not say it had no weather refresh under "
                        "way:\n${_control}")
endif()
if(NOT _quit MATCHES "glideslope: a weather refresh is under way")
    message(FATAL_ERROR "no weather refresh was under way at the quit, so this did "
                        "not test quitting mid-refresh:\n${_quit}\n${_stub_said}")
endif()
if(CASE STREQUAL "429" AND NOT _stub_said MATCHES "answered ([1-9][0-9]*) requests 429")
    message(FATAL_ERROR "the refresh was never turned away:\n${_stub_said}\n${_ran}")
endif()
if(CASE STREQUAL "stalled" AND NOT _stub_said MATCHES "holding a transfer under way")
    message(FATAL_ERROR "no transfer of the refresh's was held:\n${_stub_said}\n${_ran}")
endif()

# Seconds to thousandths, as whole milliseconds.
function(ended_ms text out)
    if(NOT text MATCHES "glideslope_exit_timer: ended ([0-9]+)\\.([0-9][0-9][0-9]) s after")
        message(FATAL_ERROR "the shot was never said:\n${text}")
    endif()
    math(EXPR _ms "${CMAKE_MATCH_1} * 1000 + 1${CMAKE_MATCH_2} - 1000")
    set(${out} ${_ms} PARENT_SCOPE)
endfunction()
ended_ms("${_control}" _control_ms)
ended_ms("${_quit}" _quit_ms)
math(EXPR _limit_ms "${_control_ms} + 2000")
set(_said_times "the control ended ${_control_ms} ms after its shot, the quit ${_quit_ms} ms")
if(_quit_ms GREATER _limit_ms OR _quit_ms GREATER 10000)
    message(FATAL_ERROR "quit during a weather refresh (${CASE}): ${_said_times}; more "
                        "than 2 s past the control's, or past 10 s:\n${_stub_said}")
endif()
message(STATUS "quit during a weather refresh (${CASE}): ${_said_times}")
if(_quit MATCHES "ended ([0-9]+\\.[0-9]+) s after \"glideslope: fetching the weather again\"")
    message(STATUS "the quit's refresh had begun ${CMAKE_MATCH_1} s before it ended")
endif()

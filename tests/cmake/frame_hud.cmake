# frame_hud.cmake - the HUD shows the flight's state at the tick it was shot.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_hud_check> -DCLI=<glideslope_cli>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir> -P frame_hud.cmake
#
# Flies the flight screen headless for 600 ticks - five seconds - in the
# weather reported at Sydney now, with --trace, shoots the last, and has
# glideslope_hud_check read the HUD back out of the frame and hold every number
# to the traced state, and the credits along the bottom to the DEM's, the
# imagery's and Open-Meteo's. The flight stands on the DEM, and draws it, so it needs the
# tiles and the geoid, fetched into CACHE, and the weather: without the network
# the test is skipped (exit 77) unless GLIDESLOPE_REQUIRE_NETWORK is set.
#
# **Without the weather it is skipped whatever is required.** The DEM and the
# geoid are pinned and kept, so CI requires them; the weather is live,
# somebody else's, and never kept, and when aviationweather.gov or Open-Meteo
# does not answer - WinHTTP's 12002 from Open-Meteo on Windows CI - there is
# nothing to fly in, which is not a fault of the HUD's. The client says which
# it was: "the weather could not be had". WEATHER_SERVICE, if given, is asked
# instead of both services (GLIDESLOPE_WEATHER_SERVICE), so a test can build
# that on purpose (frame_hud_no_weather.cmake).

#
# **With STUB, METAR_FILE and FORECAST_FILE, the weather is recorded**, not
# live: glideslope_http_stub answers aviationweather.gov's request with the
# METAR file's report from YSSY and Open-Meteo's with the forecast, made
# today's (recorded_weather.cmake), and the network is asked for nothing of
# the weather. The HUD tests on CI run so: on live weather the HUD tick test
# on Metal skipped after 334 s, five stalled requests to a service that was
# not answering (docs/status/2026-11-28-skipped-tests-say-why.md), and a test
# of the HUD has no need of today's weather. The flight is this script run
# again, beside the stub, with WEATHER_SERVICE its address. What a live
# service that does not answer does is frame_hud_no_weather.cmake's to test.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

# The inner half of a recorded run, started beside the stub: waits for its
# port, flies this script against it, keeps what that did in RESULT_FILE, and
# stops the stub.
if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _port LIMIT_COUNT 1)
    set(_flying "")
    if(DEFINED FLYING)
        set(_flying "-DFLYING=${FLYING}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}"
                            "-DPROGRAM=${PROGRAM}" "-DCHECK=${CHECK}" "-DCLI=${CLI}"
                            "-DDRIVER=${DRIVER}" "-DWORK=${WORK}" "-DCACHE=${CACHE}"
                            "-DFLY=${FLY}" ${_flying}
                            "-DWEATHER_SERVICE=http://127.0.0.1:${_port}"
                            -P "${CMAKE_CURRENT_LIST_FILE}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    file(WRITE "${RESULT_FILE}" "${_rc}\n${_out}${_err}")
    file(DOWNLOAD "http://127.0.0.1:${_port}/stop" "${RESULT_FILE}.stop")
    return()
endif()

if(DEFINED STUB AND NOT DEFINED WEATHER_SERVICE)
    include("${CMAKE_CURRENT_LIST_DIR}/recorded_weather.cmake")
    if(NOT DEFINED FLYING)
        set(_tag "pilot")
    else()
        string(MAKE_C_IDENTIFIER "${FLYING}" _tag)
    endif()
    set(_dir "${WORK}/recorded-weather")
    file(MAKE_DIRECTORY "${_dir}")
    set(_metar "${_dir}/metar-${DRIVER}-${_tag}.json")
    set(_forecast "${_dir}/forecast-${DRIVER}-${_tag}.json")
    set(_port_file "${_dir}/stub-${DRIVER}-${_tag}.port")
    set(_result_file "${_dir}/stub-${DRIVER}-${_tag}.result")
    file(REMOVE "${_port_file}" "${_result_file}")
    glideslope_recorded_weather("${METAR_FILE}" YSSY "${FORECAST_FILE}" "${_metar}" "${_forecast}")
    set(_flying "")
    if(DEFINED FLYING)
        set(_flying "-DFLYING=${FLYING}")
    endif()
    # Both at once: the stub, and this script's inner half beside it. Any
    # other request - none is expected - is answered 404, a failure.
    execute_process(COMMAND "${STUB}" 404 "${_port_file}"
                            /api/data/metar "${_metar}" /v1/forecast "${_forecast}"
                    COMMAND "${CMAKE_COMMAND}"
                            "-DPROGRAM=${PROGRAM}" "-DCHECK=${CHECK}" "-DCLI=${CLI}"
                            "-DDRIVER=${DRIVER}" "-DWORK=${WORK}" "-DCACHE=${CACHE}"
                            "-DFLY=${FLY}" ${_flying}
                            "-DSTUB_PORT_FILE=${_port_file}" "-DRESULT_FILE=${_result_file}"
                            -P "${CMAKE_CURRENT_LIST_FILE}"
                    RESULTS_VARIABLE _rcs ERROR_VARIABLE _stub_said)
    if(NOT EXISTS "${_result_file}")
        message(FATAL_ERROR "the HUD test was not run against the stub (${_rcs}):\n${_stub_said}")
    endif()
    file(READ "${_result_file}" _result)
    string(FIND "${_result}" "\n" _eol)
    string(SUBSTRING "${_result}" 0 ${_eol} _rc)
    math(EXPR _eol "${_eol} + 1")
    string(SUBSTRING "${_result}" ${_eol} -1 _said)
    message(STATUS "the HUD test, on recorded weather:\n${_said}")
    # **The recorded weather was what it flew in**: both services asked,
    # each answered from its file, and nothing else asked of it.
    if(NOT _stub_said MATCHES "answered 0 requests 404, and ([2-9]|[1-9][0-9]+) with FILE")
        message(FATAL_ERROR "the stub did not answer the METAR and the forecast, and "
                            "those alone, from its files:\n${_stub_said}")
    endif()
    # Weather not had, with the weather recorded, is a fault, not a skip.
    if(_said MATCHES "there is no weather to fly in")
        message(FATAL_ERROR "the recorded weather was not had: ${_stub_said}")
    endif()
    if(_rc EQUAL 77)
        # A download the network is not required for (the DEM, offline):
        # the inner run has said so.
        cmake_language(EXIT 77)
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the HUD test on recorded weather failed (${_rc}); the stub "
                            "said:\n${_stub_said}")
    endif()
    return()
endif()

file(MAKE_DIRECTORY "${WORK}")
# FLY, if given, is what else the client is told - `--autopilot`, or a
# `--plan` - and FLYING the HUD's line that must then say who is flying: the
# pilot, unless told otherwise.
if(NOT DEFINED FLYING)
    set(FLYING "FLYING PILOT")
endif()
string(MAKE_C_IDENTIFIER "${FLYING}" _case)
set(_shot "${WORK}/hud-${DRIVER}-${_case}.bmp")
set(_trace "${WORK}/hud-${DRIVER}-${_case}.trace.txt")
file(REMOVE "${_shot}" "${_trace}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
if(DEFINED WEATHER_SERVICE)
    set(ENV{GLIDESLOPE_WEATHER_SERVICE} "${WEATHER_SERVICE}")
endif()

# glideslope_client fails a test outright; a missing network is a skip, so the
# run is made here and its failure looked at.
set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 640x480
                        --screen flight --weather YSSY --shot-at 600 --shot "${_shot}" --trace
                        ${FLY}
                RESULT_VARIABLE _rc OUTPUT_FILE "${_trace}" ERROR_VARIABLE _err)
glideslope_skip_when_not_downloaded("${_rc}" "${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope exited ${_rc}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

# Standard output is the client's own and nothing else's. Cesium Native logs
# through spdlog, whose default logger writes to standard output unless it is
# told otherwise, and a log line landing mid-line cuts a trace line in half:
# "lat -33.94[2026-09-20 11:23:17.706] [error] [SqliteCache.cpp:592] database
# is locked" is what CI saw, and the numbers after it were gone. A log line
# carries a stamp no line of ours does, so that is what is looked for.
file(STRINGS "${_trace}" _stamped REGEX "\\[[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] ")
if(_stamped)
    list(GET _stamped 0 _first)
    message(FATAL_ERROR "something logged to the client's standard output, "
                        "where its own output goes: ${_first}")
endif()

# The altitude is above sea level: the traced height above the ellipsoid less
# the geoid there, which the CLI gives, in thousandths of a foot.
file(STRINGS "${_trace}" _last REGEX "^trace tick 600 ")
if(NOT _last MATCHES "lat (-?[0-9.]+) lon (-?[0-9.]+) alt_ft (-?[0-9]+)\\.([0-9][0-9][0-9]) ell_ft (-?[0-9]+)\\.([0-9][0-9][0-9]) ")
    message(FATAL_ERROR "no altitudes at tick 600 in the trace: ${_last}")
endif()
set(_lat "${CMAKE_MATCH_1}")
set(_lon "${CMAKE_MATCH_2}")
math(EXPR _alt "${CMAKE_MATCH_3} * 1000 + 1${CMAKE_MATCH_4} - 1000")
math(EXPR _ell "${CMAKE_MATCH_5} * 1000 + 1${CMAKE_MATCH_6} - 1000")
execute_process(COMMAND "${CLI}" height ${_lat} ${_lon}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _height ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR NOT _height MATCHES "geoid above the ellipsoid +(-?)([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "glideslope_cli height exited ${_rc}\n${_height}${_err}")
endif()
math(EXPR _geoid_mm "${CMAKE_MATCH_1}(${CMAKE_MATCH_2} * 1000 + 1${CMAKE_MATCH_3} - 1000)")
# Millimetres to thousandths of a foot: 0.3048 m a foot.
math(EXPR _geoid "${_geoid_mm} * 10000 / 3048")
math(EXPR _off "${_ell} - ${_geoid} - ${_alt}")
if(_off GREATER 3 OR _off LESS -3)
    message(FATAL_ERROR "the altitude is not above sea level: ${_alt} thousandths of a "
                        "foot, against ${_ell} above the ellipsoid less ${_geoid} of geoid")
endif()
message(STATUS "altitude above sea level: the ellipsoid's less the geoid's to "
               "${_off} thousandths of a foot")

execute_process(COMMAND "${CHECK}" "${_shot}" "${_trace}" 600 dem imagery weather
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()
# **Who is flying**, in the words expected: the check holds the line to the
# trace's pilot or AI, and this to what the AI is doing.
string(FIND "${_out}" "${FLYING}\n" _said)
if(_said LESS 0)
    message(FATAL_ERROR "the HUD did not say \"${FLYING}\":\n${_out}")
endif()

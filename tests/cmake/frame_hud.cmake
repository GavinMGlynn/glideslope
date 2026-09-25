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

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

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
if(NOT _rc EQUAL 0 AND _err MATCHES "the weather could not be had")
    message(STATUS "there is no weather to fly in: ${_err}")
    cmake_language(EXIT 77)
endif()
if(NOT _rc EQUAL 0 AND _err MATCHES "could not download")
    if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "the DEM or the weather could not be had: ${_err}")
        cmake_language(EXIT 77)
    endif()
endif()
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

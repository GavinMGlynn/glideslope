# server_weather_gusts.cmake - a client predicting in gusting air meets every
# gust at the moment the server met it, and predicts within the steady air's
# bound.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DSAME_M=<m> -P server_weather_gusts.cmake
#
# A server flies 30 kt gusting 45 from the west (`--metar`), and twelve
# seconds in 35 gusting 50 from the south-west, blended in over five
# (`--metar-then`, `--weather-blend`): server_weather.cmake's flight, in
# gusts. One player flies it, predicting its own aircraft through a relay
# holding every datagram 250 ms each way.
#
# **Why the moment is held, not only the error**: a gust is of the session's
# time, and a client meets it at the moment its clocks' difference says the
# server flies the step. With that moment five seconds wrong the median
# error was 0.150 m (2026-10-08), under the steady air's 0.25 m bound: the
# error alone does not tell a wrong gust clock. So the client keeps, for
# every step it flies forward, the moment it asked the air at, and holds it
# against the word's own step when the word comes (`gust clock:`). **Built,
# not hoped for**:
#
# - the client was told both gusting weathers - from 0 s, and from 12 s over 5;
# - it compared at least 300 updates in air, and every one was flown at the
#   server's moment: none a step out;
# - the prediction error's median is within SAME_M, server_weather.cmake's
#   steady bound.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake): another
# build's run of this test on the same port waits for it.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/weather_gusts.sqlite")
set(_ready "${WORK}/ready")
file(REMOVE "${_store}" "${_ready}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

# Sydney Airport, where the AI's plan flies.
set(_station -33.9461,151.1772,6)
set(_west "YSSY 020600Z 27030G45KT 9999 FEW030 20/10 Q1012")
set(_south_west "YSSY 020630Z 23035G50KT 9999 FEW030 19/10 Q1010")

set(_said_file "${WORK}/gusts.txt")
file(REMOVE "${_said_file}")
math(EXPR _relay "${PORT} + 4")
port_lock_hold(${_relay})
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 25
            --after-ready "${_ready}" --predict --heard "${_said_file}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 1 --data "${DATA}" --timeout 30 --store "${_store}"
            --ready-file "${_ready}"
            --metar "${_west}" --station ${_station}
            --metar-then 12 "${_south_west}" --weather-blend 5
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
file(READ "${_said_file}" _said)
foreach(_told IN ITEMS
        "told the weather: ${_west} \\(weather 1, from 0.000 s over 5 s\\)\n"
        "told the weather: ${_south_west} \\(weather 2, from 12.000 s over 5 s\\)\n")
    if(NOT _said MATCHES "${_told}")
        message(FATAL_ERROR "the client was not told '${_told}':\n${_said}")
    endif()
endforeach()
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9.]+) m[^\n]*, ([0-9.]+) s in")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
set(_worst ${CMAKE_MATCH_2})
set(_worst_at ${CMAKE_MATCH_3})
if(NOT _said MATCHES "prediction error median: ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say its median error:\n${_said}")
endif()
set(_median ${CMAKE_MATCH_1})
set(_clock_said "gust clock: ([0-9]+) updates compared in air, ([0-9]+) flown at the ")
string(APPEND _clock_said "server's moment and ([0-9]+) not, at most (-?[0-9]+) steps out")
if(NOT _said MATCHES "${_clock_said}")
    message(FATAL_ERROR "the client did not say its gust clock:\n${_said}")
endif()
set(_in_air ${CMAKE_MATCH_1})
set(_at_moment ${CMAKE_MATCH_2})
set(_off ${CMAKE_MATCH_3})
set(_most_off ${CMAKE_MATCH_4})
message(STATUS "in gusts, ${_in_air} updates compared in air, ${_at_moment} flown at the "
               "server's moment and ${_off} not (at most ${_most_off} steps out); the "
               "prediction error's median ${_median} m, its worst ${_worst} m at ${_worst_at} s")
if(_in_air LESS 300)
    message(FATAL_ERROR "the client compared ${_in_air} updates in air, fewer than the 300 a "
                        "flight of 20 s gives:\n${_said}")
endif()
if(NOT _off EQUAL 0)
    message(FATAL_ERROR "${_off} of ${_in_air} steps compared met the gusts at another moment "
                        "than the server's, at most ${_most_off} steps out:\n${_said}")
endif()
if(_median GREATER SAME_M)
    message(FATAL_ERROR "in gusts the prediction's median error was ${_median} m, more than "
                        "the steady air's ${SAME_M}")
endif()

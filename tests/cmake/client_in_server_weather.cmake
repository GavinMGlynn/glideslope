# client_in_server_weather.cmake - the client with the window, on a server
# flying a METAR, says it flies it, and predicts within the headless
# client's bound.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DDRIVER=<gpu driver>
#         -DSAME_M=<m> -P client_in_server_weather.cmake
#
# **The air of server_weather.cmake**: a 35 kt wind from the west at Sydney
# (`--metar`), and the client with the window joining through a relay
# holding every datagram 250 ms each way, so that a prediction in other air
# shows (that test says why: flying still air in it, the headless client's
# median was 0.78 m). **Built, not hoped for**: the client must say it flies
# the server's weather, that METAR, and at its shot twenty seconds in
# (`--shot-at 2400`) have compared at least 300 updates, with the median of
# its prediction error - measured as glideslope_cli measures it - within
# SAME_M: 0.1 m, inside the headless client's 0.25 there, thirty times the
# 0.003 m measured (2026-10-08) for a slower machine's margin, and a quarter
# of the 0.42 m it was flying still air.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/weather.sqlite")
set(_shot "${WORK}/weather.bmp")
set(_ready "${WORK}/flying")
file(REMOVE "${_store}" "${_shot}" "${_ready}")

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

set(ENV{LSAN_OPTIONS} "exitcode=0")
set(_station -33.9461,151.1772,6)
set(_west "YSSY 020600Z 27035KT 9999 FEW030 20/10 Q1012")
math(EXPR _relay "${PORT} + 4")
execute_process(
    # Each writes down the pipe to the next; the window client's are read.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" --ready-file "${_ready}"
            --metar "${_west}" --station ${_station}
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 2400 --view behind --after-ready "${_ready}"
            --server 127.0.0.1 ${_relay} --server-key ${_key}
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
list(GET _rcs -1 _rc)

if(NOT EXISTS "${_shot}")
    if(_err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "flying the server's weather \\(1\\): ${_west}\n")
    message(FATAL_ERROR "the client did not say it flies the server's weather, "
                        "'${_west}':\n${_out}")
endif()
if(NOT _out MATCHES "prediction error: ([0-9]+) updates compared, the median ([0-9.]+) m, the worst ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_out}")
endif()
set(_compared "${CMAKE_MATCH_1}")
set(_median "${CMAKE_MATCH_2}")
set(_worst "${CMAKE_MATCH_3}")
message(STATUS "the client with the window flew the server's ${_west}: its prediction "
               "error's median ${_median} m and worst ${_worst} m over ${_compared} updates")
if(_compared LESS 300)
    message(FATAL_ERROR "the client compared ${_compared} updates, fewer than the 300 a "
                        "flight of 20 s gives:\n${_out}")
endif()
if(_median GREATER SAME_M)
    message(FATAL_ERROR "in the server's weather the client with the window predicted with "
                        "a median error of ${_median} m, more than "
                        "${SAME_M}:\n${_out}")
endif()

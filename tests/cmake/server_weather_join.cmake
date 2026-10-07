# server_weather_join.cmake - a client joining while a weather blends in
# predicts within the same bound as one there before it.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DSAME_M=<m> -P server_weather_join.cmake
#
# A server flies a 35 kt wind from the west (`--metar`), and six seconds in
# a 40 kt one from the south-west blended in over two minutes
# (`--metar-then`, `--weather-blend`). Two players fly it, each predicting
# its own aircraft through a relay holding every datagram 250 ms each way
# (server_weather.cmake says why): **the first there before the change**,
# joining once the server is flying (`--ready-file`); **the second joining
# while the change blends in** - two seconds after the server has written
# that it changed (`--changed-file`), waiting on that, not on the clock.
# Told the new weather alone, the second flew it whole until the blend
# ended, 40 kt from the south-west where the server flew nearly 35 from the
# west. **Built, not hoped for**:
#
# - the first was told the session at under 6 s on the server's clock, and the
#   second at between 6 and 126 s - inside the blend;
# - each was told both weathers: the west from 0 s and the south-west from 6 s,
#   each over 120 s - the second the west first, though it had taken over
#   before it joined;
# - each compared at least 300 updates, and the median of each one's
#   prediction error is within SAME_M, the bound of server_weather.cmake.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/weather_join.sqlite")
set(_ready "${WORK}/ready")
set(_changed "${WORK}/changed")
file(REMOVE "${_store}" "${_ready}" "${_changed}")

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
set(_west "YSSY 020600Z 27035KT 9999 FEW030 20/10 Q1012")
set(_south_west "YSSY 020630Z 23040KT 9999 FEW030 19/10 Q1010")

set(_before "${WORK}/before.txt")
set(_during "${WORK}/during.txt")
file(REMOVE "${_before}" "${_during}")
math(EXPR _relay_before "${PORT} + 4")
math(EXPR _relay_during "${PORT} + 6")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay_before}" "${_key}" 25
            --after-ready "${_ready}" --predict --heard "${_before}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay_during}" "${_key}" 25
            --after-ready "${_changed}" --after 2 --predict --heard "${_during}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 2 --data "${DATA}" --timeout 5 --store "${_store}"
            --ready-file "${_ready}" --changed-file "${_changed}"
            --metar "${_west}" --station ${_station}
            --metar-then 6 "${_south_west}" --weather-blend 120
    COMMAND "${IMPAIR}" ${_relay_before} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    COMMAND "${IMPAIR}" ${_relay_during} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 2 --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
set(_joined_before_lo 0)
set(_joined_before_hi 6)
set(_joined_during_lo 6)
set(_joined_during_hi 126)
foreach(_who IN ITEMS before during)
    file(READ "${WORK}/${_who}.txt" _said)
    if(NOT _said MATCHES "told the session: [^\n]*, ([0-9.]+) s on its clock\n")
        message(FATAL_ERROR "the ${_who} client was not told the session's clock:\n${_said}")
    endif()
    set(_at ${CMAKE_MATCH_1})
    if(_at LESS _joined_${_who}_lo OR NOT _at LESS _joined_${_who}_hi)
        message(FATAL_ERROR "the ${_who} client joined at ${_at} s on the server's clock, "
                            "not from ${_joined_${_who}_lo} to ${_joined_${_who}_hi} s: "
                            "the situation this tests was not built:\n${_said}")
    endif()
    foreach(_told IN ITEMS
            "told the weather: ${_west} \\(weather 1, from 0.000 s over 120 s\\)\n"
            "told the weather: ${_south_west} \\(weather 2, from 6.000 s over 120 s\\)\n")
        if(NOT _said MATCHES "${_told}")
            message(FATAL_ERROR "the ${_who} client was not told '${_told}':\n${_said}")
        endif()
    endforeach()
    if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared")
        message(FATAL_ERROR "the ${_who} client did not say its prediction error:\n${_said}")
    endif()
    set(_compared_${_who} ${CMAKE_MATCH_1})
    if(NOT _said MATCHES "prediction error median: ([0-9.]+) m")
        message(FATAL_ERROR "the ${_who} client did not say its median error:\n${_said}")
    endif()
    set(_median_${_who} ${CMAKE_MATCH_1})
    set(_at_${_who} ${_at})
    if(_compared_${_who} LESS 300)
        message(FATAL_ERROR "the ${_who} client compared ${_compared_${_who}} updates, "
                            "fewer than the 300 a flight of 20 s gives:\n${_said}")
    endif()
endforeach()
message(STATUS "joined at ${_at_before} s, before the change, the prediction error's median "
               "was ${_median_before} m over ${_compared_before} updates; joined at "
               "${_at_during} s, mid-blend, ${_median_during} m over ${_compared_during}")
foreach(_who IN ITEMS before during)
    if(_median_${_who} GREATER SAME_M)
        message(FATAL_ERROR "the client joining ${_who} the blend predicted with a median "
                            "error of ${_median_${_who}} m, more than ${SAME_M}")
    endif()
endforeach()

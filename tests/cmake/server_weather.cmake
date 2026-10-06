# server_weather.cmake - a client flies the server's weather, not its own,
# and is told each change of it.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DSAME_M=<m> -DOWN_M=<m>
#         -P server_weather.cmake
#
# **The same air.** A server flies a 30 kt wind gusting 45 from the west,
# from a METAR given it (`--metar`), and twelve seconds in 35 gusting 50 from
# the south-west, blended in over five - gusts and turbulence that change from
# moment to moment, so that the air a client flies must be the air at the
# moment the server flies each step, a replayed step too: flown at one moment
# for every step of a frame, the client was out by 1.6 m (2026-10-06).
# (`--metar-then`, `--weather-blend`: a change of weather as a fetch again
# would bring, mid-flight.) Two players join
# it, each predicting its own aircraft and flying the same changing controls,
# each through a relay (`glideslope_impair`) holding every datagram 250 ms
# each way: a wind shows in a prediction by how far ahead of the server's
# word it flies, and on the loopback that is a few milliseconds, where a wind
# of other air moves an aircraft centimetres.
# The first flies the weather the server tells it; the second (`--own-air`)
# flies still air whatever it is told - which is what a client flying a
# weather of its own, other than the server's, does. **Built, not hoped for**:
#
# - both are told the collision ground, the session and the lobby, and the
#   weather twice - the 35 kt on joining and the 40 kt at its change, from
#   twelve seconds on the session's clock over five - so the change reached
#   every client;
# - the first's prediction error, where the server says its aircraft was
#   against where it had flown it, is within SAME_M over every update after the
#   first second of its own;
# - the second's is more than OWN_M: the wind is strong enough that predicting
#   in other air shows, which is what makes the first's bound mean something.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/weather.sqlite")
file(REMOVE "${_store}")

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

set(_same "${WORK}/same.txt")
set(_own "${WORK}/own.txt")
file(REMOVE "${_same}" "${_own}")
math(EXPR _relay_same "${PORT} + 4")
math(EXPR _relay_own "${PORT} + 6")
execute_process(
    # Each writes down the pipe to the next (server_impaired.cmake says how).
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay_same}" "${_key}" 25 --after 1
            --predict --heard "${_same}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay_own}" "${_key}" 25 --after 1
            --predict --own-air --heard "${_own}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 2 --data "${DATA}" --timeout 5 --store "${_store}"
            --metar "${_west}" --station ${_station}
            --metar-then 12 "${_south_west}" --weather-blend 5
    COMMAND "${IMPAIR}" ${_relay_same} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    COMMAND "${IMPAIR}" ${_relay_own} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 2 --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
# The server's own word goes down the pipe to the relays; that it changed its
# weather at 12 s is what each client says it was told, below.
foreach(_who IN ITEMS same own)
    file(READ "${WORK}/${_who}.txt" _said)
    foreach(_told IN ITEMS
            "told the collision ground: [^\n]*, this client's too\n"
            "told the session: glideslope_server on port ${PORT}, "
            "told the lobby: 2 players allowed"
            "told the weather: ${_west} \\(weather 1, from 0.000 s over 5 s\\)\n"
            "told the weather: ${_south_west} \\(weather 2, from 12.000 s over 5 s\\)\n")
        if(NOT _said MATCHES "${_told}")
            message(FATAL_ERROR "the ${_who} client was not told '${_told}':\n${_said}")
        endif()
    endforeach()
    if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9.]+) m")
        message(FATAL_ERROR "the ${_who} client did not say its prediction error:\n${_said}")
    endif()
    set(_compared_${_who} ${CMAKE_MATCH_1})
    set(_worst_${_who} ${CMAKE_MATCH_2})
    if(_compared_${_who} LESS 300)
        message(FATAL_ERROR "the ${_who} client compared ${_compared_${_who}} updates, "
                            "fewer than the 300 a flight of 20 s gives:\n${_said}")
    endif()
endforeach()
message(STATUS "flying the server's weather, the worst prediction error was ${_worst_same} m "
               "over ${_compared_same} updates; flying its own, ${_worst_own} m over "
               "${_compared_own}")
if(_worst_same GREATER SAME_M)
    message(FATAL_ERROR "flying the server's weather, the prediction was out by "
                        "${_worst_same} m, more than ${SAME_M}")
endif()
if(NOT _worst_own GREATER OWN_M)
    message(FATAL_ERROR "flying its own still air in the server's ${_west}, the prediction "
                        "was out by only ${_worst_own} m, not more than ${OWN_M}: the wind "
                        "does not show, and the bound above means nothing")
endif()


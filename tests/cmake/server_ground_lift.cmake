# server_ground_lift.cmake - a client predicting over hills in a strong wind
# flies the ground's lift the server flies, within the bound it holds over
# the sea.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DSAME_M=<m> -DLIFT_SHARE=<n>
#         -P server_ground_lift.cmake
#
# **The hills north of the Hawkesbury**, Brisbane Water's sandstone, 200 m of
# gorge and ridge, in a 50 kt southerly from a METAR given the server
# (`--metar`): the Cessna starts at 1,200 ft over them, flying north, where
# the plan does (`--plan`). The ground the lift is made from, 32 km up and down
# the wind, is all in the one DEM tile the tests download. Two players join,
# each predicting its own aircraft and flying the same changing controls,
# each through a relay holding every datagram 250 ms each way, as
# server_weather.cmake's do over the sea and the airport. **Built, not hoped
# for**:
#
# - both are told the collision ground and the weather, so both fly it;
# - the first flies the ground's lift, as the server does: its prediction's
#   median error, over every update after its first second, is within
#   SAME_M, the bound server_weather.cmake holds the same air to;
# - the second flies the same weather over no ground (`--no-ground-lift`),
#   with no lift from the hills - the server's air with the lift on one end
#   only - and the first's median is less than LIFT_SHARE of the second's:
#   the lift shows. **One run, both clients**, so a slow machine slows both.
#   Here (2026-11-19, a debug build) the first's median was 0.039 m and the
#   second's 0.097: 0.40 of it. The sea's bound alone would not tell them
#   apart; this does, and with the first client over no ground too it was
#   seen red.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/skip_or_fail.cmake")

# **Its port held machine-wide** while it runs (port_lock.cmake): another
# build's run of this test on the same port waits for it.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/lift.sqlite")
set(_plan "${WORK}/hills.plan")
file(REMOVE "${_store}")
file(WRITE "${_plan}" "aircraft c172p\n"
                      "start -33.4500 151.2000 1200 0 90\n"
                      "waypoint NORTH -33.3000 151.2000 1200 90\n")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}" --plan "${_plan}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
glideslope_warmed_up("${_rc}" "${_err}")

set(_station -33.9461,151.1772,6)
set(_south "YSSY 020600Z 18050KT 9999 FEW030 20/10 Q1012")

set(_lift "${WORK}/lift.txt")
set(_liftless "${WORK}/liftless.txt")
file(REMOVE "${_lift}" "${_liftless}")
math(EXPR _relay_lift "${PORT} + 4")
port_lock_hold(${_relay_lift})
math(EXPR _relay_liftless "${PORT} + 6")
port_lock_hold(${_relay_liftless})
execute_process(
    # Each writes down the pipe to the next (server_impaired.cmake says how).
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay_lift}" "${_key}" 25 --after 1
            --predict --heard "${_lift}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay_liftless}" "${_key}" 25 --after 1
            --predict --no-ground-lift --heard "${_liftless}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 2 --data "${DATA}" --timeout 30 --store "${_store}"
            --plan "${_plan}" --metar "${_south}" --station ${_station}
    COMMAND "${IMPAIR}" ${_relay_lift} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    COMMAND "${IMPAIR}" ${_relay_liftless} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 2 --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")
if(NOT _rcs STREQUAL "0;0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
foreach(_who IN ITEMS lift liftless)
    file(READ "${WORK}/${_who}.txt" _said)
    foreach(_told IN ITEMS
            "told the collision ground: [^\n]*, this client's too\n"
            "told the weather: ${_south} \\(weather 1, ")
        if(NOT _said MATCHES "${_told}")
            message(FATAL_ERROR "the ${_who} client was not told '${_told}':\n${_said}")
        endif()
    endforeach()
    if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9.]+) m")
        message(FATAL_ERROR "the ${_who} client did not say its prediction error:\n${_said}")
    endif()
    set(_compared_${_who} ${CMAKE_MATCH_1})
    set(_worst_${_who} ${CMAKE_MATCH_2})
    if(NOT _said MATCHES "prediction error median: ([0-9.]+) m")
        message(FATAL_ERROR "the ${_who} client did not say its median error:\n${_said}")
    endif()
    set(_median_${_who} ${CMAKE_MATCH_1})
    if(_compared_${_who} LESS 300)
        message(FATAL_ERROR "the ${_who} client compared ${_compared_${_who}} updates, "
                            "fewer than the 300 a flight of 20 s gives:\n${_said}")
    endif()
endforeach()
message(STATUS "flying the ground's lift, the prediction error's median was "
               "${_median_lift} m and its worst ${_worst_lift} m over ${_compared_lift} "
               "updates; over no ground, ${_median_liftless} m and ${_worst_liftless} m over "
               "${_compared_liftless}")
if(_median_lift GREATER SAME_M)
    message(FATAL_ERROR "flying the ground's lift over the hills, the prediction's median "
                        "error was ${_median_lift} m, more than ${SAME_M}")
endif()
# In millimetres, for math(): the medians are said to the millimetre.
foreach(_who IN ITEMS lift liftless)
    if(NOT _median_${_who} MATCHES "^([0-9]+)\\.([0-9][0-9][0-9])$")
        message(FATAL_ERROR "the ${_who} median, ${_median_${_who}}, is not in millimetres")
    endif()
    math(EXPR _mm_${_who} "${CMAKE_MATCH_1} * 1000 + 1${CMAKE_MATCH_2} - 1000")
endforeach()
if(NOT LIFT_SHARE MATCHES "^0\\.([0-9])$")
    message(FATAL_ERROR "LIFT_SHARE is a tenth, 0.1 to 0.9, not ${LIFT_SHARE}")
endif()
math(EXPR _share_tenths "${CMAKE_MATCH_1}")
math(EXPR _ten_lift "10 * ${_mm_lift}")
math(EXPR _share_of_liftless "${_share_tenths} * ${_mm_liftless}")
if(NOT _ten_lift LESS _share_of_liftless)
    message(FATAL_ERROR "flying the ground's lift, the prediction's median error was "
                        "${_median_lift} m, not less than ${LIFT_SHARE} of the "
                        "${_median_liftless} m flying the same weather over no ground: the "
                        "lift on both ends is not told from the lift on the server's alone")
endif()

# server_on_final_wind_limits.cmake - a player is put on final only in light
# wind: inside her measured limits she goes on final and is at her learnt
# landing's gate thirty seconds later; just outside them she starts where the
# plan does, and is told why.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DAIRCRAFT=<c172p|c182|j3cub|pa28> -P server_on_final_wind_limits.cmake
#
# **The ruling** (the owner, 2026-11-11; REQUIREMENTS 6.5): a server puts a
# player on final only where the wind is inside what she, left alone, stays
# at her gate in for thirty seconds. Those limits are measured and kept in
# her figures (`on_final_headwind_kts`, `on_final_tailwind_kts`,
# `on_final_crosswind_kts`), read here from the data, not written here.
#
# **Every case, counted**: four winds - straight ahead, straight behind, and
# across from the right and from the left of Sydney's 16R (heading 167.9) -
# each at her limit (calm where her limit is nought; still air, no weather,
# where her figures put her on final only in that) and one knot past it,
# the session's METAR. At her limit the command-line client, joined as the
# window client joins (`--predict --from-held`), touches nothing and asks for
# the learnt landing thirty seconds after her aircraft is given: the server
# must hand her over - its own gate check. Past it, the server must say she
# is not put on final and starts where the plan does, and her client must be
# told why. Eight cases; the test fails if it ran fewer.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

file(READ "${DATA}/figures/${AIRCRAFT}.xml" _figures)
foreach(_which headwind tailwind crosswind)
    if(NOT _figures MATCHES "on_final_${_which}_kts=\"([0-9]+)\"")
        message(FATAL_ERROR "the ${AIRCRAFT}'s figures give no on_final_${_which}_kts")
    endif()
    set(_${_which} "${CMAKE_MATCH_1}")
endforeach()
# **Or still air only** (`on_final_still_air_only`, the C182): inside is a
# session flying no weather, and past it any METAR - one knot each way.
set(_still OFF)
if(_figures MATCHES "on_final_still_air_only=\"true\"")
    set(_still ON)
endif()

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/limits.sqlite")
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
            --players-on-final YSSY/16R
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

# Direction the wind is from, and the limit it is judged by.
set(_cases "ahead:168:${_headwind}" "behind:348:${_tailwind}" "from the right:258:${_crosswind}"
           "from the left:078:${_crosswind}")
set(_ran 0)
set(_said "")
foreach(_case IN LISTS _cases)
    string(REPLACE ":" ";" _parts "${_case}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _from)
    list(GET _parts 2 _limit)
    math(EXPR _past "${_limit} + 1")
    foreach(_inside IN ITEMS ON OFF)
        if(_inside)
            set(_kts ${_limit})
        else()
            set(_kts ${_past})
        endif()
        if(_kts EQUAL 0)
            set(_wind "00000KT")
        else()
            string(LENGTH "${_kts}" _digits)
            if(_digits EQUAL 1)
                set(_wind "${_from}0${_kts}KT")
            else()
                set(_wind "${_from}${_kts}KT")
            endif()
        endif()
        set(_air --metar "YSSY 111200Z ${_wind} 9999 FEW030 20/10 Q1015"
                 --station -33.9461,151.1772)
        if(_still AND _inside)
            set(_air "")
            set(_kts "still air,")
        elseif(_still)
            set(_air --metar "YSSY 111200Z ${_from}01KT 9999 FEW030 20/10 Q1015"
                     --station -33.9461,151.1772)
            set(_kts 1)
        endif()
        set(_heard "${WORK}/heard.txt")
        set(_ready "${WORK}/flying")
        file(REMOVE "${_heard}" "${_ready}")
        execute_process(
            COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
                    --after-ready "${_ready}" --aircraft ${AIRCRAFT} --predict --from-held
                    --learnt-landing-at 30 --until-answered --heard "${_heard}"
            COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --ai 1 --headless
                    --data "${DATA}" --store "${_store}" --timeout 30 --ready-file "${_ready}"
                    --players-on-final YSSY/16R ${_air}
            RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        port_listened("${_out}" "${_err}")
        file(READ "${_heard}" _client)
        if(_inside)
            if(_out MATCHES "is not put on final")
                message(FATAL_ERROR "${AIRCRAFT}, ${_kts} kt ${_name} (her limit): not put on final:\n${_out}")
            endif()
            if(NOT _out MATCHES "handed to the learnt landing, on final to YSSY 16R, ([0-9.]+) miles out, (-?[0-9]+) m before the gate's near edge")
                message(FATAL_ERROR "${AIRCRAFT}, ${_kts} kt ${_name} (her limit): not at the gate 30 s on:\n${_out}")
            endif()
            string(APPEND _said " ${_kts} ${_name}: at the gate, ${CMAKE_MATCH_2} m;")
        else()
            if(NOT _out MATCHES "slot [0-9]+'s ${AIRCRAFT} is not put on final to YSSY 16R: ([^\n]*); it starts where the plan does\n")
                message(FATAL_ERROR "${AIRCRAFT}, ${_kts} kt ${_name} (past her limit): put on final:\n${_out}")
            endif()
            set(_why "${CMAKE_MATCH_1}")
            if(NOT _client MATCHES "told: not put on final to YSSY 16R: ")
                message(FATAL_ERROR "${AIRCRAFT}, ${_kts} kt ${_name}: her client was not told why:\n${_client}")
            endif()
            string(APPEND _said " ${_kts} ${_name}: not on final (${_why});")
        endif()
        math(EXPR _ran "${_ran} + 1")
    endforeach()
endforeach()
if(NOT _ran EQUAL 8)
    message(FATAL_ERROR "${_ran} of 8 cases ran")
endif()
message(STATUS "${AIRCRAFT}, limits ${_headwind} ahead, ${_tailwind} behind, ${_crosswind} across, "
               "8 of 8 cases:${_said}")

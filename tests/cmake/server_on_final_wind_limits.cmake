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
# **Every case, counted** (below): the session's METAR at each limit and a
# knot past it; the cases that reach their verdict are counted, and must be
# all of them. At her limit the command-line client, joined as the
# window client joins (`--predict --from-held`), touches nothing and asks for
# the learnt landing thirty seconds after her aircraft is given: the server
# must hand her over - its own gate check. Past it, the server must say she
# is not put on final and starts where the plan does, and her client must be
# told why.
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

# **The cases**, each "what|wind|in or out": at each limit (calm, once,
# where a limit is nought; still air, no weather flown, where her figures put
# her on final only in that) and a knot past it, ahead, behind, from the
# right and from the left of 16R (heading 167.9); a quartering wind with both
# its parts inside her limits, where she has a crosswind to quarter in; and
# for a crosswind limit of nought, 0.4 kt across (4 kt from 174, 4.0 ahead,
# inside her headwind), which any
# crosswind past nought must refuse.
#
# **Not swept, and why**: one temperature and pressure (the METAR's 20
# degrees and 1015 hPa, near the standard atmosphere at Sydney - a server's
# weather moves them, and the limits are for the wind); no gusts (a METAR
# without a gust group: her gust factor 0); her model's own loading; Sydney's
# 16R at 8 ft only - another field's height is another density.
set(_cases "")
set(_calm_listed OFF)
if(_still)
    list(APPEND _cases "still air|NONE|in")
endif()
foreach(_dir "ahead:168:${_headwind}" "behind:348:${_tailwind}"
             "from the right:258:${_crosswind}" "from the left:078:${_crosswind}")
    string(REPLACE ":" ";" _parts "${_dir}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _from)
    list(GET _parts 2 _limit)
    math(EXPR _past "${_limit} + 1")
    if(_still)
        list(APPEND _cases "1 kt ${_name}|${_from}01KT|out")
        continue()
    endif()
    if(_limit EQUAL 0)
        if(NOT _calm_listed)
            list(APPEND _cases "calm|00000KT|in")
            set(_calm_listed ON)
        endif()
    else()
        string(LENGTH "${_limit}" _digits)
        if(_digits EQUAL 1)
            list(APPEND _cases "${_limit} kt ${_name}|${_from}0${_limit}KT|in")
        else()
            list(APPEND _cases "${_limit} kt ${_name}|${_from}${_limit}KT|in")
        endif()
    endif()
    string(LENGTH "${_past}" _digits)
    if(_digits EQUAL 1)
        list(APPEND _cases "${_past} kt ${_name}|${_from}0${_past}KT|out")
    else()
        list(APPEND _cases "${_past} kt ${_name}|${_from}${_past}KT|out")
    endif()
endforeach()
if(NOT _still AND _crosswind EQUAL 0)
    list(APPEND _cases "0.4 kt across (4 from 174)|17404KT|out")
endif()
# Quartering, both parts at her limits as near as a METAR's whole knots and
# degrees give: 5 kt from 190 for the C172P (4.7 ahead, 1.97 across), 13
# from 181 for the PA-28 (12.7 ahead, 2.9 across);
# none for the Cub (no crosswind) or the C182 (no weather).
if(AIRCRAFT STREQUAL "c172p")
    list(APPEND _cases "quartering 5 kt from 190|19005KT|in")
elseif(AIRCRAFT STREQUAL "pa28")
    list(APPEND _cases "quartering 13 kt from 181|18113KT|in")
endif()
list(LENGTH _cases _wanted)

set(_verdicts 0)
set(_said "")
set(_two_told OFF)
foreach(_case IN LISTS _cases)
    string(REPLACE "|" ";" _parts "${_case}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _wind)
    list(GET _parts 2 _expect)
    set(_air --metar "YSSY 111200Z ${_wind} 9999 FEW030 20/10 Q1015"
             --station -33.9461,151.1772)
    if(_wind STREQUAL "NONE")
        set(_air "")
    endif()
    set(_heard "${WORK}/heard.txt")
    set(_heard_b "${WORK}/heard_b.txt")
    set(_ready "${WORK}/flying")
    file(REMOVE "${_heard}" "${_heard_b}" "${_ready}")
    # **Two sessions, once**: in the first case past a limit a second player
    # joins too, and each is told why once - her own reason, kept on her
    # session, never the other's.
    set(_second OFF)
    if(_expect STREQUAL "out" AND NOT _two_told)
        set(_second ON)
        execute_process(
            COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
                    --after-ready "${_ready}" --aircraft ${AIRCRAFT} --predict --from-held
                    --learnt-landing-at 30 --until-answered --heard "${_heard}"
            COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
                    --after-ready "${_heard}" --aircraft ${AIRCRAFT} --learnt-landing-at 30
                    --until-answered --heard "${_heard_b}"
            COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --ai 1 --headless
                    --data "${DATA}" --store "${_store}" --timeout 30 --ready-file "${_ready}"
                    --players-on-final YSSY/16R ${_air}
            RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        port_listened("${_out}" "${_err}")
    else()
        execute_process(
            COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
                    --after-ready "${_ready}" --aircraft ${AIRCRAFT} --predict --from-held
                    --learnt-landing-at 30 --until-answered --heard "${_heard}"
            COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --ai 1 --headless
                    --data "${DATA}" --store "${_store}" --timeout 30 --ready-file "${_ready}"
                    --players-on-final YSSY/16R ${_air}
            RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        port_listened("${_out}" "${_err}")
    endif()
    file(READ "${_heard}" _client)
    if(_expect STREQUAL "in")
        if(_out MATCHES "is not put on final")
            message(FATAL_ERROR "${AIRCRAFT}, ${_name} (inside her limits): not put on final:\n${_out}")
        endif()
        if(_out MATCHES "handed to the learnt landing, on final to YSSY 16R, ([0-9.]+) miles out, (-?[0-9]+) m before the gate's near edge")
            math(EXPR _verdicts "${_verdicts} + 1")
            string(APPEND _said " ${_name}: at the gate, ${CMAKE_MATCH_2} m;")
        else()
            message(FATAL_ERROR "${AIRCRAFT}, ${_name} (inside her limits): not at the gate 30 s on:\n${_out}")
        endif()
    else()
        if(NOT _out MATCHES "slot [0-9]+'s ${AIRCRAFT} is not put on final to YSSY 16R: ([^\n]*); it starts where the plan does\n")
            message(FATAL_ERROR "${AIRCRAFT}, ${_name} (past her limit): put on final:\n${_out}")
        endif()
        set(_why "${CMAKE_MATCH_1}")
        string(REGEX MATCHALL "told: not put on final to YSSY 16R: " _told "${_client}")
        list(LENGTH _told _told_n)
        if(NOT _told_n EQUAL 1)
            message(FATAL_ERROR "${AIRCRAFT}, ${_name}: her client was told why ${_told_n} times, not once:\n${_client}")
        endif()
        if(_second)
            set(_two_told ON)
            file(READ "${_heard_b}" _client_b)
            string(REGEX MATCHALL "told: not put on final to YSSY 16R: " _told_b "${_client_b}")
            list(LENGTH _told_b _told_b_n)
            if(NOT _told_b_n EQUAL 1)
                message(FATAL_ERROR "${AIRCRAFT}, ${_name}: the second session was told why ${_told_b_n} times, not once:\n${_client_b}")
            endif()
            if(NOT _client MATCHES "aircraft ([0-9]+) told: not put on final")
                message(FATAL_ERROR "no aircraft in what the first was told:\n${_client}")
            endif()
            set(_a_number "${CMAKE_MATCH_1}")
            if(_client_b MATCHES "aircraft ${_a_number} told: not put on final")
                message(FATAL_ERROR "the second session was told the first's reason:\n${_client_b}")
            endif()
            string(APPEND _said " (and a second session, told its own once);")
        endif()
        math(EXPR _verdicts "${_verdicts} + 1")
        string(APPEND _said " ${_name}: not on final (${_why});")
    endif()
endforeach()
if(NOT _verdicts EQUAL _wanted)
    message(FATAL_ERROR "${_verdicts} of ${_wanted} cases reached their verdict")
endif()
message(STATUS "${AIRCRAFT}, limits ${_headwind} ahead, ${_tailwind} behind, ${_crosswind} across, "
               "${_verdicts} of ${_wanted} cases:${_said}")

# server_players_take_turns_on_final.cmake - players joining a server that
# starts them on final each start in the learnt landing's gate, in their
# turn, and each is still at the gate thirty seconds after she starts.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DORDERS=<a|b|...> -DRUN=<which of them> -P server_players_take_turns_on_final.cmake
#
# **Built, not hoped for.** The server starts every player on final to
# Sydney's 16R (`--players-on-final YSSY/16R`), each where her own gate holds
# her thirty seconds - and a player who joins while another player's
# aircraft is flying within three miles of that point waits, given no
# aircraft (her updates say `no_aircraft`), until it is three miles on or
# landed: the radar minimum in trail on a final approach (FAA JO 7110.65,
# 5-5-4), between two light aeroplanes with no wake minimum to add. Until
# 2026-11-11 the second, third and fourth started 2.9, 3.4 and 3.9 miles out,
# outside the gate (1.7 to 2.4 miles).
#
# ORDERS is a list of runs, `|` between them, each the aeroplanes of its
# players in the order they join, `,` between them; RUN says which this
# test flies. All of them join at once, a second apart, through the
# command-line client as the window client does (`--predict --from-held`:
# every input what the server held her on - a player who touches nothing),
# and each asks for the learnt landing thirty seconds after her aircraft is
# given (`--learnt-landing-at 30`, counted from then). The server hands her
# over only if its own aircraft is at the gate - and the test holds her at
# least 100 m before its near edge. Each but the last stays until the
# learnt landing has her at rest, and goes (`--until-landed`); the last goes
# once handed over (`--until-answered`). A player after the first must have
# been told to wait, and been given her aircraft only after the one ahead
# was handed over.
#
# **Coverage, asserted.** Across all the runs ORDERS names, every aeroplane
# with a learnt landing in the data (rl/NAME-landing.txt) must join second
# or later at least once, and the second, third and fourth places must each
# be flown: so 2, 3 and 4 players. The first place is
# client_stays_at_gate.cmake's.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake): another
# build's run of this test on the same port waits for it.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

# **The space**: every aeroplane with a learnt landing, as a follower; the
# second to fourth places.
file(GLOB _policies RELATIVE "${DATA}/rl" "${DATA}/rl/*-landing.txt")
set(_gated "")
foreach(_p IN LISTS _policies)
    string(REGEX REPLACE "-landing\\.txt$" "" _p "${_p}")
    list(APPEND _gated "${_p}")
endforeach()
list(SORT _gated)
string(REPLACE "|" ";" _runs "${ORDERS}")
set(_followers "")
set(_places "")
foreach(_run IN LISTS _runs)
    string(REGEX REPLACE "\\+[a-z]+$" "" _run "${_run}")
    string(REPLACE "," ";" _order "${_run}")
    list(LENGTH _order _n)
    math(EXPR _last "${_n} - 1")
    foreach(_i RANGE 1 ${_last})
        list(GET _order ${_i} _a)
        list(APPEND _followers "${_a}")
        math(EXPR _place "${_i} + 1")
        list(APPEND _places "${_place}")
    endforeach()
endforeach()
list(REMOVE_DUPLICATES _followers)
list(SORT _followers)
list(REMOVE_DUPLICATES _places)
list(SORT _places)
if(NOT _followers STREQUAL "${_gated}")
    message(FATAL_ERROR "the aeroplanes with a learnt landing are ${_gated}, and the runs fly "
                        "${_followers} after the first: every one must follow at least once")
endif()
if(NOT _places STREQUAL "2;3;4")
    message(FATAL_ERROR "the runs fly the places ${_places} after the first; 2, 3 and 4 wanted")
endif()
list(LENGTH _gated _gated_n)

list(GET _runs ${RUN} _run)
# **How one joins** (`+window`, `+restart`): the last, the window client's
# own program, run --headless as every window test runs it, waiting with no
# aircraft and told why, then given hers; or the second, the command-line
# client begun again on the same key while it waits - its first session
# going silent, as a client killed does - after the third has joined, and
# it must keep its place ahead of the third.
set(_how "")
if(_run MATCHES "\\+([a-z]+)$")
    set(_how "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "\\+[a-z]+$" "" _run "${_run}")
endif()
string(REPLACE "," ";" _order "${_run}")
list(LENGTH _order _n)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/turns.sqlite")
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

# The clients, each writing down the pipe to the next; the server, last, to
# here. **In their order, by events**: the first once the server is flying
# (`--ready-file`), and each after once the one before has heard something
# (its `--heard` file exists) - admitted, not a second later on the clock.
set(_commands "")
set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
math(EXPR _last "${_n} - 1")
foreach(_i RANGE ${_last})
    list(GET _order ${_i} _a)
    if(_i EQUAL 0)
        set(_after "${_ready}")
    elseif(_i EQUAL 2 AND _how STREQUAL "restart")
        set(_after "${WORK}/heard_first.txt")
    else()
        math(EXPR _before "${_i} - 1")
        set(_after "${WORK}/heard${_before}.txt")
    endif()
    set(_leaves --until-landed)
    if(_i EQUAL _last)
        set(_leaves --until-answered)
    endif()
    file(REMOVE "${WORK}/heard${_i}.txt")
    if(_i EQUAL _last AND _how STREQUAL "window")
        continue()
    endif()
    if(_i EQUAL 1 AND _how STREQUAL "restart")
        # Its first session: given up on, silent, eight seconds in; begun
        # again once the third has joined behind it.
        set(_secret "1111111111111111111111111111111111111111111111111111111111111111")
        file(REMOVE "${WORK}/heard_first.txt")
        list(APPEND _commands COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8
             --after-ready "${_after}" --aircraft ${_a} --key ${_secret} --no-goodbye
             --heard "${WORK}/heard_first.txt")
        set(_after "${WORK}/heard2.txt")
        set(_again --key ${_secret})
    else()
        set(_again "")
    endif()
    list(APPEND _commands COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 900
         --after-ready "${_after}" --aircraft ${_a} --predict --from-held --learnt-landing-at 30
         ${_leaves} ${_again} --heard "${WORK}/heard${_i}.txt")
endforeach()
if(_how STREQUAL "window")
    # **Built slowly on purpose** (`--slow-start 5`): its flight begun five
    # seconds after the server gave her, as a Windows debug client's was
    # (CI run 42930877921: 89 m from the gate's near edge) - and still asked
    # 30 s after she was given on the session's clock, not after its own
    # flight began.
    # The window client last, through a relay that holds nothing back
    # (glideslope_impair, at PORT + 1), which passes the server's standard
    # output on to standard error: the client's is read from the one and the
    # server's from the other.
    math(EXPR _relay "${PORT} + 1")
    port_lock_hold(${_relay})
    math(EXPR _before "${_last} - 1")
    list(GET _order ${_last} _a)
    # **Told why before any update** (`--test-first-update-lost`): every
    # session's first state update is lost on the way, so the window client
    # hears the reason it waits - sent with that update - before it hears an
    # update saying it has no aircraft. It must still say it has none first.
    # The order it heard them in was left to the machine until 2026-11-14,
    # and on a slow runner (CI run 43513776356) the reason came first in a
    # poll of its own, and was said first.
    execute_process(
        ${_commands}
        COMMAND "${SERVER}" --port ${PORT} --seconds 1800 --until-empty --ai 1 --headless
                --data "${DATA}" --store "${_store}" --timeout 30 --ready-file "${_ready}"
                --players-on-final YSSY/16R --test-first-update-lost
        COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
                --seed 1 --until-input-ends --seconds 1800
        COMMAND "${WINDOW}" --headless --gpu-driver "${DRIVER}" --size 320x200
                --aircraft ${_a} --shot "${WORK}/window.bmp" --shot-at 999999
                --quit-at 4800 --view cockpit --press-after 30 L --slow-start 5
                --after-ready "${WORK}/heard${_before}.txt"
                --server 127.0.0.1 ${_relay} --server-key ${_key}
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _window ERROR_VARIABLE _out)
    port_listened("${_window}" "${_out}")
    if(_out MATCHES "no GPU|could not create|no device")
        message(STATUS "the window client cannot draw here: ${_out}")
        cmake_language(EXIT 77)
    endif()
    file(WRITE "${WORK}/window.txt" "${_window}")
else()
    execute_process(
        ${_commands}
        COMMAND "${SERVER}" --port ${PORT} --seconds 1800 --until-empty --ai 1 --headless
                --data "${DATA}" --store "${_store}" --timeout 30 --ready-file "${_ready}"
                --players-on-final YSSY/16R
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    port_listened("${_out}" "${_err}")
endif()
file(WRITE "${WORK}/server.txt" "${_out}")
# The first session of one begun again is ended by the server for the newer
# on its key, and says so, exiting 1: as it should.
if(_how STREQUAL "restart")
    list(REMOVE_AT _rcs 1)
    file(READ "${WORK}/heard_first.txt" _first)
    if(NOT _first MATCHES "taken over by a newer session for this key")
        message(FATAL_ERROR "the first session was not ended for the newer:\n${_first}")
    endif()
endif()
foreach(_rc IN LISTS _rcs)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}")
    endif()
endforeach()

# **The window client waited, was told why, and was given her**: no
# aircraft yet, the server's reason, and then hers, in that order.
if(_how STREQUAL "window")
    if(NOT _out MATCHES "a session's first state update is lost, as asked\n")
        message(FATAL_ERROR "the server did not lose a first update as asked:\n${_out}")
    endif()
    if(NOT _window MATCHES "the server has given it no aircraft yet: waiting for one\n(.*\n)?glideslope: the server says: waiting for the aircraft ahead: aircraft [0-9]+, [0-9.]+ miles from where she starts, until it is 3 miles on or landed, at most [0-9]+ s more\n(.*\n)?glideslope: the server gave this client aircraft [0-9]+, the ${_a}\n")
        message(FATAL_ERROR "the window client did not wait, told why, and then fly:\n${_window}")
    endif()
    if(NOT _window MATCHES "pressed L, 30\\.[0-4] s in")
        message(FATAL_ERROR "the window client did not ask 30 s after it was given her:\n${_window}")
    endif()
endif()
# **The session begun again kept her turn**: its first session was told to
# wait, and went silent; the second, on its key, begun after the third had
# joined, was given her - and before the third (her place, asserted below
# as each follower is given hers only after the one ahead is handed over).
if(_how STREQUAL "restart")
    file(READ "${WORK}/heard1.txt" _heard)
    if(NOT _heard MATCHES "given its aircraft, [0-9]+, [0-9]+ s after joining")
        message(FATAL_ERROR "the session begun again was never given her aircraft:\n${_heard}\n${_out}")
    endif()
endif()


# **Each player, by her aeroplane, read from where she is given it on**: a
# slot is a place in the lobby's order of keys, not a player, and an
# aircraft's number is given again once the aircraft that had it has gone,
# so neither names one player through a run.
string(REGEX MATCHALL "waits for its turn on final: " _waits "${_out}")
list(LENGTH _waits _waited)
if(NOT _waited EQUAL _last)
    message(FATAL_ERROR "${_waited} players were told to wait their turn, and ${_last} join "
                        "behind another:\n${_out}")
endif()
set(_said "")
set(_handed_before -1)
foreach(_i RANGE ${_last})
    list(GET _order ${_i} _a)
    math(EXPR _place "${_i} + 1")
    string(FIND "${_out}" " asked for ${_a}, and flies it\n" _given)
    if(_given EQUAL -1)
        message(FATAL_ERROR "place ${_place}: the ${_a} was never given:\n${_out}")
    endif()
    string(SUBSTRING "${_out}" ${_given} -1 _since)
    if(NOT _since MATCHES "^ asked for ${_a}, and flies it\n([^\n]*\n)*slot [0-9]+'s ${_a} starts on final to YSSY 16R (2\\.[23][0-9]) miles out\n")
        message(FATAL_ERROR "place ${_place}, the ${_a}: not started where her gate holds her:\n${_since}")
    endif()
    set(_from "${CMAKE_MATCH_2}")
    if(_i GREATER 0)
        # Her own turn: the first said after she was given her aeroplane.
        string(FIND "${_since}" "'s turn on final: aircraft " _turn_at)
        if(_turn_at EQUAL -1)
            message(FATAL_ERROR "place ${_place}, the ${_a}: her turn never came:\n${_since}")
        endif()
        string(SUBSTRING "${_since}" ${_turn_at} -1 _turn_line)
        if(NOT _turn_line MATCHES "^'s turn on final: aircraft ([0-9]+), ([0-9.]+) s")
            message(FATAL_ERROR "place ${_place}, the ${_a}: her turn unread:\n${_turn_line}")
        endif()
        set(_number "${CMAKE_MATCH_1}")
        set(_turn "given at ${CMAKE_MATCH_2} s")
        # **Let by at touchdown or at three miles, never sooner**: the one
        # ahead landed, or 3 miles or more from her start.
        if(NOT _turn_line MATCHES "^'s turn on final: aircraft [0-9]+, [0-9.]+ s on the session's clock: aircraft [0-9]+ (landed|3 miles on), ([0-9.]+) miles from her start\n")
            message(FATAL_ERROR "place ${_place}, the ${_a}: let by other than at touchdown or "
                                "three miles:\n${_turn_line}")
        endif()
        set(_let "${CMAKE_MATCH_1} ${CMAKE_MATCH_2}")
        if(CMAKE_MATCH_1 STREQUAL "3 miles on" AND CMAKE_MATCH_2 LESS 3.0)
            message(FATAL_ERROR "place ${_place}, the ${_a}: let by at ${CMAKE_MATCH_2} miles")
        endif()
        string(APPEND _turn " (the one ahead ${_let} miles from her start)")
        if(_given LESS _handed_before)
            message(FATAL_ERROR "place ${_place}, the ${_a}: given her aircraft before the one "
                                "ahead was handed over:\n${_out}")
        endif()
    else()
        if(NOT _since MATCHES "^[^\n]*\n([^\n]*\n)*slot [0-9]+'s ${_a} starts on final[^\n]*\nadmitted [0-9a-f]+ to slot")
            message(FATAL_ERROR "the first was not given her aircraft at once:\n${_out}")
        endif()
        set(_number 0)
        set(_turn "given at once")
    endif()
    if(_since MATCHES "aircraft ${_number} not handed to the learnt landing: ([^\n]*)")
        message(FATAL_ERROR "place ${_place}, the ${_a}: left alone, not at the gate thirty "
                            "seconds after she started: ${CMAKE_MATCH_1}")
    endif()
    if(NOT _since MATCHES "aircraft ${_number} handed to the learnt landing, on final to YSSY 16R, ([0-9.]+) miles out, (-?[0-9]+) m before the gate's near edge")
        message(FATAL_ERROR "place ${_place}, the ${_a}: never handed over:\n${_since}")
    endif()
    set(_then "${CMAKE_MATCH_1}")
    set(_spare "${CMAKE_MATCH_2}")
    if(_spare LESS 100)
        message(FATAL_ERROR "place ${_place}, the ${_a}: only ${_spare} m before the gate's near edge")
    endif()
    string(FIND "${_since}" "aircraft ${_number} handed to the learnt landing" _handed_at)
    math(EXPR _handed_before "${_given} + ${_handed_at}")
    if(NOT (_i EQUAL _last AND _how STREQUAL "window"))
        file(READ "${WORK}/heard${_i}.txt" _heard)
        if(NOT _heard MATCHES "asked for the learnt landing")
            message(FATAL_ERROR "place ${_place}: the client never asked:\n${_heard}")
        endif()
    endif()
    string(APPEND _said " ${_place}. ${_a}, ${_turn}, ${_from} to ${_then} miles, ${_spare} m "
                        "to spare;")
endforeach()
message(STATUS "run ${RUN} of ${ORDERS} (${_gated_n} aeroplanes with a gate, each following "
               "in some run; places 2 to 4):${_said}")

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
    if(_last GREATER_EQUAL 1)
        foreach(_i RANGE 1 ${_last})
            list(GET _order ${_i} _a)
            list(APPEND _followers "${_a}")
            math(EXPR _place "${_i} + 1")
            list(APPEND _places "${_place}")
        endforeach()
    endif()
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
# it must keep its place ahead of the third. `+ai`: an AI aircraft is put
# on the final first (`--ai-on-final`), and every player waits for it as she
# would for a player. `+circle`: the first player's copilot route orbits
# where the next would start (data/copilot/route-orbit-over-the-16r-start.txt),
# and the next, held past the bound, starts on final to another runway end
# at Sydney whose wind her gate admits: in a northerly 34L, its own other
# end; `+circlesouth`, a wind from 250 at 10 kt: 25, the crossing runway,
# most into it (16L not clear of the orbit, 34L and 34R across it);
# `+circlenone`, at Richmond (YSRI), one runway, 10, in a wind from 100 at
# 8 kt, its other end 28 behind her: none will do, and she waits, told why,
# until the one orbiting goes - the server
# never moves a person's aircraft (REQUIREMENTS 6.5; the owner's ruling,
# 2026-11-11).
set(_how "")
if(_run MATCHES "\\+([a-z]+)$")
    set(_how "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "\\+[a-z]+$" "" _run "${_run}")
endif()
string(REPLACE "," ";" _order "${_run}")
list(LENGTH _order _n)
# **Where**: Sydney's 16R, but for `+circlenone`, Richmond's 10 - a field of
# one runway, whose other end, 28, is behind her in a wind from 100.
set(_field YSSY)
set(_own 16R)
set(_orbit "${ORBIT}")
if(_how STREQUAL "circlenone")
    set(_field YSRI)
    set(_own 10)
    set(_orbit "${ORBIT_YSRI}")
endif()
set(_spec "${_field}/${_own}")

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
            --players-on-final ${_spec}
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
    if(_i EQUAL 0 AND _how STREQUAL "planai")
        # **Once the AI is on its approach**: the session's clock at 90 s,
        # when the weather's change of nothing is written (`--changed-file`),
        # the AI 4.3 miles out, inside three of her start.
        set(_after "${WORK}/changed")
    elseif(_i EQUAL 0)
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
    set(_asks --learnt-landing-at 30)
    if(_i EQUAL 0 AND _how MATCHES "^circle")
        # Orbiting over the start, until the next has had her turn and gone.
        set(_asks --send-route "${_orbit}")
        set(_leaves --copilot-stay 420)
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
         --after-ready "${_after}" --aircraft ${_a} --predict --from-held ${_asks}
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
    execute_process(
        ${_commands}
        COMMAND "${SERVER}" --port ${PORT} --seconds 1800 --until-empty --ai 1 --headless
                --data "${DATA}" --store "${_store}" --timeout 30 --ready-file "${_ready}"
                --players-on-final ${_spec}
        COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
                --seed 1 --until-input-ends --seconds 1800
        COMMAND "${WINDOW}" --headless --gpu-driver "${DRIVER}" --size 320x200
                --aircraft ${_a} --shot "${WORK}/window.bmp" --shot-at 999999
                --quit-at 4800 --view cockpit --press-after 30 L --slow-start 5
                --shot-waiting "${WORK}/waiting.bmp"
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
    set(_ai_final "")
    if(_how STREQUAL "ai")
        set(_ai_final --ai-on-final YSSY/16R)
    endif()
    # **The wind at the other ends**: a northerly of 8 kt is into 34L, which
    # her gate admits; from 250 at 10 kt, nearly straight down 25; at
    # Richmond from 100 at 8 kt, behind 28.
    if(_how STREQUAL "circle")
        set(_ai_final --metar "YSSY 111200Z 36008KT 9999 FEW030 20/10 Q1015"
            --station -33.9461,151.1772)
    elseif(_how STREQUAL "circlesouth")
        set(_ai_final --metar "YSSY 111200Z 25010KT 9999 FEW030 20/10 Q1015"
            --station -33.9461,151.1772)
    elseif(_how STREQUAL "circlenone")
        set(_ai_final --metar "YSRI 111200Z 10008KT 9999 FEW030 20/10 Q1015"
            --station -33.6006,150.7808)
    elseif(_how STREQUAL "planai")
        # The AI flies a plan that ends in a landing on 16R, begun 6.2 miles
        # out on its centreline (data/plans/landing-on-16r-from-6nm.plan).
        file(REMOVE "${WORK}/changed")
        set(_ai_final --plan "${PLANAI}"
            --metar "YSSY 111200Z 00000KT 9999 FEW030 20/10 Q1015"
            --metar-then 90 "YSSY 111201Z 00000KT 9999 FEW030 20/10 Q1015"
            --weather-blend 1 --changed-file "${WORK}/changed" --station -33.9461,151.1772)
    endif()
    execute_process(
        ${_commands}
        COMMAND "${SERVER}" --port ${PORT} --seconds 1800 --until-empty --ai 1 --headless
                --data "${DATA}" --store "${_store}" --timeout 30 --ready-file "${_ready}"
                --players-on-final ${_spec} ${_ai_final}
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
    if(NOT _window MATCHES "the server has given it no aircraft yet: waiting for one\n(.*\n)?glideslope: the server says: waiting for the aircraft ahead: aircraft [0-9]+, [0-9.]+ miles from where she starts, until it is 3 miles on or landed, at most [0-9]+ s more\n(.*\n)?glideslope: the server gave this client aircraft [0-9]+, the ${_a}\n")
        message(FATAL_ERROR "the window client did not wait, told why, and then fly:\n${_window}")
    endif()
    # **And its window showed why while she waited**: the frame written.
    if(NOT _window MATCHES "the window, waiting, shows WAITING FOR THE AIRCRAFT AHEAD / WAITING FOR THE[^\n]*AIRCRAFT [0-9]"
       OR NOT EXISTS "${WORK}/waiting.bmp")
        message(FATAL_ERROR "the window did not show why it waited:\n${_window}")
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
set(_waiting ${_last})
if(_how STREQUAL "ai" OR _how STREQUAL "planai")
    set(_waiting ${_n})
endif()
if(NOT _waited EQUAL _waiting)
    message(FATAL_ERROR "${_waited} players were told to wait their turn, and ${_last} join "
                        "behind another:\n${_out}")
endif()
set(_said "")
set(_handed_before -1)
set(_touches_checked 0)
foreach(_i RANGE ${_last})
    list(GET _order ${_i} _a)
    math(EXPR _place "${_i} + 1")
    string(FIND "${_out}" " asked for ${_a}, and flies it\n" _given)
    if(_given EQUAL -1)
        message(FATAL_ERROR "place ${_place}: the ${_a} was never given:\n${_out}")
    endif()
    string(SUBSTRING "${_out}" ${_given} -1 _since)
    set(_end ${_own})
    if(_i GREATER 0 AND _how STREQUAL "circle")
        set(_end 34L)
    elseif(_i GREATER 0 AND _how STREQUAL "circlesouth")
        set(_end 25)
    endif()
    set(_leader_before "${_leader_number}")
    set(_leader_from_before "${_leader_from}")
    unset(_leader_number)
    if(NOT _since MATCHES "^ asked for ${_a}, and flies it\n([^\n]*\n)*slot [0-9]+'s ${_a} starts on final to ${_field} ${_end} (2\\.[23][0-9]) miles out\n")
        message(FATAL_ERROR "place ${_place}, the ${_a}: not started where her gate holds her, on final to ${_end}:\n${_since}")
    endif()
    set(_from "${CMAKE_MATCH_2}")
    if(_i EQUAL 0 AND _how MATCHES "^circle")
        # The one orbiting over the start: no learnt landing asked of her;
        # told to leave the final once she has held the next past the bound.
        file(READ "${WORK}/heard0.txt" _heard)
        if(NOT _heard MATCHES "told: leave the final to ${_field} ${_own}: go around and clear it")
            message(FATAL_ERROR "the one orbiting over the start was not told to leave the "
                                "final:\n${_heard}")
        endif()
        string(APPEND _said " 1. ${_a}, orbiting over the start, told to leave the final;")
        continue()
    endif()
    if(_i GREATER 0 OR _how STREQUAL "ai" OR _how STREQUAL "planai")
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
        if(_how STREQUAL "circlesouth")
            # **Held past the bound: to 25**, the end most into the wind.
            if(NOT _turn_line MATCHES "^'s turn on final: aircraft [0-9]+, ([0-9.]+) s on the session's clock: aircraft [0-9]+ held the final to YSSY 16R past the bound; she starts on final to YSSY 25 \\(")
                message(FATAL_ERROR "place ${_place}, the ${_a}: not sent to 25 past the bound:\n${_turn_line}")
            endif()
            string(APPEND _turn " (to 25, most into the wind, held past the bound)")
        elseif(_how STREQUAL "circlenone")
            # **None will do: she waits, told why**, until the one orbiting
            # goes on its own; the server does not move it.
            if(NOT _out MATCHES "held past the bound, and no other runway will take her: YSRI 28 [0-9]+ kt of tailwind; the gate is within 5\n")
                message(FATAL_ERROR "place ${_place}, the ${_a}: no other runway refused as it should be:\n${_out}")
            endif()
            file(READ "${WORK}/heard${_i}.txt" _heard)
            if(NOT _heard MATCHES "aircraft 255 told: waiting for aircraft [0-9]+ ahead, past its time over where she starts: no other runway here will take her\n")
                message(FATAL_ERROR "place ${_place}, the ${_a}: not told why she waits on:\n${_heard}")
            endif()
            if(NOT _turn_line MATCHES "^'s turn on final: aircraft [0-9]+, ([0-9.]+) s on the session's clock: nobody ahead\n")
                message(FATAL_ERROR "place ${_place}, the ${_a}: not held until the one orbiting had gone:\n${_turn_line}")
            endif()
            if(_out MATCHES "moved off the final")
                message(FATAL_ERROR "the server moved a person's aircraft:\n${_out}")
            endif()
            string(APPEND _turn " (no other runway in this wind; told why, held until the one orbiting went)")
        elseif(_how STREQUAL "circle")
            # **Held past the bound, to the other end**: the one orbiting
            # held the final past it, and the other end's start was clear.
            if(NOT _turn_line MATCHES "^'s turn on final: aircraft [0-9]+, ([0-9.]+) s on the session's clock: aircraft [0-9]+ held the final to YSSY 16R past the bound; she starts on final to YSSY 34L \\(")
                message(FATAL_ERROR "place ${_place}, the ${_a}: not sent to 34L past the bound:\n${_turn_line}")
            endif()
            string(APPEND _turn " (to 34L, held past the bound)")
        else()
            # (Or waited out, past the bound, 1.5 miles or more on: the AI
            # whose plan lands there flies her final slower than the bound
            # allows for.)
            if(NOT _turn_line MATCHES "^'s turn on final: aircraft [0-9]+, [0-9.]+ s on the session's clock: aircraft [0-9]+ (landed|3 miles on|waited out), ([0-9.]+) miles from her start\n")
                message(FATAL_ERROR "place ${_place}, the ${_a}: let by other than at touchdown, "
                                    "three miles or the bound:\n${_turn_line}")
            endif()
            set(_let "${CMAKE_MATCH_1} ${CMAKE_MATCH_2}")
            if(CMAKE_MATCH_1 STREQUAL "3 miles on" AND CMAKE_MATCH_2 LESS 3.0)
                message(FATAL_ERROR "place ${_place}, the ${_a}: let by at ${CMAKE_MATCH_2} miles")
            endif()
            if(CMAKE_MATCH_1 STREQUAL "waited out" AND CMAKE_MATCH_2 LESS 1.5)
                message(FATAL_ERROR "place ${_place}, the ${_a}: let by at the bound ${CMAKE_MATCH_2} miles on")
            endif()
            if(CMAKE_MATCH_1 STREQUAL "waited out" AND NOT _how STREQUAL "planai")
                message(FATAL_ERROR "place ${_place}, the ${_a}: let by at the bound behind a player")
            endif()
            string(APPEND _turn " (the one ahead ${_let} miles from her start)")
        endif()
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
    if(NOT _since MATCHES "aircraft ${_number} handed to the learnt landing, on final to ${_field} ${_end}, ([0-9.]+) miles out, (-?[0-9]+) m before the gate's near edge")
        message(FATAL_ERROR "place ${_place}, the ${_a}: never handed over:\n${_since}")
    endif()
    set(_then "${CMAKE_MATCH_1}")
    set(_spare "${CMAKE_MATCH_2}")
    if(_spare LESS 100)
        message(FATAL_ERROR "place ${_place}, the ${_a}: only ${_spare} m before the gate's near edge")
    endif()
    string(FIND "${_since}" "aircraft ${_number} handed to the learnt landing" _handed_at)
    math(EXPR _handed_before "${_given} + ${_handed_at}")
    # **The runway cleared before she came down**: where her wheels touched
    # in the run ("aircraft N, ..., touched YSSY 16R", said as they touch),
    # the one ahead had left the runway before it ("has left YSSY 16R,
    # stopped beside it") - counted, and the count must not be nought where
    # one lands behind another.
    if(NOT _leader_before STREQUAL "")
        string(REGEX MATCH "aircraft ${_number}, [^\n]*, touched ${_field} ${_end}\n" _touch "${_since}")
        string(SUBSTRING "${_out}" ${_leader_from_before} -1 _leader_since)
        string(REGEX MATCH "aircraft ${_leader_before}, [^\n]*, has left ${_field} ${_end}, stopped beside it" _left "${_leader_since}")
        if(NOT _touch STREQUAL "")
            string(FIND "${_since}" "${_touch}" _touch_at)
            math(EXPR _touch_abs "${_given} + ${_touch_at}")
            set(_left_abs -1)
            if(NOT _left STREQUAL "")
                string(FIND "${_leader_since}" "${_left}" _left_at)
                math(EXPR _left_abs "${_leader_from_before} + ${_left_at}")
            endif()
            if(_left_abs EQUAL -1 OR _touch_abs LESS _left_abs)
                message(FATAL_ERROR "place ${_place}, the ${_a}: came down before the one ahead had "
                                    "left the runway:\n${_out}")
            endif()
            math(EXPR _touches_checked "${_touches_checked} + 1")
            string(APPEND _turn ", down after the one ahead left the runway")
        elseif(_left STREQUAL "")
            message(FATAL_ERROR "place ${_place}, the ${_a}: the one ahead never left the runway:\n${_leader_since}")
        endif()
    endif()
    set(_leader_number "${_number}")
    math(EXPR _leader_from "${_given} + ${_handed_at}")
    if(NOT (_i EQUAL _last AND _how STREQUAL "window"))
        file(READ "${WORK}/heard${_i}.txt" _heard)
        if(NOT _heard MATCHES "asked for the learnt landing")
            message(FATAL_ERROR "place ${_place}: the client never asked:\n${_heard}")
        endif()
    endif()
    string(APPEND _said " ${_place}. ${_a}, ${_turn}, ${_from} to ${_then} miles, ${_spare} m "
                        "to spare;")
endforeach()
# **The touch checked where one lands behind another**: the four-player
# run and the one begun again, whose followers stay to land.
if((_how STREQUAL "" OR _how STREQUAL "restart") AND _touches_checked EQUAL 0)
    message(FATAL_ERROR "no touchdown was checked against the one ahead's leaving the runway")
endif()
message(STATUS "run ${RUN} of ${ORDERS} (${_gated_n} aeroplanes with a gate, each following "
               "in some run; places 2 to 4):${_said}; ${_touches_checked} touchdowns after the "
               "one ahead left the runway")

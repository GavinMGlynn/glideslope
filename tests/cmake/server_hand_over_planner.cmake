# server_hand_over_planner.cmake - an aircraft the AI is given in the air,
# by a player who leaves or by a take-over, planned by the server's model.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         (-DCASE=leave|take_over -DCHOSEN=anthropic|openai -DMODEL=<model>
#          (-DPLAYBACK=<recording> | -DRECORD=<recording>)
#          | -DCASE=refused)
#         -P server_hand_over_planner.cmake
#
# **Built, not hoped for.** A server with one AI aircraft and a planner for
# hand-overs (`--hand-over-planner CHOSEN:MODEL`), and one headless client
# flying its own aircraft:
#
# - **CASE leave**: the server hands a leaving player's aircraft to the AI
#   (`--on-leave ai`); the client flies five seconds and leaves.
# - **CASE take_over**: the client takes over the server's AI aircraft five
#   seconds in (`--take-over-at 5`), leaving its own to the AI, and leaves
#   ten seconds in.
#
# Either way the server must say the aircraft so given is planned by the
# model chosen, asked from where it is - played back from PLAYBACK with no
# key, or asked now with the server's key and kept in RECORD - and that it
# flies the model's route; and the last of the half-minute lines on its route
# must be past its first waypoint, or nearer it than when the route was
# taken - as the server says it then. **The
# server waits on that**: with a hand-over planner, `--until-empty` stops only
# once its clients have gone and every aircraft so given has its answer and
# has been said to fly its route five times. 600 s is only the most it runs.
#
# **CASE refused**: both cases, one after the other, each with the model the
# other's recording is not - leave with anthropic, take-over with openai -
# and no key anywhere the server could look (both key variables unset, HOME,
# XDG_CONFIG_HOME and APPDATA an empty directory). The server must say each
# is refused, naming the missing key, and that the aircraft goes on as with
# no planner - left by its player, it flies the plan file; left by a
# take-over, it holds its course - and fly no model's route. (What each of
# those is is tested by server_leave.cmake and server_take_over.cmake.)
#
# Asked of a model now costs money, so only with GLIDESLOPE_LIVE_MODEL=1;
# else skipped (exit 77), before anything else. Without the DEM's tiles,
# skipped.

cmake_minimum_required(VERSION 3.28)

if(DEFINED RECORD AND NOT "$ENV{GLIDESLOPE_LIVE_MODEL}" STREQUAL "1")
    message(STATUS "${CHOSEN} is not asked: a live model call costs money, and is made only "
                   "with GLIDESLOPE_LIVE_MODEL=1 set")
    cmake_language(EXIT 77)
endif()

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/hand_over_planner.sqlite")
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

# One session: CASE_ given to the AI, planned by WHO; the server's output in
# _served.
function(given case who)
    set(_planner --hand-over-planner "${who}")
    if(DEFINED MODEL)
        set(_planner --hand-over-planner "${who}:${MODEL}")
    endif()
    if(DEFINED PLAYBACK)
        list(APPEND _planner --hand-over-playback "${PLAYBACK}")
    elseif(DEFINED RECORD)
        file(REMOVE "${RECORD}")
        list(APPEND _planner --hand-over-record "${RECORD}")
    endif()
    if(case STREQUAL "leave")
        set(_client 6 --after 1 --fly)
        set(_server --on-leave ai)
    else()
        set(_client 10 --after 1 --fly --take-over-at 5)
        set(_server)
    endif()
    # The client first and the server last, so that what the server prints
    # is what comes out of the pipeline.
    execute_process(
        COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" ${_client}
        COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --ai 1 --headless
                --data "${DATA}" --timeout 5 --store "${_store}" ${_server} ${_planner}
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _served ERROR_VARIABLE _err)
    if(NOT _rcs STREQUAL "0;0")
        if(DEFINED RECORD AND _err MATCHES "no (OpenAI|Anthropic) key|no credits|credit balance|insufficient_quota")
            message(STATUS "${who} cannot be asked here: ${_err}")
            cmake_language(EXIT 77)
        endif()
        message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_served}\n${_err}")
    endif()
    if(NOT _served MATCHES "everybody who joined has gone")
        message(FATAL_ERROR "${case}: the server did not stop on its events, but at its "
                            "most:\n${_served}")
    endif()
    if(case STREQUAL "leave")
        set(_because "left by its player")
    else()
        set(_because "left by a take-over")
    endif()
    set(_because "${_because}" PARENT_SCOPE)
    set(_served "${_served}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "refused")
    unset(ENV{GLIDESLOPE_OPENAI_KEY})
    unset(ENV{GLIDESLOPE_ANTHROPIC_KEY})
    file(REMOVE_RECURSE "${WORK}/no-config")
    file(MAKE_DIRECTORY "${WORK}/no-config")
    set(ENV{XDG_CONFIG_HOME} "${WORK}/no-config")
    set(ENV{HOME} "${WORK}/no-config")
    set(ENV{APPDATA} "${WORK}/no-config")
    # **Both cases, and both models across them**: the walk says how many it
    # covered, and fails unless it is all of them.
    set(_walks "leave|anthropic|no Anthropic key|flies the plan file"
               "take_over|openai|no OpenAI key|holds its course")
    set(_walked 0)
    foreach(_walk IN LISTS _walks)
        string(REPLACE "|" ";" _walk "${_walk}")
        list(GET _walk 0 _case)
        list(GET _walk 1 _who)
        list(GET _walk 2 _missing)
        list(GET _walk 3 _before)
        given(${_case} ${_who})
        if(NOT _served MATCHES "aircraft [0-9]+, ${_because}: ${_who} is refused: ${_missing}[^\n]*; it ${_before} instead")
            message(FATAL_ERROR "${_case}: ${_who} with no key was not refused, saying so and "
                                "that it ${_before}:\n${_served}")
        endif()
        if(_served MATCHES "flies its model's route")
            message(FATAL_ERROR "${_case}: a model's route was flown with no key:\n${_served}")
        endif()
        math(EXPR _walked "${_walked} + 1")
    endforeach()
    list(LENGTH _walks _all)
    if(NOT _walked EQUAL _all)
        message(FATAL_ERROR "walked ${_walked} of ${_all}")
    endif()
    message(STATUS "each of the ${_all} hand-overs, its model with no key, was refused and "
                   "went on as before")
    return()
endif()

given(${CASE} ${CHOSEN})
if(NOT _served MATCHES "aircraft ([0-9]+), ${_because}, is planned by ([a-z]+), ([^,\n]+)")
    message(FATAL_ERROR "the server never said a model planned the aircraft ${_because}:\n"
                        "${_served}")
endif()
set(_aircraft "${CMAKE_MATCH_1}")
if(NOT CMAKE_MATCH_2 STREQUAL CHOSEN OR NOT CMAKE_MATCH_3 STREQUAL MODEL)
    message(FATAL_ERROR "planned by ${CMAKE_MATCH_2}, ${CMAKE_MATCH_3}, not the ${CHOSEN} "
                        "${MODEL} chosen")
endif()
if(_served MATCHES "aircraft ${_aircraft}: its model's route refused: ([^\n]*)")
    message(FATAL_ERROR "the server refused its model's route: ${CMAKE_MATCH_1}\n${_served}")
endif()
if(NOT _served MATCHES "aircraft ${_aircraft}, ${_because}, flies its model's route of [0-9]+:([^\n]*), [0-9]+ s in, ([0-9]+) m from ([A-Za-z0-9_]+)")
    message(FATAL_ERROR "the aircraft ${_because} never flew its model's route:\n${_served}")
endif()
set(_names "${CMAKE_MATCH_1}")
set(_taken_m "${CMAKE_MATCH_2}")
set(_taken_to "${CMAKE_MATCH_3}")
string(REGEX MATCHALL "aircraft ${_aircraft} on its copilot's route: to [A-Za-z0-9_]+, [0-9]+ of [0-9]+, [0-9]+ m"
       _progress "${_served}")
list(LENGTH _progress _n)
if(_n LESS 2)
    message(FATAL_ERROR "the server said where the route had got to ${_n} times, not twice:\n"
                        "${_served}")
endif()
# **Measured from where it was when the route was taken**, not from the
# first half-minute line: a route of one orbit may have reached its circle by
# then, and round it the distance from its centre holds, rather than falls.
# The last line must be past the first waypoint or nearer it than then.
list(GET _progress -1 _last)
string(REGEX MATCH "to ([A-Za-z0-9_]+), ([0-9]+) of [0-9]+, ([0-9]+) m" _m "${_last}")
set(_last_to "${CMAKE_MATCH_1}")
set(_last_leg "${CMAKE_MATCH_2}")
set(_last_m "${CMAKE_MATCH_3}")
# And to one of the route's own waypoints: an aircraft that took the route
# and flew on with the plan file it had would be said to be on its way to a
# waypoint of that.
string(REPLACE " " ";" _route_names "${_names}")
if(NOT _last_to IN_LIST _route_names)
    message(FATAL_ERROR "the aircraft is flying to ${_last_to}, not a waypoint of its model's "
                        "route,${_names}: ${_last}\n${_served}")
endif()
if(_last_leg EQUAL 1 AND _last_to STREQUAL _taken_to AND NOT _last_m LESS _taken_m)
    message(FATAL_ERROR "the aircraft came no nearer its first waypoint: ${_taken_m} m from "
                        "${_taken_to} when its route was taken, then ${_last}\n${_served}")
endif()
message(STATUS "${_because}, the server's ${CHOSEN} planned it and the server flew it,${_names}: "
               "${_taken_m} m from ${_taken_to} when taken; then ${_last}")

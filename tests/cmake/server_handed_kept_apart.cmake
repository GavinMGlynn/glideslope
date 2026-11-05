# server_handed_kept_apart.cmake - an aircraft its player hands to the AI is
# measured against every other AI aircraft, as theirs are against each other.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         [-DCOPILOT=<recording>]
#         -P server_handed_kept_apart.cmake
#
# **Built, not hoped for.** A server with two AI aircraft flying the plan
# file, and one headless client flying its own aircraft, which it asks the
# server to hand to the AI two seconds in (`--hand-over-at 2`, what A does)
# and never takes back. Handed over, it stays in its player's slot - the one
# way of giving the AI an aircraft that keeps it there (a player who leaves,
# or takes over another, gives it a number of the AI's, measured already).
# **The run is counted in steps the AI has the aircraft**: the server stops
# once it has flown it for HANDED steps (`--steps-after-hand-over`) -
# whenever on the machine's clock that comes -
# and writes its `--stopped-file`, by which the client leaves
# (`--until-exists`). A run of fixed length on the clock measured fewer
# steps of the hand-over on a slow runner, which hands over later in it.
# What the server says is what is read (the client's account goes to its
# `--heard` file). The
# client's aircraft is put a thousand feet over the two AI aircraft's
# layers, so the one handed over is near both from the start.
#
# **What is checked**: the server's account of every two AI aircraft at the
# end of the run. **Coverage is asserted**: three aircraft the AI flew make
# three pairs - the two plan-file aircraft, and each of them with the handed
# one - and each pair with the handed one is measured over exactly the
# HANDED steps asked. Before, the handed one was never measured: one pair.
# Separation lost for no step of any pair.
#
# **With COPILOT, the aircraft is handed over by its copilot's route**: the
# client asks its copilot five seconds in to fly to Manly at 3,000 ft and
# orbit over Manly beach, played back from the recording COPILOT (no key:
# CI flies without one), and the route it answers is sent to the server,
# which hands the aircraft to the AI to fly it - a handed aircraft flying a
# copilot's route, measured against the two plan-file aircraft over the
# HANDED steps it flies it, as the hand-over's is. **Coverage is asserted
# by kind**: three pairs - the two on the plan file (1), and the copilot's
# with each (2), every pair with the copilot's over exactly HANDED steps.
# **Left out, by name**: the copilot's with a model-planned aircraft. Those
# take off from 16R 90 s apart, so a run with both up and the copilot's
# flying is four minutes of a client's real time (266 s, tried), in which
# the copilot's came within 1.5 nm of none of them; and two aircraft are
# measured the same whatever their routes - planned pairs are measured in
# server_kept_apart.cmake's run. The server must say it flies the copilot's
# route.
#
# Without the DEM's tiles it reports itself skipped (exit 77).

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake): another
# build's run of this test on the same port waits for it.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/handed-kept-apart.sqlite")
set(_heard "${WORK}/heard.txt")
set(_stopped "${WORK}/stopped.txt")
file(REMOVE "${_store}" "${_heard}" "${_stopped}")

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

# Thirty simulated seconds of the hand-over; the client stays at most ten
# minutes waiting for them.
set(_handed 3600)
set(_ai --ai 2)
set(_pairs_wanted 3)
set(_with_handed_wanted 2)
if(DEFINED COPILOT)
    set(_handing --predict --copilot-at 5 --copilot c172p
                 "fly to Manly at 3,000 ft, then orbit over Manly beach"
                 --copilot-provider anthropic --copilot-model claude-haiku-4-5-20251001
                 --copilot-stay 600 --copilot-playback "${COPILOT}")
else()
    set(_handing --hand-over-at 2)
endif()
execute_process(
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 600 --after 1
            ${_handing} --heard "${_heard}" --until-exists "${_stopped}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 900 ${_ai} --headless
            --data "${DATA}" --store "${_store}" --steps-after-hand-over ${_handed}
            --stopped-file "${_stopped}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")
if(NOT EXISTS "${_heard}")
    message(FATAL_ERROR "the client heard nothing:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _said MATCHES "aircraft [0-9]+ handed to the AI")
    message(FATAL_ERROR "the client never heard its aircraft handed to the AI:\n${_said}")
endif()
if(_said MATCHES "handed to its pilot")
    message(FATAL_ERROR "the aircraft was given back, and is to stay the AI's:\n${_said}")
endif()
if(DEFINED COPILOT)
    if(NOT _said MATCHES "sent its copilot's route of ([0-9]+)")
        message(FATAL_ERROR "the client sent no copilot's route:\n${_said}\n${_err}")
    endif()
    if(NOT _out MATCHES "flies its copilot's route")
        message(FATAL_ERROR "the server did not fly the copilot's route:\n${_out}")
    endif()
endif()
if(_out MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was wrecked:\n${_out}")
endif()

if(NOT _out MATCHES "kept apart: ([0-9]+) pairs of AI aircraft over ([0-9]+) steps, separation lost for ([0-9]+) steps")
    message(FATAL_ERROR "the server did not say how its AI aircraft were kept apart:\n${_out}")
endif()
set(_pairs "${CMAKE_MATCH_1}")
set(_lost "${CMAKE_MATCH_3}")
if(NOT _pairs EQUAL _pairs_wanted)
    message(FATAL_ERROR "${_pairs} pairs were measured, not the ${_pairs_wanted} that the AI "
                        "aircraft and the one handed to the AI make:\n${_out}")
endif()
string(REGEX MATCHALL "\napart: [^\n]*" _lines "${_out}")
list(TRANSFORM _lines STRIP)
if(NOT _out MATCHES "stopped after ${_handed} steps of a player's aircraft with the AI")
    message(FATAL_ERROR "the server did not stop ${_handed} steps after the hand-over:\n${_out}")
endif()
set(_with_handed 0)
# Each pair's kind, by the routes its two fly: "planned" (a model's plan),
# "file" (the plan file) or "copilot" (the handed one's copilot's route).
set(_kinds)
foreach(_line IN LISTS _lines)
    message(STATUS "${_line}")
    if(NOT _line MATCHES "^apart: (.*) and (.*), over ")
        message(FATAL_ERROR "a pair's line names no two aircraft: ${_line}")
    endif()
    set(_two)
    foreach(_who "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
        if(_who MATCHES "\\(slot [0-9]+\\)")
            list(APPEND _two copilot)
        elseif(_who MATCHES "'s plan\\)")
            list(APPEND _two planned)
        else()
            list(APPEND _two file)
        endif()
    endforeach()
    list(SORT _two)
    list(JOIN _two "+" _kind)
    list(APPEND _kinds "${_kind}")
    if(_line MATCHES "\\(slot [0-9]+\\)")
        math(EXPR _with_handed "${_with_handed} + 1")
        if(NOT _line MATCHES ", over ([0-9]+) steps, " OR NOT CMAKE_MATCH_1 EQUAL _handed)
            message(FATAL_ERROR "the handed aircraft was measured over other than ${_handed} "
                                "steps: ${_line}")
        endif()
    endif()
    if(NOT _line MATCHES "separation lost for 0\\.00 s$")
        message(FATAL_ERROR "separation was lost: ${_line}\n${_out}")
    endif()
endforeach()
if(NOT _with_handed EQUAL _with_handed_wanted)
    message(FATAL_ERROR "the handed aircraft is in ${_with_handed} pairs, not "
                        "${_with_handed_wanted}:\n${_out}")
endif()
if(DEFINED COPILOT)
    # Every kind of pair, as many of each as three aircraft make.
    foreach(_want "file+file=1" "copilot+file=2")
        string(REPLACE "=" ";" _want "${_want}")
        list(GET _want 0 _kind)
        list(GET _want 1 _count)
        set(_seen 0)
        foreach(_k IN LISTS _kinds)
            if(_k STREQUAL _kind)
                math(EXPR _seen "${_seen} + 1")
            endif()
        endforeach()
        if(NOT _seen EQUAL _count)
            message(FATAL_ERROR "${_seen} pairs of kind ${_kind} were measured, not ${_count}: "
                                "${_kinds}")
        endif()
    endforeach()
    message(STATUS "three pairs of two kinds measured: ${_kinds}")
endif()
if(NOT _lost EQUAL 0)
    message(FATAL_ERROR "separation lost for ${_lost} steps")
endif()
message(STATUS "the aircraft handed to the AI measured against every AI aircraft, "
               "over ${_handed} steps each, as asked; separation never lost")

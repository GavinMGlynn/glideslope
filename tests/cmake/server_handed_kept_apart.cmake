# server_handed_kept_apart.cmake - an aircraft its player hands to the AI is
# measured against every other AI aircraft, as theirs are against each other.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
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
# Without the DEM's tiles it reports itself skipped (exit 77).

cmake_minimum_required(VERSION 3.28)

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
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 600 --after 1
            --hand-over-at 2 --heard "${_heard}" --until-exists "${_stopped}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 900 --ai 2 --headless
            --data "${DATA}" --store "${_store}" --steps-after-hand-over ${_handed}
            --stopped-file "${_stopped}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
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
if(_out MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was wrecked:\n${_out}")
endif()

if(NOT _out MATCHES "kept apart: ([0-9]+) pairs of AI aircraft over ([0-9]+) steps, separation lost for ([0-9]+) steps")
    message(FATAL_ERROR "the server did not say how its AI aircraft were kept apart:\n${_out}")
endif()
set(_pairs "${CMAKE_MATCH_1}")
set(_lost "${CMAKE_MATCH_3}")
if(NOT _pairs EQUAL 3)
    message(FATAL_ERROR "${_pairs} pairs were measured, not the three that two AI aircraft "
                        "and one handed to the AI make:\n${_out}")
endif()
string(REGEX MATCHALL "\napart: [^\n]*" _lines "${_out}")
list(TRANSFORM _lines STRIP)
if(NOT _out MATCHES "stopped after ${_handed} steps of a player's aircraft with the AI")
    message(FATAL_ERROR "the server did not stop ${_handed} steps after the hand-over:\n${_out}")
endif()
set(_with_handed 0)
foreach(_line IN LISTS _lines)
    message(STATUS "${_line}")
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
if(NOT _with_handed EQUAL 2)
    message(FATAL_ERROR "the handed aircraft is in ${_with_handed} pairs, not two:\n${_out}")
endif()
if(NOT _lost EQUAL 0)
    message(FATAL_ERROR "separation lost for ${_lost} steps")
endif()
message(STATUS "the aircraft handed to the AI measured against both AI aircraft, "
               "over ${_handed} steps each, as asked; separation never lost")

# server_swap_wreck.cmake - a player's aircraft handed to the AI, taken back,
# then wrecked, flies again as the player's.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_swap_wreck.cmake
#
# **Built, not hoped for.** The client flies its own aircraft, asks for it to
# be handed to the AI two seconds in and back at four, and at six puts it into
# the sea: full forward stick and full power (`--dive-after`). The server must
# say it handed it both ways and that it is a wreck, and five seconds later
# that it flies again - and at the end the server must have applied every
# input the client sent. A server that took the aircraft flying again for the
# AI's - it kept a controller from having been handed over and back - gave it
# to the AI with nobody told, and applied none of the client's inputs after.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/swap-wreck.sqlite")
set(_heard "${WORK}/heard.txt")
file(REMOVE "${_store}" "${_heard}")

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

execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 45 --after 1 --fly
            --hand-over-at 2 --take-back-at 4 --dive-after 6 --heard "${_heard}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT EXISTS "${_heard}")
    message(FATAL_ERROR "the client heard nothing:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
foreach(_what IN ITEMS "handed to the AI" "handed to its pilot" "is a wreck" "flies again")
    if(NOT _said MATCHES "aircraft [0-9]+ ${_what}")
        message(FATAL_ERROR "the client never heard its aircraft ${_what}:\n${_said}")
    endif()
endforeach()
if(NOT _out MATCHES "sent ([0-9]+) input frames, the server applied ([0-9]+)")
    message(FATAL_ERROR "the client did not say what it sent:\n${_out}")
endif()
if(NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2)
    message(FATAL_ERROR "the server applied ${CMAKE_MATCH_2} of ${CMAKE_MATCH_1} inputs: after "
                        "flying again it was not flying the player's\n${_said}")
endif()
message(STATUS "handed over and back, wrecked, flown again, and all ${CMAKE_MATCH_1} "
               "inputs applied")

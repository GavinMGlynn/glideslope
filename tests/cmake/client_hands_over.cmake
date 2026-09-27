# client_hands_over.cmake - the client with the window hands its own aircraft
# to the AI on a server, and takes it back.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -P client_hands_over.cmake
#
# **Built, not hoped for.** A server with one AI Cessna, and the client with
# the window joining it and flying its own aircraft, which it asks the server
# to hand to the AI four seconds in (`--hand-over-after 4`, what A does on a
# server) and to give back ten seconds in (`--take-back-after 10`), taking a
# shot sixteen seconds in. It must say, once each and in that order, that the
# server said the AI had its aircraft - its HUD then reading FLYING AI - and
# then that the server said the pilot had it - its HUD then reading FLYING
# PILOT; and at the shot that the server says the pilot has it and has flown
# it by inputs sent since it was taken back: what handing over and taking
# back is. "The server said" is the controller the server's own state updates
# give the aircraft, not what the client asked for.
#
# **What it shows across the switch is not judged here**: the client with the
# window does not yet blend its own aircraft at a switch, which is its own
# tail in docs/COMPLETION_PLAN.md.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/hand.sqlite")
set(_shot "${WORK}/hand.bmp")
file(REMOVE "${_store}" "${_shot}")

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

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}"
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 1920 --view cockpit
            --hand-over-after 4 --take-back-after 10
            --server 127.0.0.1 ${PORT} --server-key ${_key}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT EXISTS "${_shot}")
    if(_err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()

# Once each, and the AI first.
foreach(_who IN ITEMS "the AI" "the pilot")
    string(REGEX MATCHALL "the server says ${_who} has aircraft [0-9]+\n" _said "${_out}")
    list(LENGTH _said _times)
    if(NOT _times EQUAL 1)
        message(FATAL_ERROR "the client said ${_times} times, not once, that the server "
                            "gave its aircraft to ${_who}:\n${_out}")
    endif()
endforeach()
string(FIND "${_out}" "the server says the AI has aircraft" _to_ai)
string(FIND "${_out}" "the server says the pilot has aircraft" _to_pilot)
if(_to_pilot LESS _to_ai)
    message(FATAL_ERROR "the server gave the aircraft back before it handed it over:\n${_out}")
endif()
# What the HUD read at each: FLYING AI between them, FLYING PILOT after.
math(EXPR _between "${_to_pilot} - ${_to_ai}")
string(SUBSTRING "${_out}" ${_to_ai} ${_between} _handed)
string(SUBSTRING "${_out}" ${_to_pilot} -1 _back)
if(NOT _handed MATCHES "the HUD reads FLYING AI" OR _handed MATCHES "the HUD reads FLYING PILOT")
    message(FATAL_ERROR "handed over, the HUD did not say the AI has it:\n${_out}")
endif()
if(NOT _back MATCHES "^the server says the pilot has aircraft [0-9]+\n(glideslope: the HUD reads [^\n]*\n)*glideslope: the HUD reads FLYING PILOT")
    message(FATAL_ERROR "taken back, the HUD did not say the pilot has it:\n${_out}")
endif()
# And at the shot, six seconds later, flown by this client's inputs again.
if(NOT _out MATCHES "flying aircraft ([0-9]+), the c172p; the server says the pilot has it, and has flown it by inputs sent since it was taken back")
    message(FATAL_ERROR "the server was not flying the client's aircraft by its inputs at "
                        "the shot:\n${_out}")
endif()
message(STATUS "handed aircraft ${CMAKE_MATCH_1} to the AI and took it back, "
               "flying it by the pilot's inputs")

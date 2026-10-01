# client_copilot.cmake - the client with the window asks its copilot, on the
# player's machine, and the server flies what it answered.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -DPLAYBACK=<recording> -P client_copilot.cmake
#
# **Built, not hoped for.** A server with one AI Cessna, and the client with
# the window joining it and flying its own aircraft. Three seconds in it asks
# its copilot (`--copilot-after 3`, what C does) to fly to Manly - played back
# from PLAYBACK, so asked of nobody and with no key - and sends the route it
# answers. The client must say its copilot answered with a route; the server
# must then hand its aircraft to the AI, which the client hears from the
# server's own updates (the HUD then reading FLYING AI); and **what the client
# showed of its own aircraft must not step at the switch** from predicting it
# to drawing it from the updates: one switch, under the 5 m the network checks
# hold. **The shot waits on the events**, not the clock: from its tick, 5 s
# in, until the route is sent and the server says the AI has the aircraft,
# up to five minutes of the flight past it. It was drawn at a fixed 25 s at
# first, and on CI (run 36686103327) the copilot had been asked and had not
# yet answered by then.
#
# It needs a GPU driver, and the DEM's tiles; without either it reports
# itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/copilot.sqlite")
set(_shot "${WORK}/copilot.bmp")
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
# The client connects once the server is flying (client.cmake says why).
set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 600 --view cockpit
            --copilot "fly to Manly at 3,000 ft, then orbit over Manly beach"
            --copilot-provider anthropic --copilot-model claude-haiku-4-5-20251001
            --copilot-playback "${PLAYBACK}" --copilot-after 3
            --after-ready "${_ready}" --server 127.0.0.1 ${PORT} --server-key ${_key}
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
if(NOT _out MATCHES "the shot drawn [0-9.]+ s past its tick; its copilot's route sent, and the server says the AI has it")
    message(FATAL_ERROR "the shot was drawn before the copilot's route was flown:\n${_out}")
endif()
if(NOT _out MATCHES "glideslope: its copilot answered with a route of [0-9]+:")
    message(FATAL_ERROR "the client's copilot did not answer with a route:\n${_out}")
endif()
if(NOT _out MATCHES "the server says the AI has aircraft [0-9]+\n")
    message(FATAL_ERROR "the server never said the AI had the client's aircraft:\n${_out}")
endif()
if(NOT _out MATCHES "own aircraft: ([0-9]+) switches; the largest step at a switch ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say how far what it showed stepped:\n${_out}")
endif()
set(_switches "${CMAKE_MATCH_1}")
set(_step "${CMAKE_MATCH_2}")
if(NOT _switches EQUAL 1)
    message(FATAL_ERROR "the client measured ${_switches} switches of its own aircraft, not "
                        "the 1 made - handed to the AI for its copilot's route:\n${_out}")
endif()
if(_step GREATER_EQUAL 5)
    message(FATAL_ERROR "what the client showed stepped ${_step} m at the switch, the bound 5 m:\n${_out}")
endif()
message(STATUS "the copilot's route went to the server, which handed the aircraft to its AI; "
               "what was shown stepped ${_step} m at the switch")

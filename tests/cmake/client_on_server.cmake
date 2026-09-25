# client_on_server.cmake - the client with the window flies the aircraft a
# server gives it, and draws the other aircraft in that server's sky.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -P client_on_server.cmake
#
# **Built, not hoped for.** A server with one AI Cessna, and the client with
# the window joining it and taking a shot ten seconds in (`--shot-at 1200`)
# from behind - asking for an F-15C, which the server's word overrides. The client must say it was given an aircraft - a Cessna, as the
# server's `AIRCRAFT` said - that it drew the AI, a Cessna, under a different
# number, and how its own aircraft's prediction went: put right all through,
# never too far to hide - under the 20 m that is.
#
# **A slow machine joining**, built on purpose: the client stands still five
# seconds after joining (`--slow-start 5`), as one building its flight slowly
# does, while the server flies its aircraft on. What it hears first after
# that is where its aircraft is, not a correction: a Linux debug runner on
# CI, slow to build, was put right too far to hide by the whole way flown
# meanwhile.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/on-server.sqlite")
set(_shot "${WORK}/on-server.bmp")
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

# Leaks are judged below (client.cmake), not by LeakSanitizer's exit code -
# which, left to it, ends the client before what it printed is written out.
set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(
    # Until the client has gone.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}"
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 1200 --view behind --aircraft f15c --slow-start 5
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

if(NOT _out MATCHES "the server gave this client aircraft ([0-9]+), the c172p")
    message(FATAL_ERROR "the client was not given the server's Cessna:\n${_out}")
endif()
set(_mine "${CMAKE_MATCH_1}")
# **The server's word wins**: asked for an F-15C, the client flies the Cessna
# the server gave it. (A client that took its own `--aircraft` flew one model
# and was put right by another's motion.)
if(NOT _out MATCHES "flying the [^\n]* \\(c172p\\)")
    message(FATAL_ERROR "the client did not fly the server's Cessna:\n${_out}")
endif()

string(REGEX MATCHALL "drew aircraft [0-9]+, the [a-z0-9_-]+, [0-9]+ m away" _drew "${_out}")
list(LENGTH _drew _drawn)
if(NOT _drawn EQUAL 1)
    message(FATAL_ERROR "the client drew ${_drawn} other aircraft, and there is one:\n${_out}")
endif()
if(NOT _drew MATCHES "drew aircraft ([0-9]+), the c172p, ([0-9]+) m away")
    message(FATAL_ERROR "what it drew was not the AI's Cessna: ${_drew}")
endif()
if(CMAKE_MATCH_1 EQUAL _mine)
    message(FATAL_ERROR "it drew its own aircraft as another: ${_drew}")
endif()
set(_away "${CMAKE_MATCH_2}")

if(NOT _out MATCHES "predicted: ([0-9]+) corrections, the worst ([0-9.]+) m, ([0-9]+) too large to hide")
    message(FATAL_ERROR "the client did not say how its prediction went:\n${_out}")
endif()
# A correction for each update, and updates come 25 for each second the
# server simulates: 277 in ten seconds here, and 75 from a Windows debug
# server that could not keep real time. Two a second is the floor that says
# the prediction was put right all through; how far, below, is the bound.
if(CMAKE_MATCH_1 LESS 20)
    message(FATAL_ERROR "only ${CMAKE_MATCH_1} corrections in ten seconds:\n${_out}")
endif()
if(NOT CMAKE_MATCH_3 EQUAL 0 OR CMAKE_MATCH_2 GREATER_EQUAL 20)
    message(FATAL_ERROR "its own aircraft was put right too far to hide:\n${_out}")
endif()
message(STATUS "the client flew aircraft ${_mine} on the server, and drew the AI "
               "${_away} m away; the worst correction ${CMAKE_MATCH_2} m")

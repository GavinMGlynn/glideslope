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
# **A slow machine's frames**, built on purpose with `-DSLOW_FRAMES=<ms>`:
# every pass of the frame loop held that much longer, a tenth of a second
# apart (`--slow-frames`). CI's software Vulkan drew a frame in 1.7 s and the
# client was put right by 26.8 m; held 0.7 s here it was put right by 33 m,
# twelve times too far to hide, before its words were heard after its ticks
# and once a frame (PROJECT_STATUS.md, 2026-09-30). The same bound holds.
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
set(_slow)
if(DEFINED SLOW_FRAMES)
    set(_slow --slow-frames ${SLOW_FRAMES})
endif()
# The client connects once the server is flying (client.cmake says why).
set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
execute_process(
    # Until the client has gone.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 1200 --view behind --aircraft f15c --slow-start 5
            ${_slow} --after-ready "${_ready}" --server 127.0.0.1 ${PORT} --server-key ${_key}
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
set(_corrections "${CMAKE_MATCH_1}")
set(_worst "${CMAKE_MATCH_2}")
set(_snapped "${CMAKE_MATCH_3}")
# **Put right all through, whatever the frame rate.** A correction is made
# once a frame that heard a word on its own, from the newest; the client
# counts both, and they must agree - not a count a slow machine's few frames
# miss.
# The words are held to the server's rate: 25 for each second it simulates,
# from the first to the last heard, four in five of them at least; and 20 at
# least in all, the floor that says the prediction was heard all through (75
# came from a Windows debug server that could not keep real time).
if(NOT _out MATCHES "heard ([0-9]+) words on its own aircraft over ([0-9]+)\\.([0-9][0-9]) s of the server's time, in ([0-9]+) frames")
    message(FATAL_ERROR "the client did not say what it heard of its own:\n${_out}")
endif()
set(_words "${CMAKE_MATCH_1}")
math(EXPR _span_cs "${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}")
set(_frames "${CMAKE_MATCH_4}")
if(NOT _corrections EQUAL _frames)
    message(FATAL_ERROR "${_corrections} corrections, but ${_frames} frames heard a word on "
                        "its own - one each:\n${_out}")
endif()
math(EXPR _least "${_span_cs} * 25 * 4 / 500")
if(_words LESS 20 OR _words LESS _least)
    message(FATAL_ERROR "only ${_words} words heard on its own over ${_span_cs} hundredths "
                        "of a second of the server's time, where at least ${_least} and 20 "
                        "were due:\n${_out}")
endif()
# **Held long, as asked**: a flag read as nought held nothing and passed
# untested, so the frames are checked to have been as long as the holds.
if(DEFINED SLOW_FRAMES)
    if(NOT _out MATCHES "own aircraft's longest frame ([0-9]+) ms")
        message(FATAL_ERROR "the client did not say how long its frames were:\n${_out}")
    endif()
    if(CMAKE_MATCH_1 LESS SLOW_FRAMES)
        message(FATAL_ERROR "its longest frame was ${CMAKE_MATCH_1} ms, shorter than the "
                            "${SLOW_FRAMES} ms every hold was:\n${_out}")
    endif()
endif()
if(NOT _snapped EQUAL 0 OR _worst GREATER_EQUAL 20)
    message(FATAL_ERROR "its own aircraft was put right too far to hide:\n${_out}")
endif()
message(STATUS "the client flew aircraft ${_mine} on the server, and drew the AI "
               "${_away} m away; ${_corrections} corrections in ${_frames} frames from "
               "${_words} words, the worst ${_worst} m")

# client_rides_along.cmake - the client with the window rides along in an AI
# aircraft: its seat, its instruments and its controls.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> [-DTAKE_OVER=ON] -P client_rides_along.cmake
#
# **Built, not hoped for.** A server with one AI Cessna, and the client with
# the window joining it with `--ride-along` - which watches the first AI
# aircraft from the start - in the cockpit view, taking a shot ten seconds in.
# It must say it rode along in the AI's Cessna, with the camera at its seat -
# within 5 m of its centre of gravity, where a Cessna's pilot sits a metre or
# two from it - and that the HUD said the AI was flying it and where each of
# its controls was: what riding along is.
#
# **A slow machine is not let go.** The client stands still five seconds
# after joining (`--slow-start 5`), as a slow machine building its flight
# does, against a server that lets a silent client go after three: it must
# stay in the session, and so be told the controls at all. A Windows debug
# build was let go, heard one update, and drew its HUD without them.
#
# With TAKE_OVER, the client takes the AI's Cessna over four seconds in, with
# `--take-over-after 4`, and by the shot must say it took it over and is
# flying it, the server saying the pilot has it and having flown it by inputs
# sent since, and the HUD saying the pilot has it: what taking over is. (What
# the server says of it, which goes down the pipe here, server_take_over.cmake
# checks.)
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/ride.sqlite")
set(_shot "${WORK}/ride.bmp")
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

set(_take_over)
if(TAKE_OVER)
    set(_take_over --take-over-after 4)
endif()
set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}"
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 1200 --view cockpit --ride-along --slow-start 5
            ${_take_over}
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

if(TAKE_OVER)
    if(NOT _out MATCHES "took over aircraft ([0-9]+), the c172p")
        message(FATAL_ERROR "the client did not take the AI's Cessna over:\n${_out}")
    endif()
    set(_taken "${CMAKE_MATCH_1}")
    # What the server decides, not what the client thinks: that the pilot
    # has it, and that it has flown it by inputs sent since.
    if(NOT _out MATCHES "flying aircraft ${_taken}, the c172p; the server says the pilot has it, and has flown it by inputs sent since it was taken over")
        message(FATAL_ERROR "the server was not flying aircraft ${_taken} by this client's "
                            "inputs at the shot:\n${_out}")
    endif()
    if(NOT _out MATCHES "the HUD reads FLYING PILOT" OR _out MATCHES "the HUD reads FLYING AI")
        message(FATAL_ERROR "the HUD did not say the pilot has the aircraft taken over:\n${_out}")
    endif()
    message(STATUS "took the AI's Cessna, aircraft ${_taken}, over, and flew it")
    return()
endif()
if(NOT _out MATCHES "riding along in aircraft ([0-9]+), the c172p; the camera ([0-9.]+) m from its centre")
    message(FATAL_ERROR "the client did not ride along in the AI's Cessna:\n${_out}")
endif()
set(_away "${CMAKE_MATCH_2}")
if(_away GREATER_EQUAL 5)
    message(FATAL_ERROR "the camera was ${_away} m from the aircraft ridden in, not in its seat")
endif()
foreach(_line IN ITEMS "FLYING AI" "STICK [-+][0-9]\\.[0-9][0-9] [-+][0-9]\\.[0-9][0-9]"
                       "RUDDER [-+][0-9]\\.[0-9][0-9]" "THROTTLE [0-9]\\.[0-9][0-9]"
                       "FLAPS [0-9]\\.[0-9][0-9]" "GS +[0-9]+ KT")
    if(NOT _out MATCHES "the HUD reads ${_line}")
        message(FATAL_ERROR "the HUD did not read \"${_line}\" riding along:\n${_out}")
    endif()
endforeach()
# A Cessna's gear does not retract, and the HUD says nothing of it.
if(_out MATCHES "the HUD reads GEAR")
    message(FATAL_ERROR "the HUD showed a gear the Cessna does not have:\n${_out}")
endif()
message(STATUS "rode along in the AI's Cessna, the camera ${_away} m from its centre, "
               "its instruments and controls on the HUD")

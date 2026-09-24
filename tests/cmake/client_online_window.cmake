# client_online_window.cmake - the client with the window joins the server
# server.txt names.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DWORK=<scratch> -DPORT=<a port> -DDRIVER=<gpu driver>
#         -P client_online_window.cmake
#
# **Why this is separate from `client_online.cmake`.** That one flies
# `glideslope_cli`, which needs no GPU and runs everywhere. This one is the
# program a person actually flies, and it draws - so it needs a GPU driver and
# reports itself skipped where there is none. Both matter: for most of this
# project's life `--online` worked only in the command-line client, which is
# no use at all to somebody who flies rather than types.
#
# The client is run with `--shot`, so it draws two frames and stops rather
# than waiting for somebody to close a window.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_store "${WORK}/online-window.sqlite")
set(_txt "${WORK}/window-server.txt")
set(_shot "${WORK}/online-window.bmp")
file(REMOVE "${_store}" "${_txt}" "${_shot}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

file(WRITE "${_txt}" "# the club's server\n\n127.0.0.1 ${PORT} ${_key}\n")
set(ENV{GLIDESLOPE_SERVER_TXT} "${_txt}")

execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 30 --ai 0 --headless
            --timeout 60 --store "${_store}"
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 320x240
            --shot "${_shot}" --shot-at 2 --online
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT EXISTS "${_shot}")
    if(_err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")

if(NOT _out MATCHES "server.txt: 127.0.0.1 port ${PORT}")
    message(FATAL_ERROR "the client did not read server.txt:\n${_out}")
endif()
if(NOT _out MATCHES "session with ([0-9a-f]+)\n")
    message(FATAL_ERROR "the client with the window completed no session:\n${_out}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL _key)
    message(FATAL_ERROR "it joined ${CMAKE_MATCH_1}, and server.txt names ${_key}")
endif()
# A server with nothing to fly (`--ai 0`) gives nobody an aircraft, and the
# client says so and flies alone rather than waiting for ever.
if(NOT _out MATCHES "the server gave this client no aircraft; flying alone")
    message(FATAL_ERROR "the client did not say it was given no aircraft:\n${_out}")
endif()

message(STATUS "the client with the window joined ${_key} from server.txt, and "
               "still drew its frame")

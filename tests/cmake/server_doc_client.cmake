# server_doc_client.cmake - a client written from docs/TRANSPORT.md alone
# completes a session with the server.
#
#   cmake -DSERVER=<glideslope_server> -DDOC_CLIENT=<glideslope_doc_client>
#         -DSOURCE=<doc_client.cpp> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P server_doc_client.cmake
#
# **The transport item's own verification.** tests/doc_client/doc_client.cpp
# was written by somebody who read docs/TRANSPORT.md and the Noise
# specification and nothing else of this project - not its source, not its
# tests - using libsodium and the operating system's sockets. If it completes
# a session, the document is enough to write a client from; if the document
# drifts from the code, this is where it shows.
#
# **A session, completed**: the handshake, state updates read and the client's
# own aircraft found in them, the server's knocks answered, and inputs flown -
# full left aileron held, which rolls the aeroplane past 90 degrees, which no
# AI pilot on a flight plan would do (see server_fly.cmake for why that bar).
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

# **Nothing of this project in it.** A client that included the project's own
# headers would be written from the code, not the document.
file(STRINGS "${SOURCE}" _includes REGEX "^[ \t]*#[ \t]*include")
set(_counted 0)
foreach(_line IN LISTS _includes)
    math(EXPR _counted "${_counted} + 1")
    if(NOT _line MATCHES "<[^>]+>")
        message(FATAL_ERROR "the document's client includes something of its own "
                            "or of this project's: ${_line}")
    endif()
    if(_line MATCHES "glideslope|<(net|sim|world|gfx|platform|ui|frontend)/")
        message(FATAL_ERROR "the document's client includes this project: ${_line}")
    endif()
endforeach()
if(_counted EQUAL 0)
    message(FATAL_ERROR "no includes read from ${SOURCE}")
endif()

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/doc_client.sqlite")
file(REMOVE "${_store}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# The terrain first, alone, so the client is not waiting on a download.
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 10 --ai 1 --headless
            --data "${DATA}" --timeout 30 --store "${_store}"
    COMMAND "${DOC_CLIENT}" "127.0.0.1:${PORT}" "${_key}" 8
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the document's client did not complete a session:\n${_err}\n${_out}")
endif()

foreach(_said IN ITEMS "handshake complete" "state updates received: [1-9][0-9]*"
                       "my aircraft is number [0-9]+" "pings answered: [1-9][0-9]*")
    if(NOT _out MATCHES "${_said}")
        message(FATAL_ERROR "the document's client never said '${_said}':\n${_out}\n${_err}")
    endif()
endforeach()
if(NOT _out MATCHES "sent ([0-9]+) input frames, the server applied ([0-9]+)")
    message(FATAL_ERROR "the document's client said nothing about its inputs:\n${_out}")
endif()
set(_sent ${CMAKE_MATCH_1})
set(_applied ${CMAKE_MATCH_2})
if(_sent LESS 20 OR _applied EQUAL 0)
    message(FATAL_ERROR "the document's client sent ${_sent} input frames and the "
                        "server applied ${_applied}")
endif()
if(NOT _out MATCHES "my aircraft rolled to ([-0-9]+) degrees")
    message(FATAL_ERROR "the document's client never saw its own aircraft roll:\n${_out}")
endif()
set(_roll ${CMAKE_MATCH_1})
if(_roll LESS 0)
    math(EXPR _roll "0 - ${_roll}")
endif()
if(_roll LESS 90)
    message(FATAL_ERROR "the document's client held full aileron and its aircraft "
                        "reached ${_roll} degrees of bank, not past 90")
endif()
message(STATUS "a client written from TRANSPORT.md completed a session: ${_sent} input "
               "frames sent, ${_applied} applied, rolled to ${_roll} degrees")

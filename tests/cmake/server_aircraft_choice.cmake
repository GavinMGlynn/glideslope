# server_aircraft_choice.cmake - a player asks for an aeroplane as they join
# and flies it, and every other client is told it is that aeroplane.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DDATA=<data dir> -DCACHE=<downloads dir> -DWORK=<scratch>
#         -DPORT=<a port> -P server_aircraft_choice.cmake
#
# **What is pinned** (REQUIREMENTS 4.2, docs/TRANSPORT.md "Starting a
# session"): the handshake initiation's payload is the aeroplane asked for, by
# its catalogue id. The server's plan flies the Cessna 172P; one player asks
# for the Piper PA-28 (`connect --aircraft pa28`), another asks for nothing,
# a third asks for an aeroplane the catalogue does not hold, and a fourth
# sends a payload no client of this project would: a length of 33, past the
# 32 an id may have (`--asked-payload`), through a real handshake.
#
# **What must hold**: the server says the first flies the PA-28, the third
# the plan's Cessna for want of what it asked, and the fourth the Cessna for
# a payload that does not read; the first flies it - full left
# aileron, and the only player's aircraft banked past 90 degrees is the one
# every client is told is the PA-28 - and each client is told of exactly one
# PA-28 among Cessnas: the asker, the one that asked for nothing and the one
# refused alike. The others stay until the first is done (`--until-exists`):
# the event, not a time.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/choice.sqlite")
set(_ready "${WORK}/ready.txt")
set(_done "${WORK}/done.txt")
set(_asker "${WORK}/asker.txt")
set(_other "${WORK}/other.txt")
set(_stranger "${WORK}/stranger.txt")
set(_oversized "${WORK}/oversized.txt")
file(REMOVE "${_store}" "${_ready}" "${_done}" "${_asker}" "${_other}" "${_stranger}"
     "${_oversized}")
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

# The server is last, so its words are on standard output.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_ready}" --until-exists "${_done}" --heard "${_other}"
            --key e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300 --aircraft no-such-plane
            --after-ready "${_ready}" --until-exists "${_done}" --heard "${_stranger}"
            --key 3b8e1f0c2d4a5968778695a4b3c2d1e0f0e1d2c3b4a5968778695a4b3c2d1e0f
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --asked-payload 217a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a7a
            --after-ready "${_ready}" --until-exists "${_done}" --heard "${_oversized}"
            --key 5c7d9e0f1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 15 --fly
            --aircraft pa28 --after-ready "${_ready}" --done "${_done}" --heard "${_asker}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "asked for pa28, and flies it\n")
    message(FATAL_ERROR "the server did not give the PA-28 asked for:\n${_out}")
endif()
if(NOT _out MATCHES "asked for no-such-plane, which this server does not have: it flies c172p\n")
    message(FATAL_ERROR "the server did not say it had no such aeroplane:\n${_out}")
endif()
if(NOT _out MATCHES "asked for an aeroplane in a payload that does not read: it flies c172p\n")
    message(FATAL_ERROR "the server did not say the oversized payload did not read:\n${_out}")
endif()
string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees" _players "${_out}")
list(LENGTH _players _count)
if(NOT _count EQUAL 4)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft, not four:\n${_out}")
endif()
set(_flown "")
foreach(_p IN LISTS _players)
    string(REGEX MATCH "number ([0-9]+), a player's, banked as far as ([0-9]+)" _ "${_p}")
    if(CMAKE_MATCH_2 GREATER 90)
        list(APPEND _flown ${CMAKE_MATCH_1})
    endif()
endforeach()
list(LENGTH _flown _flown_count)
if(NOT _flown_count EQUAL 1)
    message(FATAL_ERROR "${_flown_count} players' aircraft banked past 90 degrees, not the "
                        "asker's alone:\n${_out}")
endif()
foreach(_who IN ITEMS asker other stranger oversized)
    file(READ "${WORK}/${_who}.txt" _said)
    string(REGEX MATCHALL "aircraft [0-9]+ is pa28\n" _pa28 "${_said}")
    list(LENGTH _pa28 _pa28_count)
    if(NOT _pa28_count EQUAL 1 OR NOT _said MATCHES "aircraft ${_flown} is pa28\n")
        message(FATAL_ERROR "the ${_who} client was not told aircraft ${_flown}, the one the "
                            "asker flew, was the PA-28 and no other:\n${_said}")
    endif()
    string(REGEX MATCHALL "aircraft [0-9]+ is c172p\n" _cessnas "${_said}")
    list(LENGTH _cessnas _cessna_count)
    if(NOT _cessna_count EQUAL 4)
        message(FATAL_ERROR "the ${_who} client was told of ${_cessna_count} Cessnas, not "
                            "four (the AI's and three players'):\n${_said}")
    endif()
endforeach()
message(STATUS "the player who asked for the PA-28 flew aircraft ${_flown} as one, and every "
               "client was told so")

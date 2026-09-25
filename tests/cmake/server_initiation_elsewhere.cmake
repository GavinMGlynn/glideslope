# server_initiation_elsewhere.cmake - a copy of a client's initiation from
# another address, arriving first, does not keep the client out.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_initiation_elsewhere.cmake
#
# **Why.** The server drops a copy of an initiation it has already taken
# (server_initiation_again.cmake). Dropped from any address, that would let
# anybody who saw a player's initiation on the wire inject a copy from a
# spoofed address to arrive first; the player's own, and all its resends, would
# then be dropped, and the player kept out. So a copy is dropped only from the
# address that already took it, and from another it is answered as before: a
# session the copier cannot read.
#
# **The race is built, not hoped for.** The client (`--first-from-elsewhere`)
# sends its initiation from a second socket first and waits for that to be
# answered; only then does it send it from its own, and fly from there - full
# aileron, which rolls its aeroplane past 90 degrees (server_fly.cmake).
#
# **What must hold**: the key admitted twice, once for each address; an
# aircraft banked past 90 degrees, which only the client flying from its own
# address can have done; and the copy's session, which nobody speaks in, let
# go and its aircraft unflown.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/elsewhere.sqlite")
set(_ready "${WORK}/ready.txt")
file(REMOVE "${_store}" "${_ready}")

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

# The key whose public half begins 11fc7622 (see server_slots.cmake).
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8 --fly
            --first-from-elsewhere --after-ready "${_ready}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 3 --store "${_store}"
            --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

string(REGEX MATCHALL "admitted 11fc7622" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the key was admitted ${_admissions} times, not twice - once "
                        "for the copy's address and once for its own:\n${_out}\n${_err}")
endif()
if(_out MATCHES "dropped a copy")
    message(FATAL_ERROR "the server dropped the client's own initiation for being a "
                        "copy of one taken from another address:\n${_out}")
endif()

string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees"
       _players "${_out}")
list(LENGTH _players _count)
if(NOT _count EQUAL 2)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft, not two:\n"
                        "${_out}\n${_err}")
endif()
set(_flown 0)
foreach(_player IN LISTS _players)
    string(REGEX MATCH "banked as far as ([0-9]+)" _ "${_player}")
    if(CMAKE_MATCH_1 GREATER_EQUAL 90)
        math(EXPR _flown "${_flown} + 1")
    endif()
endforeach()
if(NOT _flown EQUAL 1)
    message(FATAL_ERROR "${_flown} players' aircraft banked past 90 degrees, not one - "
                        "the client's own, flown from its own address:\n${_out}\n${_err}")
endif()
message(STATUS "a copy from another address answered first did not keep the client out: "
               "admitted twice, its own aircraft flown")

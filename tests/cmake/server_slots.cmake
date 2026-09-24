# server_slots.cmake - four players, joining in the reverse of their keys'
# order, each fly their own aircraft under a number of its own.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_slots.cmake
#
# **The situation is built, not hoped for.** A slot is a key's rank among the
# players present (net/slots.hpp), so a player whose key sorts first moves
# everybody after them. The server numbered a player's aircraft by the slot it
# had on arrival, so the next to arrive could be handed a number already
# flying - two clients then each read one line of the state update as their
# own, and one aeroplane flew by both. Four fixed keys, joined a second apart
# from the one that sorts last to the one that sorts first, make every arrival
# move everybody already in: the worst order, every time. (Their public halves
# begin f661..., a189..., 6d9d... and 11fc...; the secrets are sha256 of
# "glideslope slot test player N".)
#
# Each client holds full aileron, which rolls its own aeroplane past 90
# degrees (see server_fly.cmake). **Four different numbers, and four
# aeroplanes over past 90**, say that each client's inputs flew its own; an
# aeroplane flown by two clients, or by none, would say otherwise.
#
# The server is last in the pipeline so that what it prints is what is read:
# every aircraft's number and how far it banked. It needs the DEM's tiles, so
# without the network it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/slots.sqlite")
file(REMOVE "${_store}")

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

# From the key that sorts last to the one that sorts first.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8 --fly --after 1
            --key e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8 --fly --after 2
            --key d1b109e3db55e52705b4664f92a64ab2c0a03c0e6d62fb9af829e908a06d48fc
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8 --fly --after 3
            --key 103fdaa7d7170fce52c4ee825179422582fc096901f46a78d763058a9aa74d1e
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8 --fly --after 4
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${SERVER}" --port ${PORT} --seconds 13 --ai 1 --headless --players 4
            --data "${DATA}" --timeout 30 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees"
       _players "${_out}")
list(LENGTH _players _count)
if(NOT _count EQUAL 4)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft, not four:\n"
                        "${_out}\n${_err}")
endif()
set(_numbers "")
foreach(_player IN LISTS _players)
    string(REGEX MATCH "number ([0-9]+), a player's, banked as far as ([0-9]+)"
           _ "${_player}")
    set(_number ${CMAKE_MATCH_1})
    set(_bank ${CMAKE_MATCH_2})
    if(_number IN_LIST _numbers)
        message(FATAL_ERROR "two players' aircraft are both number ${_number}:\n${_out}")
    endif()
    list(APPEND _numbers ${_number})
    if(_bank LESS 90)
        message(FATAL_ERROR "the player's aircraft number ${_number} banked only "
                            "${_bank} degrees: its client's full aileron did not fly it\n"
                            "${_out}")
    endif()
endforeach()
message(STATUS "four players joined in the worst order and flew aircraft ${_numbers}, "
               "each past 90 degrees")

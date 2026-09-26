# server_behind.cmake - a server behind real time hears all of its clients.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_behind.cmake
#
# **Built, not waited for.** A server that falls behind real time takes four
# steps between looks at its socket. Reading one datagram a look, it read some
# twenty-seven a second on a loaded CI runner while four clients sent a hundred
# and twenty: the rest waited in its socket or were dropped, and a client still
# joining went unheard for three seconds and was let go (2026-09-26). Here every
# step is made to take 30 ms (`--test-step-ms`), which puts the server at about
# a quarter of real time on any machine, and four clients join and fly
# as in server_slots.cmake, against a server that lets a client go after three
# seconds of silence. Every one must be heard: four aircraft, each banked past
# 90 degrees, nobody let go before they had flown, and the server says it was
# behind - or the situation was not built.
#
# It needs the DEM's tiles; without the network it reports itself skipped.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/behind.sqlite")
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
    # Until the last of them has gone, however long a slow machine takes to
    # let them all in: a fixed thirteen seconds stopped a server on CI before
    # the fourth client had flown its aeroplane over.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 3 --test-step-ms 30
            --store "${_store}"
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
if(NOT _out MATCHES "was ([0-9]+) steps behind at the end")
    message(FATAL_ERROR "the server did not say how far behind it was:\n${_out}")
endif()
set(_behind "${CMAKE_MATCH_1}")
if(_behind LESS 240)
    message(FATAL_ERROR "the server was only ${_behind} steps behind: the "
                        "situation was not built\n${_out}")
endif()
if(_out MATCHES "dropped a copy of an initiation")
    message(FATAL_ERROR "a client's handshake went unheard long enough to be sent "
                        "again after its session was let go:\n${_out}")
endif()
message(STATUS "a server ${_behind} steps behind heard all four players, who flew "
               "aircraft ${_numbers}, each past 90 degrees")

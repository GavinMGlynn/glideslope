# server_initiation_again.cmake - a copy of a client's initiation that arrives
# after the server has let its session go makes no second player.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_initiation_again.cmake
#
# **What CI saw.** The four-player test counted six players' aircraft (run
# 36140964489, Linux debug): the fourth client was admitted three times from
# one address. A client resends its initiation until it is answered, and a
# server slow to read its socket had copies of it still to read after the
# session the first copy made had been let go. Each was taken for a new
# handshake - a new session and a new aircraft - whose answer the client, on
# the keys of the first, ignored; so the server heard nothing under it, let it
# go after --timeout, and counted an aircraft nobody had flown.
#
# **The situation is built, not hoped for.** One client (`--again-when-let-go`)
# completes its handshake and then says nothing, answering none of the
# server's knocks, until the knocks stop - which is the server letting it go -
# and then sends the same initiation once more, from the same address. A second
# client flies for long enough to keep the server running past all of that
# (it stops when everybody who joined has gone, --until-empty).
#
# **What must hold**: the silent client admitted once and let go once; one
# aircraft of its own, not two; the other client's aircraft flown by it, past
# 90 degrees; and the copy not answered at all. An answer the same as the first
# would mean the copy reached a session still live - the situation not built -
# and fails too, rather than passing for the wrong reason.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/again.sqlite")
set(_heard "${WORK}/heard.txt")
set(_ready "${WORK}/ready.txt")
file(REMOVE "${_store}" "${_heard}" "${_ready}")

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

# The silent client's key: its public half begins 11fc7622 (see
# server_slots.cmake). The flying one's begins f661fe1f.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 5 --again-when-let-go
            --after-ready "${_ready}" --heard "${_heard}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 20 --fly
            --after-ready "${_ready}"
            --key e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 3 --store "${_store}"
            --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT EXISTS "${_heard}")
    message(FATAL_ERROR "the silent client said nothing of its initiation:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _verdict)
if(_verdict MATCHES "answered as before")
    message(FATAL_ERROR "the copy reached a session still live, so the situation was "
                        "not built - the server had not let the client go:\n${_out}")
endif()
if(_verdict MATCHES "answered afresh")
    message(FATAL_ERROR "the server took a copy of an initiation it had already taken "
                        "for a new handshake:\n${_out}")
endif()
if(NOT _verdict MATCHES "was not answered")
    message(FATAL_ERROR "the silent client's verdict is not one it gives: ${_verdict}")
endif()

string(REGEX MATCHALL "admitted 11fc7622" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 1)
    message(FATAL_ERROR "the silent client was admitted ${_admissions} times, not once:\n"
                        "${_out}")
endif()
string(REGEX MATCHALL "admitted f661fe1f" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 1)
    message(FATAL_ERROR "the flying client was admitted ${_admissions} times, not once:\n"
                        "${_out}")
endif()
string(REGEX MATCHALL "let go [0-9.:]+ after" _gone "${_out}")
list(LENGTH _gone _lets_go)
if(NOT _lets_go EQUAL 2)
    message(FATAL_ERROR "the server let ${_lets_go} sessions go, not two:\n${_out}")
endif()

# Two players, two aircraft: the silent one's, barely banked because nobody
# flew it, and the flying one's, past 90 degrees.
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
                        "the flying client's:\n${_out}")
endif()
message(STATUS "a copy of an initiation read after its session had gone made nothing: "
               "one admission, two players' aircraft, one flown")

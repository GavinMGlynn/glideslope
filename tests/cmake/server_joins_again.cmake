# server_joins_again.cmake - a client the server has let go, for going quiet
# past its --timeout, joins again by itself and flies again.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_joins_again.cmake
#
# **What was wrong.** A client the server had let go - a laptop shut, a
# process stopped in a debugger, a network gone for longer than --timeout -
# went on sealing under keys the server had thrown away. The server refused
# every datagram, the client ignored every refusal, and nobody flew its
# aircraft again until it was restarted by hand.
#
# **The situation is built, not hoped for.** One client flies full left
# aileron until its aircraft has rolled past 90 degrees, then stops
# (`--stall-once-rolled`): it sends nothing and answers nothing until the
# server's knocks stop, which is the server letting it go. Then it goes on as
# it was, and must notice by itself that it has been let go, join again, and
# fly its new aircraft past 90 degrees too. A second client, which does not
# fly, stays until the first has gone (`--done`, `--until-exists`), which
# keeps the server - stopping when everybody who joined has gone,
# --until-empty - running through the gap between the two sessions however
# slow the machine.
#
# **What must hold**: the stalling client admitted twice, and let go twice -
# once for its silence, once for its goodbye; the client saying it joined
# again; three players' aircraft and no more - the stalling client's first,
# taken out of the sky when it was let go, its second, and the other
# client's - with exactly the first two banked past 90 degrees: no ghost
# aircraft made by a copy of an old initiation, and the client flying the one
# it was given. And the server dropping no copy of the first initiation as a
# fresh one - joining again is a new initiation, not a copy.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/again.sqlite")
set(_done "${WORK}/done.txt")
set(_ready "${WORK}/ready.txt")
file(REMOVE "${_store}" "${_done}" "${_ready}")

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

# The stalling client's key: its public half begins 11fc7622 (see
# server_slots.cmake). The other one's begins f661fe1f.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300 --fly --stall-once-rolled
            --after-ready "${_ready}" --done "${_done}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_ready}" --until-exists "${_done}"
            --key e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 3 --store "${_store}"
            --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT EXISTS "${_done}")
    message(FATAL_ERROR "the stalling client never finished:\n${_out}\n${_err}")
endif()
file(READ "${_done}" _said)
if(NOT _said MATCHES "done 0")
    message(FATAL_ERROR "the stalling client failed (${_said}):\n${_out}\n${_err}")
endif()

string(REGEX MATCHALL "admitted 11fc7622" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the stalling client was admitted ${_admissions} times, not twice "
                        "- once, and again after it was let go:\n${_out}\n${_err}")
endif()
string(REGEX MATCHALL "admitted f661fe1f" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 1)
    message(FATAL_ERROR "the other client was admitted ${_admissions} times, not once:\n"
                        "${_out}")
endif()
string(REGEX MATCHALL "let go [0-9.:]+ after [0-9.]+ s of silence" _silence "${_out}")
list(LENGTH _silence _silences)
if(NOT _silences EQUAL 1)
    message(FATAL_ERROR "the server let ${_silences} sessions go for silence, not one - "
                        "the stall:\n${_out}")
endif()
if(_out MATCHES "dropped a copy of an initiation")
    message(FATAL_ERROR "joining again was taken for a copy of the first initiation:\n"
                        "${_out}")
endif()

string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees"
       _players "${_out}")
list(LENGTH _players _count)
if(NOT _count EQUAL 3)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft, not three - the "
                        "stalling client's two and the other's:\n${_out}\n${_err}")
endif()
set(_flown 0)
foreach(_player IN LISTS _players)
    string(REGEX MATCH "banked as far as ([0-9]+)" _ "${_player}")
    if(CMAKE_MATCH_1 GREATER_EQUAL 90)
        math(EXPR _flown "${_flown} + 1")
    endif()
endforeach()
if(NOT _flown EQUAL 2)
    message(FATAL_ERROR "${_flown} players' aircraft banked past 90 degrees, not two - "
                        "the stalling client's, before and after it joined again:\n${_out}")
endif()
message(STATUS "a client let go for its silence joined again by itself: admitted twice, "
               "three players' aircraft, its two flown")

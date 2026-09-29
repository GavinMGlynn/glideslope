# server_initiation_replayed_while_live.cmake - a copy of a live player's
# initiation, replayed from another address, takes neither the player's
# session nor its aircraft.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_initiation_replayed_while_live.cmake
#
# **Why.** A second session for a key takes over the first's slot and
# aircraft (server_one_key_two_addresses.cmake). Anybody who saw a player's
# initiation on the wire can send a copy of it from an address of their own,
# and the server answers it: taken over at the answer, that copy would end the
# player's session for the price of one datagram. So a new session takes over
# only once something sealed under it opens, which needs the initiation's
# ephemeral secret - the client's, and nobody else's.
#
# **The situation is built, not hoped for.** The client
# (`--again-from-elsewhere`) completes its session, then sends a copy of its
# initiation from a second port and waits for the server to answer it - so the
# copy's session exists before the client flies on - and then says nothing
# from that port again, as a replayer who cannot read the answer could not.
# It flies from its own port - full aileron, past 90 degrees
# (server_fly.cmake) - and leaves; the copy's session is let go for silence,
# and the server runs until everybody has gone (`--until-empty`).
#
# **What must hold**: the key admitted twice, once from each address; nothing
# taken over; the client never told it is leaving; the copy's session let go
# for silence; and one player's aircraft, banked past 90 degrees by the
# client that kept it.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/replayed.sqlite")
set(_ready "${WORK}/ready.txt")
set(_heard "${WORK}/heard.txt")
file(REMOVE "${_store}" "${_ready}" "${_heard}")

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

execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8 --fly
            --again-from-elsewhere --after-ready "${_ready}" --heard "${_heard}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 3 --store "${_store}"
            --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

string(REGEX MATCHALL "admitted 11fc7622" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the key was admitted ${_admissions} times, not twice - once "
                        "for the client and once for the copy:\n${_out}\n${_err}")
endif()
if(_out MATCHES "took over from")
    message(FATAL_ERROR "a replayed initiation took over a live session:\n${_out}\n${_err}")
endif()
if(EXISTS "${_heard}")
    file(READ "${_heard}" _said)
    if(_said MATCHES "dropped by the server's operator")
        message(FATAL_ERROR "the client was told to leave for a replay of its own "
                            "initiation:\n${_said}\n${_out}")
    endif()
endif()
string(REGEX MATCHALL "let go 127\\.0\\.0\\.1:[0-9]+ after [0-9.]+ s of silence" _silent
       "${_out}")
list(LENGTH _silent _silences)
if(NOT _silences EQUAL 1)
    message(FATAL_ERROR "${_silences} sessions were let go for silence, not one - the "
                        "copy's:\n${_out}\n${_err}")
endif()

string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees"
       _players "${_out}")
list(LENGTH _players _count)
if(NOT _count EQUAL 1)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft for one key, not "
                        "one:\n${_out}\n${_err}")
endif()
string(REGEX MATCH "banked as far as ([0-9]+)" _ "${_players}")
if(CMAKE_MATCH_1 LESS 90)
    message(FATAL_ERROR "the player's aircraft banked only ${CMAKE_MATCH_1} degrees - the "
                        "client did not keep flying it:\n${_out}\n${_err}")
endif()
message(STATUS "a replayed initiation took nothing from a live player: admitted twice, "
               "nothing taken over, one aircraft, flown past 90 degrees")

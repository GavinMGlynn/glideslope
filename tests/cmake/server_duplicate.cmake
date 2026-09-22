# server_duplicate.cmake - a repeated handshake initiation does not take a
# live session away from the client that has one.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DWORK=<a scratch directory> -DPORT=<a port> -P server_duplicate.cmake
#
# **The bug this pins was real and was here.** The server made a session for
# every `HANDSHAKE_INITIATION` it could answer and wrote it over whatever that
# address already had. A duplicated datagram - which a network does on its
# own, and which this client's own retransmission makes likely - would agree a
# second set of keys at the server while the client went on sealing under the
# first. Everything the client said from then on failed to open, in silence,
# and the server let it go at the timeout as though it had walked away.
#
# Anybody can make a valid `IK` initiation to a public key, so the same
# datagram from a forged address was a way to put a player off a server for
# the price of one packet.
#
# **What is held here**: the client completes a session, sends its initiation
# a second time with `--again`, and then goes on answering the server's pings
# for three seconds. If the server had rekeyed, not one of them would open.
#
# It needs no network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

set(_store "${WORK}/duplicate.sqlite")
file(REMOVE "${_store}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 3 --again
    COMMAND "${SERVER}" --port ${PORT} --seconds 4 --ai 0 --plain
            --timeout 30 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server stopped badly:\n${_err}\n${_out}")
endif()

# One slot, not two: the duplicate did not make a second session.
string(FIND "${_out}" "--- dashboard at " _at REVERSE)
if(_at EQUAL -1)
    message(FATAL_ERROR "there is no dashboard to read:\n${_out}")
endif()
string(SUBSTRING "${_out}" ${_at} -1 _last)
string(REGEX MATCHALL "\n  [0-9]+ +[0-9a-f]+ +[0-9]" _taken "${_last}")
list(LENGTH _taken _count)
if(NOT _count EQUAL 1)
    message(FATAL_ERROR "${_count} slots have a client on them after one client "
                        "connected twice:\n${_last}")
endif()

# And that one client is still being heard: a round trip means a ping went out
# and a pong came back, under the keys the first handshake agreed.
if(NOT _last MATCHES "\n  0     ([0-9a-f]+) +([0-9]+) +")
    message(FATAL_ERROR
            "slot 0 has no round trip on it, so nothing the client sealed after "
            "its duplicate initiation could be opened - the server took the "
            "duplicate for a fresh handshake:\n${_last}")
endif()
set(_who "${CMAKE_MATCH_1}")
set(_ping "${CMAKE_MATCH_2}")
if(_ping GREATER 500)
    message(FATAL_ERROR "the round trip is given as ${_ping} ms on loopback")
endif()

message(STATUS "one client, one slot: ${_who} sent its initiation twice and "
               "kept its session, round trip ${_ping} ms")

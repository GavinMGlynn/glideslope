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
# (`--again-from-elsewhere FILE`) completes its session, then sends a copy of
# its initiation from a second port and waits for the server to answer it - so
# the copy's session exists before the client flies on. From then on it resends
# the copy every quarter of a second, faster than the server's six-second
# --timeout, as a replayer holding the session open would, and beside it a
# sealed datagram that opens under nothing, which the server refuses only once
# the copy's session has gone. On that refusal it writes FILE, and the client,
# flying from its own port all the while - full aileron, past 90 degrees
# (server_fly.cmake) - leaves (`--until-exists FILE`). So the copy's session is
# gone before the player leaves, whatever the machine's speed.
#
# **What must hold**:
# - the key admitted twice, once from each address; nothing taken over; the
#   client never told its session was ended;
# - the copy's session let go for silence, once, although its initiation was
#   resent all along - a copy of an initiation keeps nothing alive;
# - the copy's session sent nothing besides its answer: no state, no pings;
# - the player's aircraft still there after the copy's session went - it is
#   taken out of the sky only after the player said it was leaving - and one
#   player's aircraft in all, banked past 90 degrees by the client that kept it.
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
set(_gone "${WORK}/copy_gone.txt")
file(REMOVE "${_store}" "${_ready}" "${_heard}" "${_gone}")

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
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 120 --fly
            --again-from-elsewhere "${_gone}" --until-exists "${_gone}"
            --after-ready "${_ready}" --heard "${_heard}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 6 --store "${_store}"
            --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT EXISTS "${_gone}")
    message(FATAL_ERROR "the copy's session was never let go while its initiation was "
                        "resent:\n${_out}\n${_err}")
endif()
file(READ "${_gone}" _copy)
if(NOT _copy MATCHES "sent 0 datagrams besides its answer")
    message(FATAL_ERROR "the copy's session, which sealed nothing, was sent more than its "
                        "answer: ${_copy}\n${_out}")
endif()

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
    if(_said MATCHES "the server ended this session")
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

# The aircraft outlived the copy's session: it went out of the sky only after
# the player said it was leaving.
string(FIND "${_out}" "after it said it was leaving" _left_at)
string(FIND "${_out}" "their aircraft is out of the sky" _gone_at)
if(_left_at EQUAL -1 OR _gone_at EQUAL -1 OR _gone_at LESS _left_at)
    message(FATAL_ERROR "the player's aircraft went before the player did - with the "
                        "copy's session:\n${_out}\n${_err}")
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
message(STATUS "a replayed initiation, resent, took nothing from a live player and "
               "was sent nothing: let go for silence, nothing taken over, one aircraft, "
               "kept until the player left")

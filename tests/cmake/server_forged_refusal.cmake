# server_forged_refusal.cmake - a client refused by a forger while its
# session is merely quiet tries to join again, goes back to its old session
# when that answers, and the server makes no second player.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P server_forged_refusal.cmake
#
# **What is pinned.** A refusal is sent in the clear, so anybody who can write
# the server's address on a datagram can send one. A client believes
# `BAD_HANDSHAKE` only after three seconds in which nothing has opened under
# its session (docs/TRANSPORT.md), and then joins again - but it keeps the old
# session's keys, and if anything opens under them it goes back: the session
# was never gone, and a forged refusal costs it nothing. The server, which
# still has the session, drops the new initiation from that address without a
# word. Until this test the going back was reviewed, never exercised.
#
# **The situation is built, not hoped for.** The client reaches the server
# through the relay (glideslope_impair, at PORT + 1), told
# `--forge-refusal-after 50`: once fifty datagrams have come from the server,
# it holds everything the server sends - the session goes quiet, but nothing
# is lost and the server still hears the client - and answers every datagram
# the client sends with a `BAD_HANDSHAKE` from the server's address that the
# server never sent. Those heard before three seconds of quiet are ignored;
# the first heard after is believed. The hold ends on the event that follows,
# not a time: the client's new initiation, passed on to the server, after
# which what was held is delivered and nothing more is forged. The client
# flies full left aileron and, back in a session, leaves once its aircraft has
# rolled past 90 degrees in it (`--leave-once-back`); the server stops when it
# has gone (--until-empty).
#
# **What must hold**: the relay forged and its hold ended on the client's
# initiation; the client believed a refusal after at least three seconds of
# nothing, went back to its old session, and did not join a new one; there,
# its inputs were numbered on from where they were, and the server applied one
# sent after it went back, before it left; the
# server admitted it once, let it go once - for its goodbye, not for silence -
# and flew one player's aircraft, banked past 90 degrees.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/forged.sqlite")
set(_ready "${WORK}/ready.txt")
set(_done "${WORK}/done.txt")
file(REMOVE "${_store}" "${_ready}" "${_done}")

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

# The server's words reach standard error through the relay; the client's
# are on standard output. The client's key's public half begins 11fc7622
# (see server_slots.cmake).
math(EXPR _relay "${PORT} + 1")
# **The server's --timeout is long, 30 s**: the session must be merely quiet,
# never let go, and the client's own three seconds are what is tested. At 5 s
# the window client, building its flight under load in a debug build, was
# silent long enough that the server let it go before the forger's hold had
# ended, and the client went back to a session already gone (2026-10-01).
# Its silence counts from what the client sends, which the relay never holds.
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 30 --store "${_store}" --ready-file "${_ready}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
            --seed 1 --until-input-ends --seconds 290 --forge-refusal-after 50
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 280 --fly --leave-once-back
            --after-ready "${_ready}" --done "${_done}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

# **The forger did its work, and the client answered it by trying to join
# again**: without both, nothing below tests anything.
set(_forger_said "impair: forged ([0-9]+) refusals while holding the server's datagrams; the hold ended on a client's initiation")
if(NOT _err MATCHES "${_forger_said}")
    string(REGEX MATCH "impair: forged[^\n]*" _forger "${_err}")
    message(FATAL_ERROR "the client never tried to join again (${_forger}):\n${_out}\n${_err}")
endif()
if(CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "the relay forged no refusal:\n${_err}")
endif()
if(NOT _out MATCHES "let go by the server: refused BAD_HANDSHAKE after ([0-9]+)\\.[0-9] s of nothing")
    message(FATAL_ERROR "the client did not say it believed a refusal:\n${_out}\n${_err}")
endif()
if(CMAKE_MATCH_1 LESS 3)
    message(FATAL_ERROR "the client believed a refusal after less than three seconds of "
                        "nothing:\n${_out}")
endif()

# **It went back, and joined nothing new.**
if(NOT _out MATCHES "the old session answered; staying in it, its inputs numbered on from ([0-9]+)\n")
    message(FATAL_ERROR "the client did not go back to its old session:\n${_out}\n${_err}")
endif()
set(_numbered_on_from "${CMAKE_MATCH_1}")
# **Back in the old session, its inputs are numbered on**, not from 1 again:
# the server's count of them did not start again, and inputs numbered afresh
# were all older than the newest it had applied, and dropped.
if(_numbered_on_from LESS 1)
    message(FATAL_ERROR "the client numbered its inputs from 1 again in the old session:\n"
                        "${_out}")
endif()
# And the server applied one sent since, and none it was not sent: the last
# stay's own words.
string(REGEX MATCHALL "sent [0-9]+ input frames, the server applied [0-9]+" _applied "${_out}")
list(GET _applied -1 _last)
string(REGEX MATCH "sent ([0-9]+) input frames, the server applied ([0-9]+)" _ "${_last}")
set(_last_sent "${CMAKE_MATCH_1}")
set(_last_applied "${CMAKE_MATCH_2}")
if(_last_applied GREATER _last_sent)
    message(FATAL_ERROR "the server applied input ${_last_applied} of the ${_last_sent} the "
                        "client numbered:\n${_out}")
endif()
if(NOT _last_applied GREATER _numbered_on_from)
    message(FATAL_ERROR "the server applied input ${_last_applied}, none of those sent after "
                        "${_numbered_on_from}, when the client went back:\n${_out}\n${_err}")
endif()
if(_out MATCHES "joined again: session with")
    message(FATAL_ERROR "the client joined a new session rather than going back:\n"
                        "${_out}\n${_err}")
endif()
if(NOT EXISTS "${_done}")
    message(FATAL_ERROR "the client never finished:\n${_out}\n${_err}")
endif()
file(READ "${_done}" _said)
if(NOT _said MATCHES "done 0")
    message(FATAL_ERROR "the client failed (${_said}):\n${_out}\n${_err}")
endif()

# **The server made no second player.**
string(REGEX MATCHALL "admitted [0-9a-f]+ to slot" _admitted "${_err}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 1)
    message(FATAL_ERROR "the server admitted ${_admissions} sessions, not one:\n${_err}")
endif()
string(REGEX MATCHALL "let go [0-9.:]+ after [0-9.]+ s of silence" _silence "${_err}")
list(LENGTH _silence _silences)
if(NOT _silences EQUAL 0)
    message(FATAL_ERROR "the server let ${_silences} sessions go for silence, not none:\n"
                        "${_err}")
endif()
string(REGEX MATCHALL "let go [0-9.:]+ after it said it was leaving" _goodbyes "${_err}")
list(LENGTH _goodbyes _goodbye_count)
if(NOT _goodbye_count EQUAL 1)
    message(FATAL_ERROR "the server let ${_goodbye_count} sessions go for a goodbye, not "
                        "one:\n${_err}")
endif()
string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees"
       _players "${_err}")
list(LENGTH _players _count)
if(NOT _count EQUAL 1)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft, not one:\n${_err}")
endif()
string(REGEX MATCH "banked as far as ([0-9]+)" _ "${_players}")
if(CMAKE_MATCH_1 LESS 90)
    message(FATAL_ERROR "the player's aircraft banked only ${CMAKE_MATCH_1} degrees, not past "
                        "90:\n${_err}")
endif()
list(GET _rcs 0 _server_rc)
if(NOT _server_rc EQUAL 0)
    message(FATAL_ERROR "the server failed (${_rcs}):\n${_err}")
endif()
message(STATUS "a client refused by a forger while its session was quiet went back to it: "
               "admitted once, one player's aircraft")

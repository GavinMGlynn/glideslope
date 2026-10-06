# server_stale_rejoin.cmake - a client whose session the server really let
# go, with the server's last updates held on the way until it tries to join
# again, joins a new session rather than going back to the dead one, and the
# server admits it once.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P server_stale_rejoin.cmake
#
# **What is pinned.** A client joining again keeps its old session's keys, in
# case the refusal that ended it was forged. Until 2026-10-06 anything that
# opened under them sent it back - and an update the server sealed before it
# let the session go opens as well as a new one. So a client whose updates
# were held on the way went back to a session already gone, while the server
# had admitted its new initiation: a ghost session holding a slot and an
# aircraft until its timeout, and the client lost for about 13 s. Now it goes
# back only on the old session's answer to a knock it sent while joining
# again (net::Rejoin), which nothing held from before can be.
#
# **The situation is built, not hoped for.** The client reaches the server
# through the relay (glideslope_impair, at PORT + 1), told
# `--hold-until-let-go-after 50`: once fifty datagrams have come from the
# server it holds everything the server sends and drops everything the
# client sends, replaying to the server a datagram it has already opened -
# dropped in silence while it has the session, refused once it has let it go.
# That refusal is the event: from it the relay passes the server's refusals
# on and the client's datagrams through, so the client hears a real
# `BAD_HANDSHAKE` and believes it. Its new initiation ends the hold, and what
# was held - updates sealed before the let-go - reaches the client first.
# The client flies full left aileron and, in its new session, leaves once
# the server has applied an input it sent there (`--leave-once-back`).
#
# **What must hold**: the relay saw the server let the session go during the
# hold, and the hold ended on the client's initiation with updates held; the
# client heard some of them open under the old session and did not go back,
# but joined again; the server admitted it twice - first, and once joining
# again - let it go for silence once (the session the relay starved), never
# a second time (a ghost), and for its goodbye and the staying client's.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/stale.sqlite")
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
# are on standard output. **The server's --timeout is short, 3 s**: the
# session must be let go, and how soon only shortens the test - the relay
# waits on the server's refusal, not on a time. **A second client, straight
# to the server, stays until the first is done** (`--until-exists`): the
# server stops when everybody who joined has gone (--until-empty), and
# without it the server stopped as it let the first client go.
math(EXPR _relay "${PORT} + 1")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_ready}" --until-exists "${_done}"
            --key e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
            --seed 1 --until-input-ends --seconds 290 --hold-until-let-go-after 50
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 280 --fly --leave-once-back
            --after-ready "${_ready}" --done "${_done}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

# **The relay built the situation**: without it, nothing below tests anything.
set(_relay_said "impair: held the server's datagrams; the server let the session go after [0-9]+ replays; [0-9]+ dropped from the client meanwhile; the hold ended on a client's initiation; ([0-9]+) held delivered after it, 0 dropped past its cap")
if(NOT _err MATCHES "${_relay_said}")
    string(REGEX MATCH "impair: held[^\n]*" _said "${_err}")
    message(FATAL_ERROR "the relay did not build the situation (${_said}):\n${_out}\n${_err}")
endif()
if(CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "the relay held nothing from the server to deliver late:\n${_err}")
endif()
if(NOT _out MATCHES "let go by the server: refused BAD_HANDSHAKE after [0-9.]+ s of nothing")
    message(FATAL_ERROR "the client did not say it believed a refusal:\n${_out}\n${_err}")
endif()

# **Updates from before the let-go opened, and were not gone back for.**
if(_out MATCHES "the old session answered")
    message(FATAL_ERROR "the client went back to a session the server had let go:\n"
                        "${_out}\n${_err}")
endif()
if(NOT _out MATCHES "while joining again, ([0-9]+) opened under the old session that were not its answer")
    message(FATAL_ERROR "the client did not say what opened while it joined again:\n${_out}")
endif()
if(CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "no held update opened under the old session while the client joined "
                        "again, so going back was not tested:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "joined again: session with")
    message(FATAL_ERROR "the client did not join again:\n${_out}\n${_err}")
endif()
if(NOT EXISTS "${_done}")
    message(FATAL_ERROR "the client never finished:\n${_out}\n${_err}")
endif()
file(READ "${_done}" _said)
if(NOT _said MATCHES "done 0")
    message(FATAL_ERROR "the client failed (${_said}):\n${_out}\n${_err}")
endif()

# **The server admitted it once joining again, and held no ghost.**
string(REGEX MATCHALL "admitted 11fc7622 to slot" _admitted "${_err}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the server admitted ${_admissions} sessions, not two (the first and "
                        "one joining again):\n${_err}")
endif()
string(REGEX MATCHALL "let go [0-9.:]+ after [0-9.]+ s of silence" _silence "${_err}")
list(LENGTH _silence _silences)
if(NOT _silences EQUAL 1)
    message(FATAL_ERROR "the server let ${_silences} sessions go for silence, not one - a "
                        "second is a ghost:\n${_err}")
endif()
string(REGEX MATCHALL "let go [0-9.:]+ after it said it was leaving" _goodbyes "${_err}")
list(LENGTH _goodbyes _goodbye_count)
if(NOT _goodbye_count EQUAL 2)
    message(FATAL_ERROR "the server let ${_goodbye_count} sessions go for a goodbye, not "
                        "two (the client's new session and the one that stayed):\n${_err}")
endif()
list(GET _rcs 1 _server_rc)
if(NOT _server_rc EQUAL 0)
    message(FATAL_ERROR "the server failed (${_rcs}):\n${_err}")
endif()
message(STATUS "a client let go with its updates held joined again, admitted once, no ghost")

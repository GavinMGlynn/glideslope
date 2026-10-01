# client_forged_refusal.cmake - the client with the window, refused by a
# forger while its session is merely quiet, tries to join again, goes back to
# its old session when that answers, and the server makes no second player.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DIMPAIR=<glideslope_impair>
#         -DDATA=<data> -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -P client_forged_refusal.cmake
#
# The command-line client's half is server_forged_refusal.cmake, which says
# why. **The situation is built the same way**: the client reaches the server
# through the relay (glideslope_impair, at PORT + 1) told
# `--forge-refusal-after 50` - once fifty datagrams have come from the server
# it holds everything the server sends, and answers everything the client
# sends with a `BAD_HANDSHAKE` the server never sent, until the client's new
# initiation, which ends the hold and lets what was held go on its way.
# **The shot waits on the events**, not the clock (`--shot-once-back`): it is
# held, up to a minute of the flight past its tick, until the client has gone
# back to its old session and the server has flown it by an input sent since.
#
# **What must hold**: the relay forged and its hold ended on the client's
# initiation; the client believed a refusal after at least three seconds of
# nothing, went back to its old session, joined no new one, and at the shot
# was flown by an input sent since it went back, by the pilot; the server
# admitted it once, let it go once - for its goodbye, not for silence.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/forged.sqlite")
set(_shot "${WORK}/forged.bmp")
file(REMOVE "${_store}" "${_shot}")

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

set(ENV{LSAN_OPTIONS} "exitcode=0")
# The server's words reach standard error through the relay; the client's are
# on standard output.
math(EXPR _relay "${PORT} + 1")
# **The server's --timeout is long, 30 s**: the session must be merely quiet,
# never let go, and the client's own three seconds are what is tested. At 5 s
# the window client, building its flight under load in a debug build, was
# silent long enough that the server let it go before the forger's hold had
# ended, and the client went back to a session already gone (2026-10-01).
# Its silence counts from what the client sends, which the relay never holds.
# The client connects once the server is flying (client.cmake says why).
set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 30 --store "${_store}" --ready-file "${_ready}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
            --seed 1 --until-input-ends --seconds 290 --forge-refusal-after 50
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --view cockpit --shot-at 1200 --shot-once-back
            --after-ready "${_ready}" --server 127.0.0.1 ${_relay} --server-key ${_key}
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
list(GET _rcs -1 _rc)

# **A machine that cannot draw skips, before anything is judged** (as in
# client_joins_again.cmake): the client's own words only.
if(_err MATCHES "glideslope: no GPU device")
    message(STATUS "the client cannot draw here: ${_err}")
    cmake_language(EXIT 77)
endif()
if(NOT _out MATCHES "the server gave this client aircraft [0-9]+")
    message(FATAL_ERROR "the client was given no aircraft:\n${_out}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

set(_forger_said "impair: forged ([0-9]+) refusals while holding the server's datagrams; the hold ended on a client's initiation after [0-9.]+ s; 0 dropped past its cap")
if(NOT _err MATCHES "${_forger_said}")
    string(REGEX MATCH "impair: forged[^\n]*" _forger "${_err}")
    message(FATAL_ERROR "the client never tried to join again (${_forger}):\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "let go by the server: refused BAD_HANDSHAKE after ([0-9.]+) s of nothing; joining again\n")
    message(FATAL_ERROR "the client did not say it believed a refusal:\n${_out}\n${_err}")
endif()
set(_quiet "${CMAKE_MATCH_1}")
if(_quiet LESS 3)
    message(FATAL_ERROR "the client believed a refusal after ${_quiet} s of nothing, "
                        "under the three it must wait:\n${_out}")
endif()
if(NOT _out MATCHES "glideslope: the old session answered; staying in it\n")
    message(FATAL_ERROR "the client did not go back to its old session:\n${_out}\n${_err}")
endif()
if(_out MATCHES "glideslope: joined again")
    message(FATAL_ERROR "the client joined a new session rather than going back:\n"
                        "${_out}\n${_err}")
endif()
if(NOT EXISTS "${_shot}")
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "the shot drawn [0-9.]+ s past its tick; gone back to its old session, and flown by an input sent since")
    message(FATAL_ERROR "the shot was drawn before the client had gone back and been "
                        "flown:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "glideslope: flying aircraft [0-9]+, the c172p; the server says the pilot has it")
    message(FATAL_ERROR "the server was not flying the client's aircraft by the pilot at "
                        "the shot:\n${_out}")
endif()

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
message(STATUS "refused by a forger after ${_quiet} s of nothing, the client with the "
               "window went back to its old session and was flown in it; admitted once")

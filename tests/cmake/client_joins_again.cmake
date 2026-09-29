# client_joins_again.cmake - the client with the window, let go by the server
# for going quiet past its --timeout, joins again by itself and flies its new
# aircraft; and, dropped by the server's operator, does not.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope>
#         -DKEEPER=<glideslope_cli> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -DIMPAIR=<glideslope_impair> [-DDROP=ON]
#         -P client_joins_again.cmake
#
# **What was wrong.** The command-line client joined again by itself (#45);
# the one a person flies did not. Let go for silence - a laptop shut, a
# debugger's breakpoint, a network gone for longer than --timeout - it went on
# sealing under keys the server had thrown away until it was restarted.
#
# **The situation is built, not hoped for.** A server with --timeout 3 and one
# AI Cessna, and the client with the window flying the aircraft it is given.
# Two seconds of flight in it stalls (`--stall-after 2`): it sends nothing and
# answers nothing, reading and throwing away what comes, until nothing has
# come for three seconds - the server's knocks have stopped, so it has let it
# go - and then knocks once a second until the server refuses a knock. That
# refusal is the let-go, and the client must notice it by itself, as any
# client does: a `BAD_HANDSHAKE` from the server's address after three
# seconds of nothing opening. **The shot waits on the events**, not the clock:
# it is held, up to a minute of the flight past its tick, until the client has
# joined again and the server has flown its new aircraft by an input sent
# since. **A command-line client keeps the server running** through the gap
# between the two sessions - the server stops when everybody who joined has
# gone (--until-empty), and in that gap nobody is in - staying until the
# window client's shot exists, however slow the machine.
#
# **What must hold**: the client said it was let go, joined again, was given
# an aircraft, and at the shot was flying it with the server applying its
# inputs; the server admitted it twice, let one session go for silence and
# one for its goodbye, and dropped no copy of an initiation.
#
# **With DROP**, the server's operator drops it once its input has been flown
# (`--drop-once-flown`, which the window's button takes too). The client must
# say it was dropped and is not joining again, exit 1, and be admitted once:
# the operator's drop is not a let-go.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/again.sqlite")
set(_shot "${WORK}/again.bmp")
set(_ready "${WORK}/ready.txt")
file(REMOVE "${_store}" "${_shot}" "${_ready}")

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
# The client reaches the server through a relay (glideslope_impair, at
# PORT + 1) that holds nothing and drops nothing: it is there to pass the
# server's standard output on to standard error, so that both programs are
# heard - the server's in _err, the client's in _out.
math(EXPR _relay "${PORT} + 1")
if(DROP)
    # The shot is a bound, not a wait: dropped, it stops long before; not
    # dropped, it draws its shot a minute of flight in and the test fails
    # saying so.
    execute_process(
        COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
                --data "${DATA}" --timeout 3 --store "${_store}" --drop-once-flown
        COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
                --seed 1 --until-input-ends --seconds 290
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
                --shot "${_shot}" --view cockpit --shot-at 7200
                --server 127.0.0.1 ${_relay} --server-key ${_key}
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
else()
    # The keeper's key's public half begins f661fe1f (see server_slots.cmake).
    execute_process(
        COMMAND "${KEEPER}" connect "127.0.0.1:${PORT}" "${_key}" 300
                --after-ready "${_ready}" --until-exists "${_shot}"
                --key e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26
        COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
                --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
        COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
                --seed 1 --until-input-ends --seconds 290
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
                --shot "${_shot}" --view cockpit --shot-at 1200 --stall-after 2
                --server 127.0.0.1 ${_relay} --server-key ${_key}
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
endif()
list(LENGTH _rcs _programs)
math(EXPR _last "${_programs} - 1")
list(GET _rcs ${_last} _rc)

# **A machine that cannot draw skips, before anything is judged**: the
# client is given its aircraft before it opens the GPU, so the other checks
# would fail a run that never had a device (macOS has no Vulkan). The
# client's own words only: the server's and the relay's are on standard
# error too, and must not turn a failure into a skip.
if(_err MATCHES "glideslope: no GPU device")
    message(STATUS "the client cannot draw here: ${_err}")
    cmake_language(EXIT 77)
endif()
if(NOT _out MATCHES "the server gave this client aircraft [0-9]+")
    message(FATAL_ERROR "the client was given no aircraft:\n${_out}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

# The server's lines come on standard error here, piped as they are.
# The keeper's are not counted.
string(REGEX MATCHALL "admitted [0-9a-f]+ to slot" _admitted "${_err}")
list(FILTER _admitted EXCLUDE REGEX "f661fe1f")
list(LENGTH _admitted _admissions)
if(_err MATCHES "dropped a copy of an initiation")
    message(FATAL_ERROR "joining again was taken for a copy of the first initiation:\n"
                        "${_err}")
endif()

if(DROP)
    if(NOT _err MATCHES "dropped 127\\.0\\.0\\.1:[0-9]+ by the operator")
        message(FATAL_ERROR "the server dropped nobody:\n${_out}\n${_err}")
    endif()
    if(NOT _out MATCHES "glideslope: the server ended this session \\(dropped, or taken over by a newer session for this key\\); not joining again\n")
        message(FATAL_ERROR "the dropped client did not say it was dropped:\n${_out}\n${_err}")
    endif()
    if(_out MATCHES "; joining again\n|joined again")
        message(FATAL_ERROR "the dropped client tried to join again:\n${_out}")
    endif()
    if(NOT _rc EQUAL 1)
        message(FATAL_ERROR "the dropped client exited ${_rc}, not 1:\n${_out}\n${_err}")
    endif()
    if(NOT _admissions EQUAL 1)
        message(FATAL_ERROR "the dropped client was admitted ${_admissions} times, not once:\n"
                            "${_err}")
    endif()
    message(STATUS "the client with the window, dropped by the operator, said so, stopped "
                   "and did not come back")
    return()
endif()

if(NOT EXISTS "${_shot}")
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "glideslope: stalling, sending and answering nothing")
    message(FATAL_ERROR "the client never stalled:\n${_out}")
endif()
if(NOT _out MATCHES "let go by the server: refused BAD_HANDSHAKE after ([0-9.]+) s of nothing; joining again\n")
    message(FATAL_ERROR "the client did not notice it was let go:\n${_out}\n${_err}")
endif()
set(_quiet "${CMAKE_MATCH_1}")
if(_quiet LESS 3)
    message(FATAL_ERROR "the client believed a refusal after ${_quiet} s of nothing, "
                        "under the three it must wait:\n${_out}")
endif()
if(NOT _out MATCHES "glideslope: joined again: the server gave this client aircraft ([0-9]+), the c172p\n")
    message(FATAL_ERROR "the client did not join again:\n${_out}\n${_err}")
endif()
set(_again "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "the shot drawn [0-9.]+ s past its tick; joined again, and flown by an input sent since")
    message(FATAL_ERROR "the shot was drawn before the client had joined again and "
                        "flown:\n${_out}")
endif()
if(NOT _out MATCHES "glideslope: flying aircraft ${_again}, the c172p; the server says the pilot has it, and has flown it by inputs sent since it joined again")
    message(FATAL_ERROR "the server was not flying the client's new aircraft by its "
                        "inputs at the shot:\n${_out}")
endif()
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the client was admitted ${_admissions} times, not twice - once, "
                        "and again after it was let go:\n${_err}")
endif()
string(REGEX MATCHALL "let go [0-9.:]+ after [0-9.]+ s of silence" _silence "${_err}")
list(LENGTH _silence _silences)
if(NOT _silences EQUAL 1)
    message(FATAL_ERROR "the server let ${_silences} sessions go for silence, not one - "
                        "the stall:\n${_err}\n${_out}")
endif()
string(REGEX MATCHALL "let go [0-9.:]+ after it said it was leaving" _goodbyes "${_err}")
list(LENGTH _goodbyes _goodbye_count)
if(NOT _goodbye_count EQUAL 2)
    message(FATAL_ERROR "the server let ${_goodbye_count} sessions go for a goodbye, not "
                        "two - the second session's and the keeper's:\n${_err}")
endif()
message(STATUS "let go after ${_quiet} s of nothing, the client with the window joined "
               "again by itself and flew its new aircraft ${_again} by its inputs")

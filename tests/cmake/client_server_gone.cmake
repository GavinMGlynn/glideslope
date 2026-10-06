# client_server_gone.cmake - the client with the window, its session ended
# by the server without a word: dropped with every goodbye lost, it finds out
# by trying and stops; its server started again under it, it joins the new
# one and flies.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope>
#         -DKEEPER=<glideslope_cli> -DDATA=<data> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DDRIVER=<gpu driver>
#         -DMODE=lost|restart -P client_server_gone.cmake
#
# The server runs in server_started_again.cmake, which says what it said on
# standard error once it has gone.
#
# **lost**: the server's operator drops the client once its input has been
# flown, and every copy of the server's goodbye is lost (`--lose-goodbyes`:
# none is sent). Told nothing, the client hears nothing; three seconds on it
# believes the server's refusal of what it sends, as any let-go is believed,
# and tries to join again - and must be refused `DROPPED`, say it was dropped
# and is not joining again, never say it joined again, exit 1, and be
# admitted once. **A keeper holds the server open** meanwhile: with nobody
# else in, the drop would leave the server empty and it would stop
# (`--until-empty`), and the attempt to join again would go unanswered. The
# keeper stays until the client has gone (its `--done` file).
#
# **restart**: the server is started again under the client with its key
# from `--store`: the first stops, telling nobody, once it has flown the
# client's input, and the second starts when the first has gone. The client
# must notice it was let go, join the new server, and at the shot
# (`--shot-once-joined-again`, which waits on those events) be flying the
# aircraft the new server gave it by its inputs - on a clock started again
# from nought. Admitted twice, once by each server.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/gone.sqlite")
set(_shot "${WORK}/gone.bmp")
set(_ready "${WORK}/ready.txt")
set(_gone "${WORK}/client_gone.txt")
file(REMOVE "${_store}" "${_shot}" "${_ready}" "${_gone}")

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
if(MODE STREQUAL "lost")
    # The shot is a bound, not a wait: dropped, the client stops long before.
    # The keeper's key's public half begins f661fe1f (see server_slots.cmake).
    execute_process(
        COMMAND "${KEEPER}" connect "127.0.0.1:${PORT}" "${_key}" 300
                --after-ready "${_ready}" --until-exists "${_gone}"
                --key e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26
        COMMAND "${CMAKE_COMMAND}" -DSERVER=${SERVER} -DPORT=${PORT} -DDATA=${DATA}
                -DSTORE=${_store} -DREADY=${_ready} -DMODE=lost
                -P "${CMAKE_CURRENT_LIST_DIR}/server_started_again.cmake"
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
                --shot "${_shot}" --view cockpit --shot-at 7200 --done "${_gone}"
                --after-ready "${_ready}" --server 127.0.0.1 ${PORT} --server-key ${_key}
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
elseif(MODE STREQUAL "restart")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DSERVER=${SERVER} -DPORT=${PORT} -DDATA=${DATA}
                -DSTORE=${_store} -DREADY=${_ready} -DMODE=restart
                -P "${CMAKE_CURRENT_LIST_DIR}/server_started_again.cmake"
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
                --shot "${_shot}" --view cockpit --shot-at 600 --shot-once-joined-again
                --after-ready "${_ready}" --server 127.0.0.1 ${PORT} --server-key ${_key}
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
else()
    message(FATAL_ERROR "client_server_gone.cmake: MODE is lost or restart, not '${MODE}'")
endif()
list(LENGTH _rcs _programs)
math(EXPR _last "${_programs} - 1")
list(GET _rcs ${_last} _rc)

# **A machine that cannot draw skips, before anything is judged** - by the
# client's own words, not the server's on the same stream.
if(_err MATCHES "glideslope: no GPU device")
    message(STATUS "the client cannot draw here: ${_err}")
    cmake_language(EXIT 77)
endif()
if(NOT _out MATCHES "the server gave this client aircraft [0-9]+")
    message(FATAL_ERROR "the client was given no aircraft:\n${_out}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

# The servers' lines come on standard error. The keeper's are not counted.
string(REGEX MATCHALL "admitted [0-9a-f]+ to slot" _admitted "${_err}")
list(FILTER _admitted EXCLUDE REGEX "f661fe1f")
list(LENGTH _admitted _admissions)
if(NOT _out MATCHES "let go by the server: refused BAD_HANDSHAKE after ([0-9.]+) s of nothing; joining again\n")
    message(FATAL_ERROR "the client did not find out its session had ended, and try to join "
                        "again:\n${_out}\n${_err}")
endif()
set(_quiet "${CMAKE_MATCH_1}")
if(_quiet LESS 3)
    message(FATAL_ERROR "the client believed a refusal after ${_quiet} s of nothing, "
                        "under the three it must wait:\n${_out}")
endif()

if(MODE STREQUAL "lost")
    if(NOT _err MATCHES "dropped 127\\.0\\.0\\.1:[0-9]+ by the operator")
        message(FATAL_ERROR "the server dropped nobody:\n${_out}\n${_err}")
    endif()
    if(NOT _err MATCHES "refused [0-9a-f]+ from 127\\.0\\.0\\.1:[0-9]+: dropped by the operator")
        message(FATAL_ERROR "the server did not refuse the dropped client joining again:\n"
                            "${_err}")
    endif()
    if(NOT _out MATCHES "glideslope: the server ended this session \\(dropped, or taken over by a newer session for this key\\); not joining again\n")
        message(FATAL_ERROR "the client, refused joining again, did not say it was "
                            "dropped:\n${_out}\n${_err}")
    endif()
    if(_out MATCHES "joined again")
        message(FATAL_ERROR "the dropped client joined again:\n${_out}")
    endif()
    if(NOT _rc EQUAL 1)
        message(FATAL_ERROR "the dropped client exited ${_rc}, not 1:\n${_out}\n${_err}")
    endif()
    if(NOT _admissions EQUAL 1)
        message(FATAL_ERROR "the dropped client was admitted ${_admissions} times, not once:\n"
                            "${_err}")
    endif()
    message(STATUS "dropped with every goodbye lost, the client with the window found out "
                   "after ${_quiet} s, tried to join again, was refused, said it was dropped "
                   "and stopped")
    return()
endif()

if(NOT EXISTS "${_shot}")
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()
if(NOT _err MATCHES "stopped once a player was flown, telling nobody")
    message(FATAL_ERROR "the first server did not stop under the client:\n${_err}")
endif()
if(NOT _out MATCHES "glideslope: joined again: the server gave this client aircraft ([0-9]+), the c172p\n")
    message(FATAL_ERROR "the client did not join the server started again:\n${_out}\n${_err}")
endif()
set(_again "${CMAKE_MATCH_1}")
# **On a clock started again**: the new server's word that gave it the
# aircraft is earlier on its clock than the old one's last - so it was taken
# though older than every word the old session had, as it must be.
if(NOT _out MATCHES "joined again at ([0-9.]+) s on the server's clock, the old session's newest word at ([0-9.]+) s")
    message(FATAL_ERROR "the client did not say on what clock it joined again:\n${_out}")
endif()
set(_new_s "${CMAKE_MATCH_1}")
set(_old_s "${CMAKE_MATCH_2}")
if(NOT _new_s LESS _old_s)
    message(FATAL_ERROR "the client joined again at ${_new_s} s on the new server's clock, not "
                        "before the old one's ${_old_s} s: the restart was not under it, or "
                        "a clock started again was waited out:\n${_out}")
endif()
if(NOT _out MATCHES "the shot drawn [0-9.]+ s past its tick; joined again, and flown by an input sent since")
    message(FATAL_ERROR "the shot was drawn before the client had joined again and "
                        "flown:\n${_out}")
endif()
if(NOT _out MATCHES "glideslope: flying aircraft ${_again}, the c172p; the server says the pilot has it, and has flown it by inputs sent since it joined again")
    message(FATAL_ERROR "the server started again was not flying the client's new aircraft "
                        "by its inputs at the shot:\n${_out}")
endif()
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the client was admitted ${_admissions} times, not twice - once by "
                        "each server:\n${_err}")
endif()
message(STATUS "its server started again under it, the client with the window found out "
               "after ${_quiet} s, joined the new one and flew its aircraft ${_again} by its "
               "inputs")

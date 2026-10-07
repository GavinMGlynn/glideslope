# client_kept_while_away.cmake - the client with the window, away from its
# session for longer than the server waits for a client that says nothing -
# building its flight, or in a long pass of its frame loop - is not let go.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DIMPAIR=<glideslope_impair>
#         -DDATA=<data> -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -P client_kept_while_away.cmake
#
# **What was wrong.** The client polled its session once a pass of its frame
# loop, and while its flight was built only from a thread that stopped when
# the loop began. A debug build under load went silent to the server for more
# than five seconds just after it was given its aircraft, and was let go.
# Now client::Online keeps the session from a thread of its own whenever the
# frame loop has not polled it for a fifth of a second, wherever the loop is.
#
# **The situation is built, not hoped for.** A server that lets a client go
# after two seconds of silence (`--timeout 2`), and the client with the window
# kept from its session past that, in two cases, run one after the other:
#
#   - build: it stands still six seconds after joining, before it builds
#     its flight (`--slow-start 6`), as a slow machine building it does;
#   - pass: every pass of its frame loop is held three seconds longer,
#     a tenth of a second apart (`--slow-frames 3000`), as a machine whose
#     frames take longer than the server's timeout.
#
# **What must hold**, in each case: the client says how long it was away from its session
# at the longest, and that is past the server's two seconds - so the rule was
# tested, not passed by a machine too quick to need it; the server admitted
# it once, let nobody go for silence and it once for its goodbye; the client
# was never let go, flew the aircraft it was given, and the server says the
# pilot has it. **The shot is at a tick**, a count of simulated steps: ten
# seconds of flight, however long the machine takes to reach it. Both cases
# must have run.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/away.sqlite")
file(REMOVE "${_store}")

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
set(_timeout 2)
# **Both cases, one after the other**, on the same two ports: the block of
# fixed ports has no two more side by side to give a second test.
set(_cases build pass)
set(_run 0)
foreach(AWAY IN LISTS _cases)
    if(AWAY STREQUAL "build")
        set(_away --slow-start 6)
    else()
        set(_away --slow-frames 3000)
    endif()
    set(_shot "${WORK}/away-${AWAY}.bmp")
    file(REMOVE "${_shot}")
    # **The client connects once the server is flying** (client.cmake says
    # why): started at once, on a loaded Windows debug runner the client's
    # five-second handshake gave up before the server was listening -
    # "cannot reach", then "listening" (run 37596019954, twice).
    set(_ready "${WORK}/flying-${AWAY}")
    file(REMOVE "${_ready}")
    execute_process(
        COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
                --data "${DATA}" --timeout ${_timeout} --store "${_store}" --ready-file "${_ready}"
        COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
                --seed 1 --until-input-ends --seconds 290
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
                --shot "${_shot}" --view cockpit --shot-at 1200 ${_away} --after-ready "${_ready}"
                --server 127.0.0.1 ${_relay} --server-key ${_key}
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    list(GET _rcs -1 _rc)

    # **A machine that cannot draw skips, before anything is judged**: the
    # client's own words only.
    if(_err MATCHES "glideslope: no GPU device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    if(NOT _out MATCHES "the server gave this client aircraft [0-9]+")
        message(FATAL_ERROR "the client was given no aircraft:\n${_out}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")

    # **The rule was tested**: away longer than the server waits, where this
    # case puts it - building the flight, or flying it, before the shot (whose
    # own wait for its terrain is kept too, and is not what is tested here).
    if(NOT _out MATCHES "glideslope: built its flight; away from its session ([0-9.]+) s at the longest meanwhile")
        message(FATAL_ERROR "the client did not say how long it was away building its "
                            "flight:\n${_out}\n${_err}")
    endif()
    set(_building "${CMAKE_MATCH_1}")
    if(NOT _out MATCHES "glideslope: at the shot, away from its session ([0-9.]+) s at the longest since flying began")
        message(FATAL_ERROR "the client did not say how long it was away flying:\n${_out}\n${_err}")
    endif()
    set(_flying "${CMAKE_MATCH_1}")
    if(AWAY STREQUAL "build")
        set(_longest "${_building}")
    else()
        set(_longest "${_flying}")
    endif()
    if(_longest LESS_EQUAL ${_timeout})
        message(FATAL_ERROR "the client was away ${_longest} s at the longest (${AWAY}), not "
                            "past the server's ${_timeout} s: the test tested nothing:\n${_out}")
    endif()
    if(NOT _out MATCHES "glideslope: the session was kept for the frame loop ([0-9]+) times")
        message(FATAL_ERROR "the client did not say how often it was kept:\n${_out}\n${_err}")
    endif()
    set(_times "${CMAKE_MATCH_1}")

    string(REGEX MATCHALL "let go [0-9.:]+ after [0-9.]+ s of silence" _silence "${_err}")
    list(LENGTH _silence _silences)
    if(NOT _silences EQUAL 0)
        message(FATAL_ERROR "the server let ${_silences} sessions go for silence, not none, the "
                            "client away ${_longest} s at the longest:\n${_out}\n${_err}")
    endif()
    if(_out MATCHES "glideslope: let go by the server")
        message(FATAL_ERROR "the client was let go:\n${_out}\n${_err}")
    endif()
    string(REGEX MATCHALL "admitted [0-9a-f]+ to slot" _admitted "${_err}")
    list(LENGTH _admitted _admissions)
    if(NOT _admissions EQUAL 1)
        message(FATAL_ERROR "the server admitted ${_admissions} sessions, not one:\n${_err}")
    endif()
    string(REGEX MATCHALL "let go [0-9.:]+ after it said it was leaving" _goodbyes "${_err}")
    list(LENGTH _goodbyes _goodbye_count)
    if(NOT _goodbye_count EQUAL 1)
        message(FATAL_ERROR "the server let ${_goodbye_count} sessions go for a goodbye, not "
                            "one:\n${_err}")
    endif()
    if(NOT EXISTS "${_shot}")
        message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
    endif()
    if(NOT _out MATCHES "glideslope: flying aircraft [0-9]+, the c172p; the server says the pilot has it")
        message(FATAL_ERROR "the server was not flying the client's aircraft by the pilot at "
                            "the shot:\n${_out}")
    endif()
    message(STATUS "away from its session ${_longest} s at the longest (${AWAY}), past the "
                   "server's ${_timeout} s, the client with the window was kept in it "
                   "${_times} times and never let go")
    math(EXPR _run "${_run} + 1")
endforeach()
list(LENGTH _cases _count)
if(NOT _run EQUAL _count)
    message(FATAL_ERROR "ran ${_run} of the ${_count} cases")
endif()
message(STATUS "both ways of being away, ${_run} of ${_count} cases, kept in the session")

# prediction_let_go_of_a_nan.cmake - a predicting client whose prediction is
# given a NaN lets go of it and takes the server's state back, and flies on.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P prediction_let_go_of_a_nan.cmake
#
# **What is pinned.** A prediction checks every input it puts on JSBSim and
# the motion each step leaves, and the first not finite lets it go: its
# client takes the server's state back as on a fresh start - a new aircraft
# from the next word - rather than flying a NaN into JSBSim's tables, where
# a debug build asserts (FGTable.cpp:618; sim::Prediction::lost, and
# docs/status, 2026-10-23, for the CI run whose NaN was never explained).
#
# **The situation is built**: the client gives its prediction, and only its
# prediction, a NaN elevator on the first step at or after 10 s of its clock
# (`--test-nan-at 10`), through a relay delaying 100 ms each way, with the
# NaN trap set (`--trap-nan`), and stays until 500 updates have been compared.
#
# **What must hold**: every program exits 0 - nothing asserted, nothing
# trapped; the client said it let go of its prediction for the elevator's
# NaN, once; and it compared at least 240 updates since - ten seconds of the
# server's words - so it flew on, predicting, from the server's state.
#
# Seen red with the guard taken out of sim::Prediction: the NaN flown into
# JSBSim, the trap stopped the client and named the numbers gone with it.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/nan.sqlite")
set(_heard "${WORK}/heard.txt")
file(REMOVE "${_store}" "${_heard}")
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

math(EXPR _relay "${PORT} + 1")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 120 --after 1
            --predict --until-compared 500 --heard "${_heard}" --test-nan-at 10
            --trap-nan --track "${WORK}/track.txt"
    COMMAND "${SERVER}" --port ${PORT} --seconds 400 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 390
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(_err MATCHES "nan trap:")
    message(FATAL_ERROR "the NaN trap fired:\n${_err}")
endif()
string(REGEX MATCHALL "its prediction of aircraft [0-9]+ \\(c172p\\) went to a number not finite at its step [0-9]+ - the input's elevator - and is let go of" _lets "${_err}")
list(LENGTH _lets _said_let_go)
if(NOT _said_let_go EQUAL 1)
    message(FATAL_ERROR "the client said it let go of its prediction for the elevator's "
                        "NaN ${_said_let_go} times, not once:\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _said MATCHES "let go of: ([0-9]+) predictions gone to a number not finite, ([0-9]+) updates compared since the last")
    message(FATAL_ERROR "the client did not say what it let go of:\n${_said}")
endif()
set(_let_go "${CMAKE_MATCH_1}")
set(_since "${CMAKE_MATCH_2}")
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
set(_compared "${CMAKE_MATCH_1}")
set(_line "${_let_go} let go of, ${_since} of ${_compared} updates compared since")
if(NOT _let_go EQUAL 1)
    message(FATAL_ERROR "the prediction was let go of ${_let_go} times, not once: "
                        "${_line}:\n${_said}")
endif()
if(_since LESS 240)
    message(FATAL_ERROR "the client did not fly on from the server's state: ${_line}:\n${_said}")
endif()
message(STATUS "${_line}")

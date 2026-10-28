# client_let_go_of_a_nan.cmake - the client with the window, its prediction
# given a NaN, lets go of it, takes the server's state back on the aircraft
# it has, and flies on.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DDRIVER=<gpu driver>
#         -P client_let_go_of_a_nan.cmake
#
# **What is pinned.** The guard of sim::Prediction (Prediction::lost), in the
# window client's take-back path (client::Flight::let_go_if_lost): a
# prediction gone to a number not finite is let go of, and the server's
# motion taken back on the aircraft it has, rather than flown into JSBSim's
# tables, where a debug build asserts (FGTable.cpp:618; docs/status,
# 2026-10-23). glideslope_cli's own path, which builds a new aircraft, is
# prediction_let_go_of_a_nan.cmake.
#
# **The situation is built**: the client gives its prediction, and only its
# prediction, a NaN elevator at tick 960 (`--test-nan-at 960`, 8 s into its
# flight); and from tick 2400 hears the server's words on its own with a NaN
# in their motion for half a second (`--test-nan-words-at 2400`), a dozen
# words not finite in a row; through a relay delaying 100 ms each way,
# headless, and flies to its shot at tick 4200.
#
# **What must hold**: every program exits 0 - nothing asserted; the client
# said, once each, that it let go of its prediction for the elevator's NaN
# and for the word's, taking the server's word on the aircraft it has; it let
# go of two predictions - counted once for each loss, not once for each word
# heard while the words were not finite; and it compared at least 240
# updates since the last - ten seconds of the server's words - so it flew
# on, predicting.
#
# Seen red with the guard taken out of sim::Prediction, and with the count
# taken for every word (client::Flight::let_go_if_lost).
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed; and a GPU to draw with, or it skips too.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/nan.sqlite")
set(_shot "${WORK}/nan.bmp")
set(_ready "${WORK}/flying")
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
math(EXPR _relay "${PORT} + 1")
execute_process(
    # Each writes down the pipe to the next; the window client's are read.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 30 --store "${_store}" --ready-file "${_ready}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 4200 --view behind --after-ready "${_ready}"
            --test-nan-at 960 --test-nan-words-at 2400 --server 127.0.0.1 ${_relay} --server-key ${_key}
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT EXISTS "${_shot}")
    if(_err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
foreach(_what IN ITEMS "the input's elevator" "the server's word's uvw_mps\\[0\\]")
    string(REGEX MATCHALL "the prediction of the c172p went to a number not finite at its step [0-9]+ - ${_what} - and is let go of, and started again from the server's word on the aircraft it has" _lets "${_err}")
    list(LENGTH _lets _said)
    if(NOT _said EQUAL 1)
        message(FATAL_ERROR "the client said ${_said} times, not once, that it let go of "
                            "its prediction for ${_what}:\n${_err}\n${_out}")
    endif()
endforeach()
if(NOT _out MATCHES "let go of: ([0-9]+) predictions gone to a number not finite, ([0-9]+) updates compared since the last")
    message(FATAL_ERROR "the client did not say what it let go of:\n${_out}")
endif()
set(_let_go "${CMAKE_MATCH_1}")
set(_since "${CMAKE_MATCH_2}")
set(_line "${_let_go} let go of, ${_since} updates compared since")
if(NOT _let_go EQUAL 2)
    message(FATAL_ERROR "the prediction was let go of ${_let_go} times, not twice: "
                        "${_line}:\n${_out}")
endif()
if(_since LESS 240)
    message(FATAL_ERROR "the client did not fly on from the server's state: ${_line}:\n${_out}")
endif()
message(STATUS "${_line}")

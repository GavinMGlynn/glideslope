# starved_prediction.cmake - a client too slow for its own prediction puts
# it right once per pass, from the newest word, and keeps up.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DSLOW_US=<microseconds>
#         -P starved_prediction.cmake
#
# **What is pinned.** A client reads its socket dry before it draws a frame,
# and each of the server's words about its own aircraft puts the prediction
# right: back to the word, and every step since flown again. Put right from
# every word read in a pass, a client that could not fly those steps as fast
# as the words came read them ever later, its steps held to put right grew to
# their bound, and it never caught up: CI's Ubuntu debug, one frame of
# 450.9 s, 42 km corrections (#232, 2026-10-23). Each word older than the
# newest read with it is superseded: it is still held against where this
# client had flown, and tells the clocks' difference, and only the newest
# puts the prediction right (sim::Prediction::hear_clock).
#
# **The situation is built**: the client spends `SLOW_US` microseconds busy
# on every step its prediction flies, forward or again (`--slow-steps`) - a
# machine too slow for its own prediction, on any machine, and its own time,
# so never excused as the machine's stall. Through a relay that delays
# everything 100 ms each way, it stays until 400 updates are compared
# (`--until-compared 400`), the event, not a time.
#
# **What must hold**: no pass put right from more than one word, so the
# replay's work a pass is bounded by sim::most_unacknowledged; never more
# than a second's words (25) read between two times the socket ran dry; the 400
# updates compared within 120 s of the client's stay - 16 s of the server's
# updates at 25 a second, with the client's slowness to spare; and the
# prediction error as a client on a server is held to elsewhere
# (server_paced_prediction.cmake): under a metre for 95 in a hundred, 3 m at
# worst, and the corrections under 20 m (sim::snap_beyond_m), none snapped.
#
# Seen red with every word putting it right (docs/status, 2026-10-23).
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/starved.sqlite")
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
string(TIMESTAMP _began "%s")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 120 --after 1
            --predict --until-compared 400 --heard "${_heard}" --slow-steps ${SLOW_US}
            --track "${WORK}/track.txt"
    COMMAND "${SERVER}" --port ${PORT} --seconds 400 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 390
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
string(TIMESTAMP _ended "%s")
math(EXPR _took "${_ended} - ${_began}")
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _said MATCHES "reconciled: the socket read dry ([0-9]+) times, at most ([0-9]+) words read, ([0-9]+) put right from and ([0-9]+) steps flown again between two, ([0-9]+) steps flown again in all; ([0-9]+) words superseded")
    message(FATAL_ERROR "the client did not say how it put its prediction right:\n${_said}")
endif()
set(_dry "${CMAKE_MATCH_1}")
set(_most_read "${CMAKE_MATCH_2}")
set(_most_words "${CMAKE_MATCH_3}")
set(_most_steps "${CMAKE_MATCH_4}")
set(_all_steps "${CMAKE_MATCH_5}")
set(_superseded "${CMAKE_MATCH_6}")
if(NOT _said MATCHES "predicted: ([0-9]+) corrections, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m, ([0-9]+) too large to hide")
    message(FATAL_ERROR "the client did not say its corrections:\n${_said}")
endif()
set(_corrections "${CMAKE_MATCH_1}")
math(EXPR _worst_correction_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
set(_snapped "${CMAKE_MATCH_4}")
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
set(_compared "${CMAKE_MATCH_1}")
math(EXPR _worst_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
set(_p95_mm -1)
if(_said MATCHES "prediction error 95th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    math(EXPR _p95_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
endif()
string(CONCAT _line "at ${SLOW_US} us a step: ${_compared} updates compared in ${_took} s, "
          "the worst error ${_worst_mm} mm and the 95th percentile ${_p95_mm} mm; "
          "${_corrections} corrections, the worst ${_worst_correction_mm} mm, ${_snapped} "
          "snapped; the socket read dry ${_dry} times, at most ${_most_read} words read, "
          "${_most_words} put right from and ${_most_steps} steps flown again between two, "
          "${_all_steps} in all, ${_superseded} words superseded")
include("${CMAKE_CURRENT_LIST_DIR}/machine_stalls_check.cmake")
check_machine_stalls("${_said}" updates "${_compared}" _stalls)
string(APPEND _line "; ${_stalls}")
if(_most_words GREATER 1)
    message(FATAL_ERROR "a pass put the prediction right from more than one word: "
                        "${_line}:\n${_said}")
endif()
# **Caught up: never more than a second of words waiting** - 25 - between two
# times the socket was read dry.
if(_most_read GREATER 25)
    message(FATAL_ERROR "the starved client fell behind its words: ${_line}:\n${_said}")
endif()
if(_compared LESS 400 OR _took GREATER 120)
    message(FATAL_ERROR "the starved client did not keep up: ${_line}:\n${_said}")
endif()
if(_p95_mm LESS 0 OR _p95_mm GREATER 1000 OR _worst_mm GREATER 3000 OR
   _worst_correction_mm GREATER 20000 OR NOT _snapped EQUAL 0)
    message(FATAL_ERROR "the starved client's prediction was off: ${_line}:\n${_said}")
endif()
message(STATUS "${_line}")

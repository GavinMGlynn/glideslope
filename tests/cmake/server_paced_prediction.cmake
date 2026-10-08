# server_paced_prediction.cmake - a client predicting against a server behind
# real time flies at the server's pace and is off by centimetres, not metres.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> [-DUNPACED=ON]
#         -P server_paced_prediction.cmake
#
# **What is pinned.** The server flies each input from when it arrives until
# the next does. One behind real time flies fewer steps of each than a client
# flying at its own machine's pace, and the two part by metres. The client
# steers its pace by the clocks' difference its prediction measures, holding
# it where it was first known (sim::Pacing), and so flies each input for as
# many steps as the server does.
#
# **The situation is built**: the server's clock runs at a set 0.6 of real
# time (`--test-pace 0.6`) - the same on every machine that can keep up with
# that, where one slowed by sleeping in its steps is as slow as the runner.
# The client predicts through a relay that delays everything 100 ms each way,
# flying changing controls, and stays until 600 updates have been compared
# (`--until-compared 600`) - 40 s of the server's updates at 15 a second - the
# event, not a time.
#
# **What must hold**: the client flew as many steps as the server, within
# 1 percent, between the first update compared and the last, and its prediction error was under a metre for 95 updates in a
# hundred, under 3 m at worst, and its median under 5 cm. `-DUNPACED=ON` flies
# the client at its own pace (`--unpaced`): that is what fails, by metres.
#
# **A named limit: on Windows about one update in a hundred is off by two
# steps' travel.** A step's travel is 0.42 m at this aircraft's speed, and
# the clocks' difference - the least of the last fifty words' - is known to a
# step. Windows' sleeps are 15.6 ms, not the millisecond the server, relay
# and client ask for, so each word carries up to two steps of spread; as a
# word that came through quickest enters the fifty, or leaves it, the least
# moves by two steps, and the server's word is placed two steps off - 1.1 m
# on the update that does it, and the correction a player would see is that
# far, hidden by the blend. Read from the client's track on Windows debug
# (2026-10-08): the 1.1 m updates fell exactly where the least moved, -20 to
# -22 and back, with centimetres between. On CI's Windows debug, 1 in 100
# such updates or more put the 99th percentile at 1.105 m (run 37723785657),
# so the metre bounds the 95th, the worst three metres - under seven steps'
# travel. Tried and taken out: steering the pace by each word's difference,
# smoothed, instead of the least of the newest ten - five in a hundred off
# by a step instead (PROJECT_STATUS.md).
#
# RUN_SERIAL: a runner busy beside it would put the server further behind
# than its set pace, unsteadily, which is not what this builds.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/paced.sqlite")
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

set(_pace 0.6)
set(_unpaced "")
if(UNPACED)
    set(_unpaced --unpaced)
endif()
math(EXPR _relay "${PORT} + 1")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 280 --after 1
            --predict --until-compared 600 --heard "${_heard}" ${_unpaced}
            --track "${WORK}/track.txt"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" --test-pace ${_pace}
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}\n${_err}")
endif()
set(_compared "${CMAKE_MATCH_1}")
math(EXPR _worst_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
if(NOT _said MATCHES "prediction error median: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its median error:\n${_said}")
endif()
math(EXPR _median_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "prediction error 95th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its 95th percentile:\n${_said}")
endif()
math(EXPR _p95_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "prediction error 99th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its 99th percentile:\n${_said}")
endif()
math(EXPR _p99_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "paced: [^\n]* at ([0-9]+)\\.([0-9][0-9][0-9]) of this machine's clock at the end; the clocks' difference (-?[0-9]+) steps from the one held then, (-?[0-9]+) at worst")
    message(FATAL_ERROR "the client did not say its pace:\n${_said}")
endif()
math(EXPR _pace_thousandths "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "paced in steps: ([0-9]+) flown here while the server flew ([0-9]+),")
    message(FATAL_ERROR "the client did not say the steps it flew:\n${_said}")
endif()
set(_steps_here "${CMAKE_MATCH_1}")
set(_steps_there "${CMAKE_MATCH_2}")
math(EXPR _steps_apart "${_steps_here} - ${_steps_there}")
if(_steps_apart LESS 0)
    math(EXPR _steps_apart "0 - ${_steps_apart}")
endif()
math(EXPR _steps_allowed "${_steps_there} / 100")
string(CONCAT _line "${_compared} updates compared, the worst error ${_worst_mm} mm, the "
          "95th and 99th percentiles ${_p95_mm} and ${_p99_mm} mm and the median "
          "${_median_mm} mm, flown at ${_pace_thousandths} thousandths of real time against a "
          "server at ${_pace} (${_steps_here} steps flown here to the server's "
          "${_steps_there})")
if(_compared LESS 600)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
if(_p95_mm GREATER 1000 OR _worst_mm GREATER 3000 OR _median_mm GREATER 50)
    message(FATAL_ERROR "against a server behind real time the prediction was off: "
                        "${_line}:\n${_said}")
endif()
# **The pace in steps, counted, not timed**: the pace at the end is a real
# time's, which Windows' 15.6 ms timers moved to 0.583 once on CI's Windows
# debug (run 37729451368) with the steps held; the steps flown here and on
# the server between the first update compared and the last are the same
# whatever the clocks do. Within 1 percent; unpaced they part by 40.
if(_steps_apart GREATER _steps_allowed)
    message(FATAL_ERROR "the client did not fly at the server's pace: ${_line}:\n${_said}")
endif()
message(STATUS "${_line}")

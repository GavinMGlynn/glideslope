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
# **What must hold**: the client said it flew at the server's pace, within
# 2 percent, and its prediction error was under a metre for 99 updates in a
# hundred, under 2 m at worst, and its median under 5 cm. `-DUNPACED=ON` flies
# the client at its own pace (`--unpaced`): that is what fails, by metres.
#
# **Why the 99th percentile and not the worst** for the metre: a step's
# travel is 0.42 m at this aircraft's speed, and the clocks' difference is
# known to a step, so the 99th percentile sits at about one step, 0.45 to
# 0.55 m on Linux and Windows alike. Windows' sleeps are 15.6 ms long, not the
# millisecond the server, relay and client ask for, which adds up to two
# steps of jitter to a relay told to add none: on CI's Windows debug and
# clang-cl, and here once in five on Windows, a single update of 600 was off
# by 1.1 m, the 99th percentile still 0.55 m. The worst is bounded at 2 m,
# under five steps' travel.
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
if(NOT _said MATCHES "prediction error 99th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its 99th percentile:\n${_said}")
endif()
math(EXPR _p99_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "paced: [^\n]* at ([0-9]+)\\.([0-9][0-9][0-9]) of this machine's clock at the end; the clocks' difference (-?[0-9]+) steps from the one held then, (-?[0-9]+) at worst")
    message(FATAL_ERROR "the client did not say its pace:\n${_said}")
endif()
math(EXPR _pace_thousandths "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
string(CONCAT _line "${_compared} updates compared, the worst error ${_worst_mm} mm, the "
          "99th percentile ${_p99_mm} mm and the median "
          "${_median_mm} mm, flown at ${_pace_thousandths} thousandths of real time against a "
          "server at ${_pace}; the clocks' difference ${CMAKE_MATCH_4} steps off at worst")
if(_compared LESS 600)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
if(_p99_mm GREATER 1000 OR _worst_mm GREATER 2000 OR _median_mm GREATER 50)
    message(FATAL_ERROR "against a server behind real time the prediction was off: "
                        "${_line}:\n${_said}")
endif()
if(_pace_thousandths LESS 588 OR _pace_thousandths GREATER 612)
    message(FATAL_ERROR "the client did not fly at the server's pace: ${_line}:\n${_said}")
endif()
message(STATUS "${_line}")

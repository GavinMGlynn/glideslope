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
# 2 percent, and its prediction error was under a metre at worst and its
# median under 5 cm. `-DUNPACED=ON` flies the client at its own pace
# (`--unpaced`): that is what fails, by metres.
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
if(NOT _said MATCHES "paced: [^\n]* at ([0-9]+)\\.([0-9][0-9][0-9]) of this machine's clock at the end; the clocks' difference (-?[0-9]+) steps from the one held then, (-?[0-9]+) at worst")
    message(FATAL_ERROR "the client did not say its pace:\n${_said}")
endif()
math(EXPR _pace_thousandths "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
string(CONCAT _line "${_compared} updates compared, the worst error ${_worst_mm} mm and the median "
          "${_median_mm} mm, flown at ${_pace_thousandths} thousandths of real time against a "
          "server at ${_pace}; the clocks' difference ${CMAKE_MATCH_4} steps off at worst")
if(_compared LESS 600)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
if(_worst_mm GREATER 1000 OR _median_mm GREATER 50)
    message(FATAL_ERROR "against a server behind real time the prediction was off: "
                        "${_line}:\n${_said}")
endif()
if(_pace_thousandths LESS 588 OR _pace_thousandths GREATER 612)
    message(FATAL_ERROR "the client did not fly at the server's pace: ${_line}:\n${_said}")
endif()
message(STATUS "${_line}")

# server_impaired.cmake - prediction, interpolation and the player limit,
# through a network made worse on purpose.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DCHECK=<glideslope_interpolation_check>
#         -DDATA=<data dir> -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDELAY=<ms> -DJITTER=<ms> -DLOSS=<percent> -DSEED=<n> -DPREDICT_M=<m>
#         -P server_impaired.cmake
#
# **Built, not hoped for.** A server with one AI aircraft and room for two
# players. Three clients:
#
# - **one that hears everything**, straight to the server, which writes down
#   every update it hears - the truth the others are judged against;
# - **one that predicts**, through `glideslope_impair` (DELAY ms each way, up
#   to JITTER more, LOSS% of datagrams lost, drawn from SEED), flying a pilot
#   whose controls never stop changing, predicting its own aircraft and
#   drawing every other one 100 ms behind, as a real client does;
# - **one too many**, through the same relay, after the other two are in: the
#   server has room for two, so it must be refused as full - through loss and
#   latency, not only on a clean line.
#
# **The bounds.** The predicting client's prediction error - where the server
# said its aircraft was on an input, against where the client had flown it to
# on that input - must stay under PREDICT_M. The server applies an input when
# it arrives, so an input made late by jitter is flown late: the error is about
# the aeroplane's speed times the jitter and an input's length (1/30 s) - at
# 50 m/s, 5 m for 60 ms. The bounds given are that with room for a slow
# runner. The inputs flown while it joins, before the server has applied any
# of them, are not compared, and are counted.
#
# Its corrections - how far its own
# aircraft moved each time the server's word put it right - must none of them
# be too large to hide - 20 m, the bound the design states (sim/prediction.hpp).
# They are metres, not the millimetres the in-process test finds, because the
# server does not say how far into its latest input it had flown: the client
# replays whole inputs, and a step at 55 m/s is half a metre.
# Every other aircraft it drew must be within 2 m of where the server said it
# was at that moment: the bound test_interpolation.cpp holds the interpolator
# to in-process, now over a network. And the relay must have lost datagrams
# both ways, or the loss was not tested.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/impaired.sqlite")
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

math(EXPR _relay "${PORT} + 1")
set(_truth "${WORK}/truth.txt")
set(_shown "${WORK}/shown.txt")
set(_predicting "${WORK}/predicting.txt")
set(_extra "${WORK}/one-too-many.txt")
file(REMOVE "${_truth}" "${_shown}" "${_predicting}" "${_extra}")
execute_process(
    # Each program's standard output goes down the pipe to the next, so each
    # comes before one that outlives it: one that wrote to a pipe whose reader
    # had gone was killed for it.
    #
    # Asks after the other two are in: four seconds of resending at a quarter
    # of a second would have to be lost for it to overtake the one that
    # predicts.
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 5 --after 6
            --heard "${_extra}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 20 --after 2
            --predict c172p --heard "${_predicting}" --track "${_shown}"
    # Hears everything, and outlasts the one that predicts.
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 30 --after 1
            --track "${_truth}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 2 --data "${DATA}" --timeout 3 --store "${_store}"
    # Last, so that what it says about what it did is what is read; and it
    # stops when the server does.
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay ${DELAY} --jitter ${JITTER}
            --loss ${LOSS} --seed ${SEED} --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "exit codes: ${_rcs}\n${_out}")
# The one too many is refused (1); every other program finished as it should.
if(NOT _rcs STREQUAL "1;0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}, not 1;0;0;0;0:\n${_out}\n${_err}")
endif()

# **The relay made it worse**, both ways.
if(NOT _out MATCHES "forwarded ([0-9]+) to the server and ([0-9]+) back; dropped ([0-9]+) and ([0-9]+)")
    message(FATAL_ERROR "the relay did not say what it did:\n${_out}\n${_err}")
endif()
set(_up "${CMAKE_MATCH_1}")
set(_down "${CMAKE_MATCH_2}")
set(_lost_up "${CMAKE_MATCH_3}")
set(_lost_down "${CMAKE_MATCH_4}")
if(_up LESS 100 OR _down LESS 100)
    message(FATAL_ERROR "the relay carried ${_up} and ${_down} datagrams: hardly a session")
endif()
if(LOSS GREATER 0 AND (_lost_up EQUAL 0 OR _lost_down EQUAL 0))
    message(FATAL_ERROR "the relay lost ${_lost_up} and ${_lost_down}: loss was not tested both ways")
endif()

# **Prediction**: corrections, none too large to hide.
if(NOT EXISTS "${_predicting}")
    message(FATAL_ERROR "the predicting client wrote nothing:\n${_err}")
endif()
file(READ "${_predicting}" _said)
if(NOT _said MATCHES "predicted: ([0-9]+) corrections, the worst ([0-9.]+) m, ([0-9]+) too large to hide")
    message(FATAL_ERROR "the predicting client did not say how it predicted:\n${_said}\n${_err}")
endif()
set(_corrections "${CMAKE_MATCH_1}")
set(_worst "${CMAKE_MATCH_2}")
set(_snapped "${CMAKE_MATCH_3}")
if(_corrections LESS 200)
    message(FATAL_ERROR "only ${_corrections} corrections: its own aircraft was hardly flown")
endif()
if(NOT _snapped EQUAL 0)
    message(FATAL_ERROR "${_snapped} corrections were too large to hide:\n${_said}")
endif()
if(_worst GREATER_EQUAL 20)
    message(FATAL_ERROR "the worst correction was ${_worst} m, the bound 20 m:\n${_said}")
endif()

if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9.]+) m \\(([0-9]+) inputs")
    message(FATAL_ERROR "the predicting client did not say its prediction error:\n${_said}")
endif()
set(_compared "${CMAKE_MATCH_1}")
set(_error "${CMAKE_MATCH_2}")
set(_joining "${CMAKE_MATCH_3}")
if(_compared LESS 150)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
# Joining takes a round trip and a little: a second of inputs is more than it
# should ever be.
if(_joining GREATER 30)
    message(FATAL_ERROR "${_joining} inputs were flown before the server applied one:\n${_said}")
endif()
if(_error GREATER_EQUAL PREDICT_M)
    message(FATAL_ERROR "the worst prediction error was ${_error} m, the bound ${PREDICT_M} m:\n${_said}")
endif()

# **Interpolation**: every other aircraft drawn within 2 m of the truth.
execute_process(COMMAND "${CHECK}" "${_truth}" "${_shown}" 2
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _judged)
message(STATUS "${_judged}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "interpolation out of bound:\n${_judged}")
endif()
if(NOT _judged MATCHES "interpolation: ([0-9]+) of")
    message(FATAL_ERROR "the check did not say how many it judged:\n${_judged}")
endif()
# Two others - the AI and the client that hears everything - drawn for most of
# twenty seconds. At 60 frames a second that is over 2,000; a Windows client,
# whose shortest sleep is longer, drew 1,492. The floor is what says the run
# hardly happened - thirty frames a second for ten seconds - and the check
# itself holds the share judged to 95% of those drawn.
if(CMAKE_MATCH_1 LESS 600)
    message(FATAL_ERROR "only ${CMAKE_MATCH_1} frames were judged:\n${_judged}")
endif()

# **The player limit**: the third was refused as full (TRANSPORT.md, 05).
if(NOT EXISTS "${_extra}")
    message(FATAL_ERROR "the client one too many wrote nothing - it was never answered:\n${_err}")
endif()
file(READ "${_extra}" _refused)
if(NOT _refused MATCHES "refused, reason 5\n")
    message(FATAL_ERROR "the client one too many was not refused as full:\n${_refused}")
endif()
message(STATUS "through ${DELAY} ms, ${JITTER} ms of jitter and ${LOSS}% loss: "
               "prediction error at worst ${_error} m over ${_compared} updates; ${_corrections} "
               "corrections, the worst ${_worst} m; the third refused as full")

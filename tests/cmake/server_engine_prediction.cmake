# server_engine_prediction.cmake - a client predicting its own aircraft is
# put right no more after its engine stops on the server than before.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P server_engine_prediction.cmake
#
# **What is pinned.** A client predicts its own aircraft with a flight model of
# its own, whose engines it flies on the same inputs as the server's. An
# engine the server stops - a failure, which the client's model does not
# have - was run on by the prediction, and the client put right by the thrust
# it did not have, correction by correction. Now the state update's
# `engine_stopped` stops the client's first engine before it flies its inputs
# again (sim::Prediction::hear_engine_stopped).
#
# **The situation is built**: the server stops every player's first engine 20
# simulated seconds in (`--fail-engine-at 20`); the client predicts through a
# relay that delays everything 250 ms each way, so that each correction flies
# half a second of inputs again, and flies changing controls with the
# throttle at 0.5 to 0.9. It stays until 300 updates have been compared since
# the server said the engine had stopped (`--until-engine-compared 300`) -
# the event, not a time.
#
# **What must hold**: the client stopped one engine for the server's word,
# compared 300 updates after, and its median error **in speed** after the
# stop is no more than before it - put right no more with it stopped than
# with it running - and under 5 cm a second. **In speed, not distance.** The distance is the server's
# word against where this client had flown it to at the step the clocks'
# difference places the word at, and on Windows, whose sleeps are 15.6 ms,
# that placing is a step off for seconds at a time (PROJECT_STATUS.md, the
# paced test's excursions): a step is 0.45 m at the Cessna's speed, and a
# median of 0.43 m after the stop against 0.08 m before failed on CI's
# Windows release (run 37759639784), 0.4 to 1.5 m on its debug and clang.
# Which half of the run the steps off fall in is chance. A step moves the
# speed by what the aeroplane gains in a 120th of a second - a centimetre a
# second - where an engine run on moves it by its thrust over the round
# trip. Measured on linux-debug: 0.018 m/s after against 0.135 before (the
# throttle's changes, flown a step or two apart, are most of before's);
# with the engine run on, 0.485 after - and the distance 0.183 m against
# 0.042. On the Windows copy (release), 0.011, 0.012 and 0.013 after against
# 0.127 to 0.135 before. The client keeps its track (`track.txt`): each
# comparison's distance and speed error, and the clocks' difference it was
# placed by.
#
# **And against a server behind real time** (`-DSLOWED=ON`): its clock at a
# set 0.6 of real time (`--test-pace 0.6`), as CI's Windows debug runners'
# fell behind when this failed there. The relative bound above did not
# catch that: flown at its own pace (`--unpaced`) the client's medians were
# 20.2 m after and 20.5 m before, and after was the less. So slowed, both
# medians must also be under 5 cm: paced, they were 4 and 23 mm (4 and 21
# with the server slowed by sleeping). `-DUNPACED=ON` flies the client
# unpaced, which must fail.
#
# `-DSERVER_EXTRA=...` adds to the server's arguments - `--test-step-ms 15`
# puts it behind real time, as a loaded runner does, to reproduce one.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/engine.sqlite")
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

set(_slowed)
if(SLOWED)
    set(_slowed --test-pace 0.6)
endif()
set(_unpaced)
if(UNPACED)
    set(_unpaced --unpaced)
endif()
math(EXPR _relay "${PORT} + 1")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 280 --after 1
            --predict --until-engine-compared 300 --heard "${_heard}" ${_unpaced}
            --track "${WORK}/track.txt"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" --fail-engine-at 20 ${_slowed}
            ${SERVER_EXTRA}
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _said MATCHES "engine stopped for the server's word \\(([0-9]+) stopped here\\): ([0-9]+) updates compared after, the worst error ([0-9.]+) m and correction ([0-9.]+) m; before, ([0-9.]+) m and ([0-9.]+) m")
    message(FATAL_ERROR "the client never heard its engine had stopped:\n${_said}\n${_err}")
endif()
set(_stopped_here "${CMAKE_MATCH_1}")
set(_compared "${CMAKE_MATCH_2}")
set(_after "${CMAKE_MATCH_3}")
set(_before "${CMAKE_MATCH_5}")
if(_compared LESS 300)
    message(FATAL_ERROR "only ${_compared} updates were compared after the engine stopped:\n"
                        "${_said}")
endif()
if(NOT _said MATCHES "engine stopped: the median error after ([0-9]+)\\.([0-9][0-9][0-9]) m, before ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its median errors:\n${_said}")
endif()
# In millimetres, as CMake's arithmetic has no fractions.
math(EXPR _after_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
math(EXPR _before_mm "${CMAKE_MATCH_3} * 1000 + ${CMAKE_MATCH_4}")
if(NOT _said MATCHES "engine stopped: the median speed error after ([0-9]+)\\.([0-9][0-9][0-9][0-9]) m/s, before ([0-9]+)\\.([0-9][0-9][0-9][0-9]) m/s")
    message(FATAL_ERROR "the client did not say its median speed errors:\n${_said}")
endif()
# In tenths of a millimetre a second.
math(EXPR _after_speed "${CMAKE_MATCH_1} * 10000 + ${CMAKE_MATCH_2}")
math(EXPR _before_speed "${CMAKE_MATCH_3} * 10000 + ${CMAKE_MATCH_4}")
# **And under 5 cm a second after**, whatever before was: 0.023 m/s the most
# measured with the engine stopped here (ten runs on the Windows copy, 0.015 to
# 0.018 on linux-debug, 0.009 slowed), twice that the bound; an engine run on
# was 0.487, and one run on for part of the flight is between. The distance's
# medians are not held: on Windows a step's misplacing, 0.45 m, is chance.
if(_after_speed GREATER 500)
    message(FATAL_ERROR "with the engine stopped the median speed error was "
                        "${_after_speed} tenths of a mm/s, not under 5 cm/s:\n${_said}")
endif()
if(_after_speed GREATER _before_speed)
    message(FATAL_ERROR "with the engine stopped the median speed error was "
                        "${_after_speed} tenths of a mm/s, more than the ${_before_speed} before "
                        "it:\n${_said}")
endif()
if(SLOWED AND (_after_mm GREATER 50 OR _before_mm GREATER 50))
    message(FATAL_ERROR "against a server at 0.6 of real time the median prediction "
                        "errors were ${_after_mm} mm after the stop and ${_before_mm} mm "
                        "before, not both under 5 cm:\n${_said}")
endif()
if(NOT _stopped_here EQUAL 1)
    message(FATAL_ERROR "the client stopped ${_stopped_here} engines for the server's word, "
                        "not one:\n${_said}")
endif()
message(STATUS "with its engine stopped, the median speed error was ${_after_speed} tenths "
               "of a mm/s over ${_compared} updates, against ${_before_speed} before; the "
               "median distance ${_after_mm} mm against ${_before_mm} mm (the worst ${_after} m "
               "and ${_before} m)")

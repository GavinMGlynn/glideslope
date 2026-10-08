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
# compared 300 updates after, and its median prediction error after the stop
# is no more than 2 cm over its median before - "as small as with it
# running". **Over, not the larger of**: on Windows both medians are 50 to
# 60 mm, a millimetre or four apart either way from run to run (five runs
# here, 2026-10-08, and CI's 47 against 43 mm), and the larger-of bound
# failed on that alone; the engine run on put the median after 0.2 m against
# 0.03. **The median, not the worst**: half a second at a time, every few
# seconds, the prediction is put right by a metre or more whether an engine
# has stopped or not (1.6 m in 40 s of this flight with none, 2026-10-06) -
# the clock's estimate, an open tail - and the worst is what that made it.
# With the engine run on, the median after was 0.2 m against 0.03 before.
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
math(EXPR _bound_mm "${_before_mm} + 20")
if(_after_mm GREATER _bound_mm)
    message(FATAL_ERROR "with the engine stopped the median prediction error was "
                        "${_after_mm} mm, more than 2 cm over the ${_before_mm} mm before "
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
message(STATUS "with its engine stopped, the median prediction error was ${_after_mm} mm "
               "over ${_compared} updates, against ${_before_mm} mm before (the worst "
               "${_after} m and ${_before} m)")

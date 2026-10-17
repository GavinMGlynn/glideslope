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
# compared 300 updates after, and its median error **in speed, where it
# was** after the stop is under 10 cm a second and no more than half again
# what it was before - put right no more with it stopped than with it
# running.
#
# **Where it was, not when.** Each server's word is held against the speed
# of the step this client had flown nearest to where the word puts her, not
# the step the clocks' difference places the word at. The latter depends on
# the pace: a server behind real time by fits and starts - a loaded
# Windows runner - moves the difference by tens of steps for seconds, and
# the word is held against a step that far from its own (PROJECT_STATUS.md,
# 2026-10-17). Every step off costs the speed's change in a 120th of a
# second, which a glide with its engine stopped makes far more than a
# cruise: on CI's windows-clang (run 38980963376, paced at 0.873, 95 steps
# off at worst) the median distance after the stop was 9.2 m against 0.04 m
# before, and the speed placed by the clocks 0.33 m/s - the 0.33 to 0.49 an
# engine run on gives. Nearest in place, a misplacing is taken out and an
# engine run on is not: she got there faster. Here, with the server stopped
# for 20 to 800 ms every 0.2 to 1.2 s (a test's SIGSTOP, on linux-debug):
# 0.0088 m/s where it was after, 15 steps placed off at the median, against
# 0.030 by the clocks and a median distance of 7.3 m; unstopped, 0.0054. An
# engine run on: 0.332 and 0.358 where it was, 0.379 stopped as above, 0.228
# slowed.
# **And at least 270 of the 300 placed**: a word whose nearest is either end
# of the two seconds and more kept is left out, as its nearest may lie
# beyond - 2 to 4 in a run stopped as above. The speed placed by the
# clocks' difference, and the distances, are reported, not held: they are
# the pace's. The client keeps its track (`track.txt`): each comparison's
# distance and speed error, and the clocks' difference it was placed by.
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
# **Where it was** (the header): the speed against the step flown nearest.
if(NOT _said MATCHES "engine stopped: the median speed error where it was after ([0-9]+)\\.([0-9][0-9][0-9][0-9]) m/s over ([0-9]+) updates, before ([0-9]+)\\.([0-9][0-9][0-9][0-9]) m/s over ([0-9]+); placed ([0-9]+) and ([0-9]+) steps from there at the median, ([0-9]+) nearest at an end")
    message(FATAL_ERROR "the client did not say its median speed errors where it was:\n${_said}")
endif()
math(EXPR _after_placed "${CMAKE_MATCH_1} * 10000 + ${CMAKE_MATCH_2}")
set(_placed_after "${CMAKE_MATCH_3}")
math(EXPR _before_placed "${CMAKE_MATCH_4} * 10000 + ${CMAKE_MATCH_5}")
set(_steps_off_after "${CMAKE_MATCH_7}")
set(_steps_off_before "${CMAKE_MATCH_8}")
set(_at_an_end "${CMAKE_MATCH_9}")
# **At least nine in ten of the 300 placed**, so that the medians are of
# the comparisons the run was kept for.
if(_placed_after LESS 270)
    message(FATAL_ERROR "only ${_placed_after} of ${_compared} updates after the engine stopped "
                        "were placed where it was (${_at_an_end} nearest at an end), not 270:\n"
                        "${_said}")
endif()
# **Under 10 cm a second after**: 0.0054 m/s here, 0.0088 with the server
# stopped by fits and starts; an engine run on 0.332 to 0.358, 0.379 stopped, and
# 0.228 slowed - the least of them two and a quarter times this.
if(_after_placed GREATER 1000)
    message(FATAL_ERROR "with the engine stopped the median speed error where it was was "
                        "${_after_placed} tenths of a mm/s, not under 10 cm/s:\n${_said}")
endif()
# **And no more than half again before**: before is the throttle's changes,
# flown a step or two apart, 0.06 to 0.10 m/s; an engine run on slowed was
# 0.228 against 0.076.
math(EXPR _before_most "${_before_placed} * 3 / 2")
if(_after_placed GREATER _before_most)
    message(FATAL_ERROR "with the engine stopped the median speed error where it was was "
                        "${_after_placed} tenths of a mm/s, more than half again the "
                        "${_before_placed} before it:\n${_said}")
endif()
# **Slowed, the median distances, not the speed**: both under 5 cm, the
# bound the header names for a server at 0.6 of real time.
if(SLOWED AND (_after_mm GREATER 50 OR _before_mm GREATER 50))
    message(FATAL_ERROR "against a server at 0.6 of real time the median prediction "
                        "distance errors were ${_after_mm} mm after the stop and "
                        "${_before_mm} mm before, not both under 5 cm:\n${_said}")
endif()
if(NOT _stopped_here EQUAL 1)
    message(FATAL_ERROR "the client stopped ${_stopped_here} engines for the server's word, "
                        "not one:\n${_said}")
endif()
message(STATUS "with its engine stopped, the median speed error where it was was "
               "${_after_placed} tenths of a mm/s over ${_placed_after} updates, against "
               "${_before_placed} before, placed ${_steps_off_after} and ${_steps_off_before} "
               "steps off by the clocks' difference; by it, ${_after_speed} against "
               "${_before_speed}; the "
               "median distance ${_after_mm} mm against ${_before_mm} mm (the worst ${_after} m "
               "and ${_before} m)")

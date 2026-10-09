# server_ai_lands_at_its_model_weight.cmake - the server's AI aircraft, put
# on final at her model's own weight, is flown down at the approach speed for
# that weight and landed, with no go-around.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DPLAN=<plan naming the aircraft> -DMODEL=<its model> -DSTEPS=<steps>
#         -DKTS=<her approach speed for her model's weight, to 0.1 kt>
#         -P server_ai_lands_at_its_model_weight.cmake
#
# **Built, not hoped for.** The server loads no loading: every aircraft flies
# at her model's own weight, and her figures' approach speed is for another
# (sim::for_weight). One AI aircraft, the plan's, is put on final to YSSY 16R
# three miles out (`--ai-on-final YSSY/16R`) and flown STEPS steps as fast as
# they go. It must say she starts on final landed by the approach autopilot,
# that she is flown down it at KTS (within 0.1 kt), that she has left 16R
# and stopped beside it - landed - and never that she went around or was
# wrecked. **Seen to fail** with the speeds unscaled: all three, flown down
# at their figures' speeds (the PA-28 at 64.4 kt, the A380 136.3, the B-2A
# 124.0) - and the B-2A, at 327,000 lb against her speed's 177,160, went
# around.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}" --plan "${PLAN}"
            --ai-on-final YSSY/16R
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --ai 1 --headless --data "${DATA}" --plan "${PLAN}"
            --ai-on-final YSSY/16R --steps ${STEPS}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server exited ${_rc}:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "an AI's ${MODEL}, starts on final to YSSY 16R [0-9.]+ miles out, landed by the approach autopilot")
    message(FATAL_ERROR "the AI ${MODEL} was not put on final for the approach autopilot:\n${_out}")
endif()
# Flown down at the speed for her weight, as the server says it: KTS, the
# unit test's figure for her model's weight (test_lesson.cpp), to 0.1 kt.
if(NOT _out MATCHES "an AI's ${MODEL} at [0-9]+ lb, is flown down final at ([0-9.]+) kt")
    message(FATAL_ERROR "the server never said what speed the AI ${MODEL} is flown down at:\n${_out}")
endif()
set(_kts "${CMAKE_MATCH_1}")
string(REPLACE "." "" _kts10 "${_kts}")
string(REPLACE "." "" _want10 "${KTS}")
math(EXPR _diff "${_kts10} - ${_want10}")
if(_diff GREATER 1 OR _diff LESS -1)
    message(FATAL_ERROR "the AI ${MODEL} is flown down at ${_kts} kt, not the ${KTS} for her weight:\n${_out}")
endif()
if(_out MATCHES "wreck")
    message(FATAL_ERROR "the AI ${MODEL} was wrecked:\n${_out}")
endif()
if(_out MATCHES "goes around")
    message(FATAL_ERROR "the AI ${MODEL} went around:\n${_out}")
endif()
if(NOT _out MATCHES "${MODEL} \\(AI 1\\), has left YSSY 16R, stopped beside it")
    message(FATAL_ERROR "the AI ${MODEL} never landed and left the runway:\n${_out}")
endif()
message(STATUS "the AI ${MODEL} at her model's weight was landed on 16R and left it")

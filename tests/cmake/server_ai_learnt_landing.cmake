# server_ai_learnt_landing.cmake - the server's own AI Cessna 172P, started
# on final outside the learnt landing's gate, is flown into it by the approach
# autopilot, handed to the learnt landing there, and landed.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -P server_ai_learnt_landing.cmake
#
# **Built, not hoped for.** The server is told to put its AI aircraft on
# final to Sydney's 16R (`--ai-on-final YSSY/16R`): three miles out on the
# centreline and the glidepath, half a mile outside the gate. It flies
# 36,000 steps - five simulated minutes - as fast as they go (`--steps`),
# with nobody joining. It must say the AI C172P started there, then that it
# handed it to the learnt landing at its gate, and then that the learnt
# landing touched it down within 5 m of the centreline under 300 ft/min and
# stopped it on the runway: what a player's handed over is held to
# (server_learnt_landing.cmake).
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
            --ai-on-final YSSY/16R
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --ai 1 --headless --data "${DATA}"
            --ai-on-final YSSY/16R --steps 36000
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server exited ${_rc}:\n${_out}\n${_err}")
endif()

if(NOT _out MATCHES "aircraft ([0-9]+), an AI's c172p, starts on final to YSSY 16R 3\\.0 miles out, to be handed to the learnt landing at its gate")
    message(FATAL_ERROR "the server did not put its AI C172P on final, outside the gate:\n${_out}")
endif()
set(_n "${CMAKE_MATCH_1}")
string(FIND "${_out}" "aircraft ${_n}, an AI's, handed to the learnt landing at its gate, on final to YSSY 16R" _handed)
if(_handed EQUAL -1)
    message(FATAL_ERROR "the AI's approach was never handed to the learnt landing:\n${_out}")
endif()
if(NOT _out MATCHES "aircraft ${_n}: the learnt landing touched down on YSSY 16R at ([0-9]+) ft/min, ([-+][0-9.]+) m across the centreline, and stopped ([0-9]+) m along, ([-+][0-9.]+) m across")
    message(FATAL_ERROR "the learnt landing never said it had landed the AI's C172P:\n${_out}")
endif()
set(_sink "${CMAKE_MATCH_1}")
set(_across "${CMAKE_MATCH_2}")
set(_along "${CMAKE_MATCH_3}")
set(_stopped_across "${CMAKE_MATCH_4}")
string(FIND "${_out}" "aircraft ${_n}: the learnt landing touched down" _touched)
if(_touched LESS _handed)
    message(FATAL_ERROR "it said it touched down before it was handed over:\n${_out}")
endif()
string(REGEX REPLACE "^[-+]" "" _across_abs "${_across}")
string(REGEX REPLACE "^[-+]" "" _stopped_abs "${_stopped_across}")
# YSSY 16R is 3,962 m long; stopped on it is short of its end and within
# 15 m of its centreline, as the player's test holds it.
if(_sink GREATER_EQUAL 300 OR _across_abs GREATER_EQUAL 5 OR _stopped_abs GREATER_EQUAL 15
   OR _along GREATER_EQUAL 3959)
    message(FATAL_ERROR "the learnt landing was outside its limits: ${_sink} ft/min, ${_across} "
                        "m across, stopped ${_along} m along and ${_stopped_across} m across")
endif()
if(_out MATCHES "aircraft ${_n}[^\n]*wreck")
    message(FATAL_ERROR "the AI's C172P was wrecked:\n${_out}")
endif()
message(STATUS "the AI's C172P handed to the learnt landing at its gate and landed on YSSY 16R: "
               "${_sink} ft/min, ${_across} m across, stopped ${_along} m along, "
               "${_stopped_across} m across")

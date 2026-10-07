# server_ai_learnt_landing.cmake - the server's own AI Cessna 172P, flying a
# plan that ends in a landing, is flown on to the final approach, handed to
# the learnt landing at its gate, and landed; and so is one the operator puts
# on final.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -P server_ai_learnt_landing.cmake
#
# **Built, not hoped for.** The server flies the data's plan
# plans/sydney-arrival.plan with one AI aircraft and no other option: from
# eleven miles out down 16R's centreline to a waypoint, then `land YSSY_16R`.
# It flies 90,000 steps - twelve and a half simulated minutes - as fast as they
# go (`--steps`), with nobody joining. It must say it handed the AI C172P to
# the learnt landing at its gate on final to YSSY_16R, and then that the
# learnt landing touched it down within 5 m of the centreline under 300
# ft/min and stopped it on the runway: what a player's handed over is held
# to (server_learnt_landing.cmake). Then the same of an AI C172P the operator
# puts on final three miles out (`--ai-on-final YSSY/16R`), flying the
# default plan, in 36,000 steps.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

# Each run's ground fetched first, where it starts.
foreach(_where IN ITEMS "--plan;${DATA}/plans/sydney-arrival.plan" "--ai-on-final;YSSY/16R")
    execute_process(
        COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}" ${_where}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
    if(NOT _rc EQUAL 0)
        message(STATUS "the server could not get its terrain: ${_err}")
        cmake_language(EXIT 77)
    endif()
endforeach()

# The landing every run must say, within the limits; `_run` names the run.
function(landed_within_limits out n runway run)
    string(FIND "${out}" "aircraft ${n}, an AI's, handed to the learnt landing at its gate, on final to ${runway}" _handed)
    if(_handed EQUAL -1)
        message(FATAL_ERROR "${run}: the AI's approach was never handed to the learnt landing:\n${out}")
    endif()
    if(NOT out MATCHES "aircraft ${n}: the learnt landing touched down on ${runway} at ([0-9]+) ft/min, ([-+][0-9.]+) m across the centreline, and stopped ([0-9]+) m along, ([-+][0-9.]+) m across")
        message(FATAL_ERROR "${run}: the learnt landing never said it had landed the AI's C172P:\n${out}")
    endif()
    set(_sink "${CMAKE_MATCH_1}")
    set(_across "${CMAKE_MATCH_2}")
    set(_along "${CMAKE_MATCH_3}")
    set(_stopped_across "${CMAKE_MATCH_4}")
    string(FIND "${out}" "aircraft ${n}: the learnt landing touched down" _touched)
    if(_touched LESS _handed)
        message(FATAL_ERROR "${run}: it said it touched down before it was handed over:\n${out}")
    endif()
    string(REGEX REPLACE "^[-+]" "" _across_abs "${_across}")
    string(REGEX REPLACE "^[-+]" "" _stopped_abs "${_stopped_across}")
    # YSSY 16R is 3,962 m long; stopped on it is short of its end and within
    # 15 m of its centreline, as the player's test holds it.
    if(_sink GREATER_EQUAL 300 OR _across_abs GREATER_EQUAL 5 OR _stopped_abs GREATER_EQUAL 15
       OR _along GREATER_EQUAL 3959)
        message(FATAL_ERROR "${run}: the learnt landing was outside its limits: ${_sink} ft/min, "
                            "${_across} m across, stopped ${_along} m along and ${_stopped_across} m across")
    endif()
    if(out MATCHES "aircraft ${n}[^\n]*wreck")
        message(FATAL_ERROR "${run}: the AI's C172P was wrecked:\n${out}")
    endif()
    message(STATUS "${run}: handed to the learnt landing at its gate and landed on ${runway}: "
                   "${_sink} ft/min, ${_across} m across, stopped ${_along} m along, "
                   "${_stopped_across} m across")
endfunction()

# **A plan that ends in a landing**, and nothing else asked.
execute_process(
    COMMAND "${SERVER}" --port 0 --ai 1 --headless --data "${DATA}"
            --plan "${DATA}/plans/sydney-arrival.plan" --steps 90000
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server exited ${_rc}:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "aircraft ([0-9]+), an AI's, handed to the learnt landing")
    message(FATAL_ERROR "the AI flying the arrival plan was never handed to the learnt landing:\n${_out}")
endif()
landed_within_limits("${_out}" "${CMAKE_MATCH_1}" "YSSY_16R" "the arrival plan")

# **Put on final by the operator.**
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
landed_within_limits("${_out}" "${CMAKE_MATCH_1}" "YSSY 16R" "put on final")

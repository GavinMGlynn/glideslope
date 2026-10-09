# planned_landing.cmake - a model's flight plan from the ground that ends in a
# landing is checked, flown by the server's AI, and landed.
#
#   cmake -DSERVER=<glideslope_server> -DCLI=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DTASK=<task file>
#         -DPROVIDER=openai|anthropic -DMODEL=<model> -DSTEPS=<steps to fly>
#         (-DPLAYBACK=<recording> | -DRECORD=<recording>) -P planned_landing.cmake
#
# **Built, not hoped for.** The task - a C172P at Sydney told to "take off,
# fly to Bankstown airport and land there" - is asked of the model: played
# back from PLAYBACK, or asked of PROVIDER now by `glideslope_cli plan` with
# RECORD, which keeps what was asked and answered there with no header and so
# no key, and is then played back the same way. The server plans its one AI
# aircraft from it (--ai-planner, --ai-playback, --ai-task) and flies STEPS
# steps as fast as they go: simulated time. It must say:
#   - the plan it was given, ending in `land` - the model's answer, checked by
#     the planner as a copilot's route is (copilot::landing_refusal), and
#     landed on the server's own runway there - with nothing refused;
#   - the AI handed to the learnt landing at its gate on final to that runway,
#     and the learnt landing touching down on it under 300 ft/min, within 5 m
#     of its centreline, and stopping on it; and nothing wrecked.
#
# **Asked of PROVIDER now costs money**, so it is asked only when
# GLIDESLOPE_LIVE_MODEL=1 is set; otherwise it reports itself skipped (exit
# 77), before anything else. So does a model with no key, or one whose
# service will not answer for want of credit - skipped, never passed - and a
# run without the DEM's tiles, unless GLIDESLOPE_REQUIRE_NETWORK is set
# (client.cmake).

cmake_minimum_required(VERSION 3.28)

if(DEFINED RECORD AND NOT "$ENV{GLIDESLOPE_LIVE_MODEL}" STREQUAL "1")
    message(STATUS "${PROVIDER} is not asked: a live model call costs money, and is made only "
                   "with GLIDESLOPE_LIVE_MODEL=1 set")
    cmake_language(EXIT 77)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
file(MAKE_DIRECTORY "${WORK}")

# The task's three lines, for glideslope_cli plan.
file(STRINGS "${TASK}" _lines)
foreach(_line IN LISTS _lines)
    if(_line MATCHES "^aircraft (.+)$")
        set(_aircraft "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^airport (.+)$")
        set(_airport "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^task (.+)$")
        set(_task "${CMAKE_MATCH_1}")
    endif()
endforeach()
if(NOT DEFINED _aircraft OR NOT DEFINED _airport OR NOT DEFINED _task)
    message(FATAL_ERROR "${TASK} does not name its aircraft, airport and task")
endif()

if(DEFINED RECORD)
    file(REMOVE "${RECORD}")
    execute_process(
        COMMAND "${CLI}" --data "${DATA}" plan ${_aircraft} ${_airport} "${_task}"
                --provider ${PROVIDER} --model ${MODEL} --record "${RECORD}"
                --out "${WORK}/planned.plan"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _planned ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        if(_err MATCHES "no (OpenAI|Anthropic) key|no credits|credit balance|insufficient_quota")
            message(STATUS "${PROVIDER} cannot be asked here: ${_err}")
            cmake_language(EXIT 77)
        endif()
        glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
        message(FATAL_ERROR "${PROVIDER} made no plan of \"${_task}\":\n${_planned}\n${_err}")
    endif()
    message(STATUS "asked now, and recorded in ${RECORD}:\n${_planned}")
    set(PLAYBACK "${RECORD}")
endif()

execute_process(
    COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 1
            --ai-planner "1=${PROVIDER}:${MODEL}" --ai-playback "1=${PLAYBACK}"
            --ai-task "${TASK}" --steps ${STEPS}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server did not fly its planned aircraft (exit ${_rc}):\n${_out}\n${_err}")
endif()
if(_out MATCHES "AI 1[^\n]*(is refused|cannot be planned)")
    message(FATAL_ERROR "the model's plan was not flown:\n${_out}")
endif()
if(NOT _out MATCHES "AI 1 planned by [^\n]*")
    message(FATAL_ERROR "the server did not plan its AI aircraft by the model:\n${_out}")
endif()

# The plan as given, and its last line.
string(REGEX MATCHALL "AI 1 plan: [^\n]*" _plan_lines "${_out}")
list(LENGTH _plan_lines _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "the server said no plan:\n${_out}")
endif()
list(GET _plan_lines -1 _last)
if(NOT _last MATCHES "^AI 1 plan: land ([A-Za-z0-9_]+) ")
    message(FATAL_ERROR "the plan does not end in a landing - its last line is \"${_last}\":\n${_out}")
endif()
set(_named "${CMAKE_MATCH_1}")

if(NOT _out MATCHES "aircraft ([0-9]+), an AI's, handed to the learnt landing at its gate, on final to ([^\n]+)")
    message(FATAL_ERROR "the AI was never handed to the learnt landing on final:\n${_out}")
endif()
set(_n "${CMAKE_MATCH_1}")
set(_runway "${CMAKE_MATCH_2}")
if(NOT _out MATCHES "aircraft ${_n}: the learnt landing touched down on ${_runway} at ([0-9]+) ft/min, ([-+][0-9.]+) m across the centreline, and stopped ([0-9]+) m along, ([-+][0-9.]+) m across")
    message(FATAL_ERROR "the learnt landing never said it had landed the AI on ${_runway}:\n${_out}")
endif()
set(_sink "${CMAKE_MATCH_1}")
set(_across "${CMAKE_MATCH_2}")
set(_along "${CMAKE_MATCH_3}")
set(_stopped_across "${CMAKE_MATCH_4}")
string(REGEX REPLACE "^[-+]" "" _across_abs "${_across}")
string(REGEX REPLACE "^[-+]" "" _stopped_abs "${_stopped_across}")
if(_sink GREATER_EQUAL 300 OR _across_abs GREATER_EQUAL 5 OR _stopped_abs GREATER_EQUAL 15)
    message(FATAL_ERROR "the landing was outside its limits: ${_sink} ft/min, ${_across} m across, "
                        "stopped ${_along} m along and ${_stopped_across} m across:\n${_out}")
endif()
if(_out MATCHES "aircraft ${_n}[^\n]*wreck")
    message(FATAL_ERROR "the AI's aircraft was wrecked:\n${_out}")
endif()
message(STATUS "the model's plan ended in land ${_named}; the AI landed on ${_runway} by the "
               "learnt landing: ${_sink} ft/min, ${_across} m across, stopped ${_along} m along, "
               "${_stopped_across} m across")

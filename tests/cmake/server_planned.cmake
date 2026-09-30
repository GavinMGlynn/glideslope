# server_planned.cmake - each AI aircraft is planned by the model its server
# gives it, and a model with no key is refused.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DRECORDINGS=<dir of cbd-orbit-*.jsonl>
#         -DSTEPS=<steps to fly> -P server_planned.cmake
#
# **Three cases, and the test says it walked all three:**
#   - planned: a server with two AI aircraft, the first planned by OpenAI's
#     model and the second by Anthropic's, each played back from its recording
#     of "take off, climb to 3,000 ft and orbit the CBD" - the task file the
#     server reads - so no key is needed and none is sent anywhere. Each must
#     take off, reach its orbit and fly round it as often as its plan asks
#     (twice for one flown for ever), within 60 m of its circle and 50 ft of
#     its height - 3,000 ft, stacked 500 ft higher for the second planned, as
#     the server stacks its AI aircraft - its centre within 2 km of Town Hall,
#     and nothing wrecked. Flown for STEPS steps as fast as they go: simulated
#     time, not the machine's.
#   - keyless, anthropic and openai: a server told to plan with that model and
#     given no recording and no key refuses it, saying why, and the aircraft
#     flies the plan file - not faked. **No key can be found**: both key
#     variables are taken out of the environment and every directory a config
#     directory is found under points at an empty one, so the refusal comes
#     before any request could be made. No model is called by this test.
#
# Without the DEM's tiles or the runways it reports itself skipped (exit 77),
# unless GLIDESLOPE_REQUIRE_NETWORK is set (client.cmake).

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
# No key anywhere this server could look.
unset(ENV{GLIDESLOPE_OPENAI_KEY})
unset(ENV{GLIDESLOPE_ANTHROPIC_KEY})
file(REMOVE_RECURSE "${WORK}/no-config")
file(MAKE_DIRECTORY "${WORK}/no-config")
set(ENV{XDG_CONFIG_HOME} "${WORK}/no-config")
set(ENV{HOME} "${WORK}/no-config")
set(ENV{APPDATA} "${WORK}/no-config")

set(_walked 0)

# ---- planned: one aircraft by each model, from its recording ----
execute_process(
    COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 2
            --ai-planner 1=openai:gpt-5.4-mini-2026-03-17
            --ai-playback "1=${RECORDINGS}/cbd-orbit-openai.jsonl"
            --ai-planner 2=anthropic:claude-haiku-4-5-20251001
            --ai-playback "2=${RECORDINGS}/cbd-orbit-anthropic.jsonl"
            --steps ${STEPS}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server did not fly its planned aircraft (exit ${_rc}):\n${_out}\n${_err}")
endif()
if(_out MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was wrecked:\n${_out}")
endif()
if(NOT _out MATCHES "ran 2 AI aircraft")
    message(FATAL_ERROR "the server did not run its two AI aircraft:\n${_out}")
endif()
if(_out MATCHES "is refused")
    message(FATAL_ERROR "a model with a recording was refused:\n${_out}")
endif()

set(_flown 0)
foreach(_case "1:openai:0" "2:anthropic:1")
    string(REPLACE ":" ";" _case "${_case}")
    list(GET _case 0 _n)
    list(GET _case 1 _provider)
    list(GET _case 2 _stacked)
    if(NOT _out MATCHES "AI ${_n} planned by ${_provider}, [^\n]*played back")
        message(FATAL_ERROR "AI ${_n} was not planned by ${_provider}:\n${_out}")
    endif()
    # The orbit as planned, from the plan the server printed.
    if(NOT _out MATCHES "AI ${_n} plan: orbit ([A-Za-z0-9_]+) ([-0-9.]+) ([-0-9.]+) ([0-9.]+) ([0-9.]+) [0-9.]+ ([0-9]+) ")
        message(FATAL_ERROR "AI ${_n}'s plan has no orbit:\n${_out}")
    endif()
    set(_orbit "${CMAKE_MATCH_1}")
    set(_lat "${CMAKE_MATCH_2}")
    set(_lon "${CMAKE_MATCH_3}")
    set(_radius "${CMAKE_MATCH_4}")
    set(_altitude "${CMAKE_MATCH_5}")
    set(_planned_turns "${CMAKE_MATCH_6}")
    # And as flown: the line the server ends with for that aircraft.
    if(NOT _out MATCHES "planned by ${_provider}; took off from [^,]+, handed over ([0-9]+) ft above it; round ${_orbit} ([0-9.]+) turns, ([0-9]+) to ([0-9]+) m from its centre, at ([-0-9]+) to ([-0-9]+) ft")
        message(FATAL_ERROR "AI ${_n} did not take off and fly round ${_orbit}:\n${_out}")
    endif()
    set(_handed "${CMAKE_MATCH_1}")
    set(_turns "${CMAKE_MATCH_2}")
    set(_near "${CMAKE_MATCH_3}")
    set(_far "${CMAKE_MATCH_4}")
    set(_low "${CMAKE_MATCH_5}")
    set(_high "${CMAKE_MATCH_6}")
    if(_handed LESS 400)
        message(FATAL_ERROR "AI ${_n}'s take-off handed over only ${_handed} ft above the runway")
    endif()
    if(_planned_turns EQUAL 0)
        set(_planned_turns 2)
    endif()
    math(EXPR _short "${_planned_turns} - 1")
    if(_turns LESS ${_short}.99)
        message(FATAL_ERROR "AI ${_n} went round ${_orbit} ${_turns} times of ${_planned_turns}:\n${_out}")
    endif()
    string(REGEX REPLACE "\\..*$" "" _radius "${_radius}")
    string(REGEX REPLACE "\\..*$" "" _altitude "${_altitude}")
    if(_near LESS _radius)
        math(EXPR _inside "${_radius} - ${_near}")
    else()
        set(_inside 0)
    endif()
    math(EXPR _outside "${_far} - ${_radius}")
    if(_inside GREATER 60 OR _outside GREATER 60)
        message(FATAL_ERROR "AI ${_n} flew round ${_orbit} ${_near} to ${_far} m from its "
                            "centre, off its ${_radius} m circle by more than 60 m")
    endif()
    math(EXPR _want_ft "${_altitude} + ${_stacked} * 500")
    math(EXPR _below "${_want_ft} - ${_low}")
    math(EXPR _above "${_high} - ${_want_ft}")
    if(_below GREATER 50 OR _above GREATER 50)
        message(FATAL_ERROR "AI ${_n} flew round ${_orbit} at ${_low} to ${_high} ft, not "
                            "within 50 ft of ${_want_ft}")
    endif()
    # Its centre from Town Hall, in micro-degrees to keep CMake's arithmetic
    # whole: 0.111 m a micro-degree of latitude, 0.092 of longitude there.
    foreach(_v _lat _lon)
        set(_x "${${_v}}")
        if(NOT _x MATCHES "\\.")
            set(_x "${_x}.0")
        endif()
        string(REGEX MATCH "^(-?)([0-9]+)\\.([0-9]*)$" _m "${_x}")
        set(_sign "${CMAKE_MATCH_1}")
        set(_whole "${CMAKE_MATCH_2}")
        string(SUBSTRING "${CMAKE_MATCH_3}000000" 0 6 _frac)
        string(REGEX REPLACE "^0+([0-9])" "\\1" _frac "${_frac}")
        math(EXPR _micro${_v} "${_sign}(${_whole} * 1000000 + ${_frac})")
    endforeach()
    math(EXPR _north "(${_micro_lat} - (-33873200)) * 111 / 1000")
    math(EXPR _east "(${_micro_lon} - 151206600) * 92 / 1000")
    math(EXPR _away2 "${_north} * ${_north} + ${_east} * ${_east}")
    if(_away2 GREATER 4000000)
        message(FATAL_ERROR "AI ${_n}'s orbit is centred ${_north} m north and ${_east} m east "
                            "of Town Hall, more than 2 km")
    endif()
    message(STATUS "AI ${_n}, planned by ${_provider}: took off, handed over ${_handed} ft up, "
                   "round ${_orbit} ${_turns} times at ${_near} to ${_far} m (${_radius} asked) "
                   "and ${_low} to ${_high} ft (${_want_ft} asked)")
    math(EXPR _flown "${_flown} + 1")
endforeach()
if(NOT _flown EQUAL 2)
    message(FATAL_ERROR "only ${_flown} of the two planned aircraft were checked")
endif()
math(EXPR _walked "${_walked} + 1")

# ---- keyless: each model, asked with no key, is refused ----
foreach(_case "anthropic:Anthropic" "openai:OpenAI")
    string(REPLACE ":" ";" _case "${_case}")
    list(GET _case 0 _provider)
    list(GET _case 1 _name)
    execute_process(
        COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 1
                --ai-planner 1=${_provider} --steps 120
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the server with a keyless ${_provider} did not run (exit ${_rc}):\n${_out}\n${_err}")
    endif()
    if(NOT _out MATCHES "AI 1: ${_provider} is refused: no ${_name} key[^\n]*; it flies the plan file instead")
        message(FATAL_ERROR "${_provider} with no key was not refused, saying so:\n${_out}")
    endif()
    if(_out MATCHES "planned by")
        message(FATAL_ERROR "an aircraft said it was planned with no key:\n${_out}")
    endif()
    # And the aircraft is there, flying the plan file, as an AI's.
    if(NOT _out MATCHES "flew c172p \\(AI 1\\)[^\n]*\n  number [0-9]+, an AI's")
        message(FATAL_ERROR "the aircraft refused a planner did not fly the plan file:\n${_out}")
    endif()
    message(STATUS "${_provider} with no key: refused, and AI 1 flies the plan file")
    math(EXPR _walked "${_walked} + 1")
endforeach()

if(NOT _walked EQUAL 3)
    message(FATAL_ERROR "only ${_walked} of the three cases were run")
endif()

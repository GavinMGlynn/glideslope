# cli_plan_flown.cmake - a flight plan that takes off and orbits, flown by the
# AI over the DEM; and, with COMMAND, one a language model wrote from words.
#
#   cmake -DCLI=<glideslope_cli> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> [-DPLAN=<plan file>]
#         [-DCOMMAND=<words> -DPROVIDER=openai|anthropic [-DPLAYBACK=<recording>]]
#         [-DCENTRE_LAT=<deg> -DCENTRE_LON=<deg> -DALTITUDE_FT=<ft>]
#         -P cli_plan_flown.cmake
#
# **Built, not hoped for.** The plan - PLAN, or else what `glideslope_cli plan`
# makes of COMMAND for the Cessna standing at Sydney, played back from
# PLAYBACK or asked of PROVIDER now - is flown by `glideslope_cli fly-plan`
# over the DEM. It must:
#   - take off: the take-off autopilot hands over above the runway;
#   - reach its orbit and fly round it twice, within 150 m of its circle and
#     50 ft of ALTITUDE_FT - less than the geoid lifts the sea above the
#     ellipsoid at Sydney, 72 ft, so heights confused between the two show;
#   - have that orbit's centre within 2 km of CENTRE_LAT, CENTRE_LON - for
#     "the CBD", Town Hall - and nothing wrecked.
#
# Asked of PROVIDER now, with no key, or a key its service will not answer
# for want of credit, it reports itself skipped (exit 77), never passed; so
# does anything without the DEM's tiles.

cmake_minimum_required(VERSION 3.28)

set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
if(NOT DEFINED CENTRE_LAT)
    set(CENTRE_LAT -33.8732)
    set(CENTRE_LON 151.2066)
endif()
if(NOT DEFINED ALTITUDE_FT)
    set(ALTITUDE_FT 3000)
endif()

if(DEFINED COMMAND)
    set(PLAN "${WORK}/planned.plan")
    file(REMOVE "${PLAN}")
    set(_how)
    if(DEFINED PLAYBACK)
        set(_how --playback "${PLAYBACK}")
    endif()
    execute_process(
        COMMAND "${CLI}" --data "${DATA}" plan c172p YSSY "${COMMAND}" --provider ${PROVIDER}
                ${_how} --out "${PLAN}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _planned ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        if(NOT DEFINED PLAYBACK AND _err MATCHES "no (OpenAI|Anthropic) key|no credits|credit balance|insufficient_quota")
            message(STATUS "${PROVIDER} cannot be asked here: ${_err}")
            cmake_language(EXIT 77)
        endif()
        message(FATAL_ERROR "${PROVIDER} made no plan of \"${COMMAND}\":\n${_planned}\n${_err}")
    endif()
    message(STATUS "planned:\n${_planned}")
endif()

execute_process(
    COMMAND "${CLI}" --data "${DATA}" fly-plan "${PLAN}" --orbits 2
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(_err MATCHES "cannot download|could not download|no network")
    message(STATUS "the DEM could not be had: ${_err}")
    cmake_language(EXIT 77)
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the plan was not flown (exit ${_rc}):\n${_out}\n${_err}")
endif()
if(_out MATCHES "wrecked")
    message(FATAL_ERROR "the aircraft was wrecked:\n${_out}")
endif()

if(NOT _out MATCHES "took off: the take-off autopilot handed over at [-0-9]+ ft, ([0-9]+) ft above the runway")
    message(FATAL_ERROR "the plan did not take off:\n${_out}")
endif()
if(CMAKE_MATCH_1 LESS 400)
    message(FATAL_ERROR "the take-off handed over only ${CMAKE_MATCH_1} ft above the runway:\n${_out}")
endif()

# The orbit as flown.
if(NOT _out MATCHES "round ([A-Za-z0-9_]+): ([0-9.]+) turns, ([0-9]+) to ([0-9]+) m from its centre, at ([-0-9]+) to ([-0-9]+) ft")
    message(FATAL_ERROR "no orbit was flown:\n${_out}")
endif()
set(_orbit "${CMAKE_MATCH_1}")
set(_turns "${CMAKE_MATCH_2}")
set(_near "${CMAKE_MATCH_3}")
set(_far "${CMAKE_MATCH_4}")
set(_low "${CMAKE_MATCH_5}")
set(_high "${CMAKE_MATCH_6}")
if(_turns LESS 1.99)
    message(FATAL_ERROR "round ${_orbit} only ${_turns} times:\n${_out}")
endif()

# The orbit as planned: its centre and radius, from the plan's own line.
file(READ "${PLAN}" _plan)
if(NOT _plan MATCHES "orbit ${_orbit} ([-0-9.]+) ([-0-9.]+) ([0-9.]+) ")
    message(FATAL_ERROR "the plan has no orbit ${_orbit}:\n${_plan}")
endif()
set(_lat "${CMAKE_MATCH_1}")
set(_lon "${CMAKE_MATCH_2}")
set(_radius "${CMAKE_MATCH_3}")
if(_near LESS _radius)
    math(EXPR _inside "${_radius} - ${_near}")
else()
    set(_inside 0)
endif()
math(EXPR _outside "${_far} - ${_radius}")
if(_inside GREATER 150 OR _outside GREATER 150)
    message(FATAL_ERROR "round ${_orbit} from ${_near} to ${_far} m, off its ${_radius} m circle "
                        "by more than 150 m:\n${_out}")
endif()
math(EXPR _below "${ALTITUDE_FT} - ${_low}")
math(EXPR _above "${_high} - ${ALTITUDE_FT}")
if(_below GREATER 50 OR _above GREATER 50)
    message(FATAL_ERROR "round ${_orbit} at ${_low} to ${_high} ft, not within 50 ft of "
                        "${ALTITUDE_FT}:\n${_out}")
endif()
# Its centre from the place asked for: metres on a sphere, near enough over 2 km.
# CMake's math is integers only; micro-degrees keep the arithmetic whole.
foreach(_v _lat _lon CENTRE_LAT CENTRE_LON)
    set(_x "${${_v}}")
    if(NOT _x MATCHES "\\.")
        set(_x "${_x}.0")
    endif()
    string(REGEX MATCH "^(-?)([0-9]+)\\.([0-9]*)$" _m "${_x}")
    set(_sign "${CMAKE_MATCH_1}")
    set(_whole "${CMAKE_MATCH_2}")
    string(SUBSTRING "${CMAKE_MATCH_3}000000" 0 6 _frac)
    string(REGEX REPLACE "^0+([0-9])" "\\1" _frac "${_frac}")
    math(EXPR _micro_${_v} "${_sign}(${_whole} * 1000000 + ${_frac})")
endforeach()
# 0.111 m a micro-degree of latitude; of longitude, that times cos(33.9 deg), 0.830.
math(EXPR _north "(${_micro__lat} - ${_micro_CENTRE_LAT}) * 111 / 1000")
math(EXPR _east "(${_micro__lon} - ${_micro_CENTRE_LON}) * 92 / 1000")
math(EXPR _away2 "${_north} * ${_north} + ${_east} * ${_east}")
if(_away2 GREATER 4000000)
    message(FATAL_ERROR "the orbit's centre ${_lat} ${_lon} is more than 2 km from "
                        "${CENTRE_LAT} ${CENTRE_LON} (${_north} m north, ${_east} m east)")
endif()
message(STATUS "took off, and round ${_orbit} ${_turns} times at ${_near} to ${_far} m from "
               "its centre (${_radius} m asked) and ${_low} to ${_high} ft; its centre "
               "${_north} m north and ${_east} m east of the place asked for")

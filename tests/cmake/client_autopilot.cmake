# client_autopilot.cmake - the client's AI flies a flight plan.
#
#   cmake -DPROGRAM=<glideslope> -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -P client_autopilot.cmake
#
# Flies the flight screen headless with --plan sydney-harbour - from off Bondi to
# the Heads, the Harbour Bridge, Olympic Park and the airport, fifteen minutes -
# and shoots it at tick 115,000, after the plan is flown. The client prints each
# waypoint as the AI passes it: every one must be passed, in order, within
# 100 m and within 50 ft of its altitude, as the unit test holds the navigator
# to, and the plan flown. The flight stands on the DEM and draws it, so it
# needs the tiles and the geoid, fetched into CACHE: without the network the
# test is skipped (exit 77) unless GLIDESLOPE_REQUIRE_NETWORK is set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(_shot "${WORK}/autopilot-${DRIVER}.bmp")
file(REMOVE "${_shot}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                        --screen flight --plan sydney-harbour --shot-at 115000
                        --shot "${_shot}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0 AND _err MATCHES "could not download")
    if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "the DEM could not be had: ${_err}")
        cmake_language(EXIT 77)
    endif()
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope exited ${_rc}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

# Each waypoint, in order: passed within 100 m, at its altitude within 50 ft.
set(_rest "${_out}")
foreach(_wp THE_HEADS:3000 BRIDGE:2500 OLYMPIC:2500 AIRPORT:3000)
    string(REPLACE ":" ";" _wp "${_wp}")
    list(GET _wp 0 _name)
    list(GET _wp 1 _planned)
    string(FIND "${_rest}" "glideslope: passed ${_name} " _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "${_name} was not passed, or not in order:\n${_out}")
    endif()
    string(SUBSTRING "${_rest}" ${_at} -1 _rest)
    if(NOT _rest MATCHES "^glideslope: passed ${_name} ([0-9]+)\\.[0-9] m off at (-?[0-9]+) ft")
        message(FATAL_ERROR "cannot read the passing of ${_name}:\n${_out}")
    endif()
    set(_metres "${CMAKE_MATCH_1}")
    math(EXPR _off "${CMAKE_MATCH_2} - ${_planned}")
    if(_metres GREATER_EQUAL 100 OR _off GREATER 50 OR _off LESS -50)
        message(FATAL_ERROR "${_name} passed ${_metres} m off, ${_off} ft from its "
                            "altitude: at most 100 m and 50 ft")
    endif()
    string(LENGTH "glideslope: passed ${_name}" _skip)
    string(SUBSTRING "${_rest}" ${_skip} -1 _rest)
endforeach()
if(NOT _out MATCHES "glideslope: the plan is flown")
    message(FATAL_ERROR "the plan was not flown to its end:\n${_out}")
endif()
if(NOT EXISTS "${_shot}")
    message(FATAL_ERROR "no frame was written")
endif()

# frame_checklist.cmake - the checklist on screen is the one the flight is
# working through, ticks and all.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_checklist_check>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -P frame_checklist.cmake
#
# Flies the flight screen headless with the take-off checklist showing, shoots
# a frame, and has glideslope_checklist_check read the block back off it and
# hold every line - the phase, the count, and each item's tick or dash - to
# what the same run said it drew.
#
# The flight stands on the DEM and draws it, so it needs the tiles and the
# geoid, fetched into CACHE: without the network the test is skipped (exit 77)
# unless GLIDESLOPE_REQUIRE_NETWORK is set.
#
# It also holds the refusals: a phase of flight there is none of is refused by
# name, with the nine there are.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(_shot "${WORK}/checklist-${DRIVER}.bmp")
set(_said "${WORK}/checklist-${DRIVER}.txt")
file(REMOVE "${_shot}" "${_said}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

execute_process(
    COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 640x480
            --screen flight --checklist take-off
            --shot-at 300 --shot "${_shot}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

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

if(NOT _out MATCHES "showing the take-off checklist")
    message(FATAL_ERROR "the client did not say it was showing a checklist:\n${_out}")
endif()

# What the flight says it drew, one line a line, the prefix taken off.
string(REGEX MATCHALL "checklist: [^\n]+" _lines "${_out}")
if(NOT _lines)
    message(FATAL_ERROR "the flight drew no checklist:\n${_out}")
endif()
set(_text "")
foreach(_line IN LISTS _lines)
    string(REGEX REPLACE "^checklist: " "" _line "${_line}")
    string(STRIP "${_line}" _line)
    string(APPEND _text "${_line}\n")
endforeach()
file(WRITE "${_said}" "${_text}")

# Seven lines: the take-off list's six items and the heading above them. If
# the Cessna's take-off list changes length this must be changed with it,
# which is the point - a silent change to what is taught is not wanted.
list(LENGTH _lines _count)
if(NOT _count EQUAL 7)
    message(FATAL_ERROR
            "the take-off checklist drew ${_count} lines, not the heading and "
            "six items:\n${_text}")
endif()

execute_process(COMMAND "${CHECK}" "${_shot}" "${_said}"
                RESULT_VARIABLE _check_rc OUTPUT_VARIABLE _check_out
                ERROR_VARIABLE _check_err)
message(STATUS "${_check_out}")
if(NOT _check_rc EQUAL 0)
    message(FATAL_ERROR "the checklist on the frame is not the one flown:\n"
                        "${_check_err}")
endif()

# **A phase of flight there is none of is refused by name.**
execute_process(
    COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 64x48
            --screen flight --checklist before-lunch --shot-at 2
            --shot "${WORK}/checklist-none.bmp"
    RESULT_VARIABLE _bad_rc OUTPUT_VARIABLE _bad_out ERROR_VARIABLE _bad_err)
if(_bad_rc EQUAL 0)
    message(FATAL_ERROR "a phase of flight there is none of was accepted")
endif()
if(NOT _bad_err MATCHES "no phase of flight before-lunch")
    message(FATAL_ERROR "it did not say which phase it could not find:\n${_bad_err}")
endif()
foreach(_phase before-start taxi take-off climb cruise descent approach landing
        after-landing)
    if(NOT _bad_err MATCHES "${_phase}")
        message(FATAL_ERROR "it did not offer ${_phase}:\n${_bad_err}")
    endif()
endforeach()
message(STATUS "a phase there is none of says: ${_bad_err}")

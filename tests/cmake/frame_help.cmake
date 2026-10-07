# frame_help.cmake - the controls' help on screen is every line the client
# made from its bindings, and reads back off the frame as it said it drew it.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_help_check>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -P frame_help.cmake
#
# Flies the flight screen headless at the client's own size, 1280x720, with
# the help showing (--show-help, as F1 shows it), shoots a frame, and has
# glideslope_help_check read every column back off it and hold each line to
# what the same run said it drew. That every binding, key and command is in
# those lines is the_help_on_screen_names_every_binding_... (test_input.cpp).
#
# The flight stands on the DEM and draws it, so it needs the tiles and the
# geoid, fetched into CACHE: without the network the test is skipped (exit 77)
# unless GLIDESLOPE_REQUIRE_NETWORK is set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(_shot "${WORK}/help-${DRIVER}.bmp")
set(_said "${WORK}/help-${DRIVER}.txt")
file(REMOVE "${_shot}" "${_said}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

execute_process(
    COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 1280x720
            --screen flight --show-help --shot-at 300 --shot "${_shot}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

glideslope_skip_when_not_downloaded("${_rc}" "${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope exited ${_rc}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

# Every line made, drawn: none cut at the client's own size.
if(NOT _out MATCHES "the help drew ([0-9]+) lines of ([0-9]+)")
    message(FATAL_ERROR "the client did not say it drew the help:\n${_out}")
endif()
if(NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_1 LESS 40)
    message(FATAL_ERROR "the help drew ${CMAKE_MATCH_1} lines of ${CMAKE_MATCH_2}")
endif()
message(STATUS "the help drew ${CMAKE_MATCH_1} lines of ${CMAKE_MATCH_2}")

# What it said, whole: the check takes its "help column" and "help: " lines
# (a line can hold a semicolon, which a CMake list would split).
file(WRITE "${_said}" "${_out}")

execute_process(COMMAND "${CHECK}" "${_shot}" "${_said}"
                RESULT_VARIABLE _check_rc OUTPUT_VARIABLE _check_out
                ERROR_VARIABLE _check_err)
message(STATUS "${_check_out}")
if(NOT _check_rc EQUAL 0)
    message(FATAL_ERROR "the help on the frame is not what the client drew:\n${_check_err}")
endif()

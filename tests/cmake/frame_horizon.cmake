# **The HUD's horizon line lies on the horizon drawn**, banked BANK degrees,
# at each pitch walked. Over the Nullarbor Plain - level ground, a few metres
# of relief in the 30 km to the horizon - 63 m above it, in clear air and the
# cockpit view, with the imagery off so that the sky is blue and the ground
# green; each frame shot at the flight's second tick, the C172P started at
# that attitude. glideslope_horizon_check finds the horizon drawn, column by
# column, and holds it within TOLERANCE pixels of the HUD's line carried on
# across the frame.
#
# **What is walked**: banks of -60, -30, 0, 30 and 60 degrees, a test each
# (tests/CMakeLists.txt), by pitches of -15, 0 and 15 here - fifteen frames,
# every one with the horizon across the frame. Left out, and why: pitches
# beyond the field of view, where neither horizon is on the frame; banks
# steeper than 60, where the horizon stands too near upright for its columns
# to find it; and flight upside down. The ground is not quite level and is
# curved: from 63 m the horizon drawn lies 0.25 degrees below the true
# horizontal, 1.8 pixels on a 480-pixel frame - which the HUD's line, the true
# horizontal, does not follow. Measured, the horizon drawn is 1.7 to 2.7 pixels
# below the line on average and 3.24 at worst, so it is held to 4. The line
# as it was before 2026-10-06, a hundredth of the frame a degree of pitch,
# was 35 pixels off at 15 degrees down: seen to fail so.
cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

set(_eye "-31.0,129.0,190")
set(_size 640x480)
set(_tolerance 4)
set(_pitches -15 0 15)
set(_banks -60 -30 0 30 60)
if(NOT BANK IN_LIST _banks)
    message(FATAL_ERROR "BANK is '${BANK}'; it is one of ${_banks}")
endif()

set(_walked 0)
foreach(_pitch IN LISTS _pitches)
    set(_name "horizon-${DRIVER}-pitch${_pitch}-bank${BANK}")
    set(_shot "${WORK}/${_name}.bmp")
    set(_said "${WORK}/${_name}.txt")
    file(REMOVE "${_shot}" "${_said}")
    execute_process(
        COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size ${_size}
                --screen flight --aircraft c172p --imagery off --at "${_eye}"
                --attitude "${_pitch},${BANK},90" --shot "${_shot}"
        RESULT_VARIABLE _rc OUTPUT_FILE "${_said}" ERROR_VARIABLE _err)
    glideslope_skip_when_not_downloaded("${_rc}" "${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope at pitch ${_pitch}, bank ${BANK} exited ${_rc}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
    execute_process(COMMAND "${CHECK}" "${_shot}" "${_said}" ${_tolerance}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    message(STATUS "pitch ${_pitch}, bank ${BANK}: ${_out}${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "pitch ${_pitch}, bank ${BANK}: the HUD's horizon is not the "
                            "horizon drawn (${_shot})")
    endif()
    math(EXPR _walked "${_walked} + 1")
endforeach()
list(LENGTH _pitches _of)
if(NOT _walked EQUAL _of)
    message(FATAL_ERROR "walked ${_walked} pitches of ${_of}")
endif()
message(STATUS "bank ${BANK}: all ${_walked} pitches walked, the HUD's horizon on the horizon drawn")

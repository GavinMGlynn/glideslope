# frame_hud_jet.cmake - the HUD shows a jet's Mach number and flight level at
# the tick it was shot.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_hud_check> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -P frame_hud_jet.cmake
#
# Flies the flight screen headless for 600 ticks - five seconds - with the
# A320, from 11,000 m above Sydney, in the standard atmosphere, with --trace;
# shoots the last; and has glideslope_hud_check read the HUD back out of the
# frame and hold every number to the traced state - the Mach number and the
# flight level among them, which at that height and speed must both apply -
# and the credits along the bottom to the DEM's and the imagery's. The flight
# draws the DEM, so it needs the tiles and the geoid, fetched into CACHE:
# without the network the test is skipped (exit 77) unless
# GLIDESLOPE_REQUIRE_NETWORK is set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(_shot "${WORK}/hud-jet-${DRIVER}.bmp")
set(_trace "${WORK}/hud-jet-${DRIVER}.trace.txt")
file(REMOVE "${_shot}" "${_trace}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 640x480
                        --screen flight --aircraft a320 --at -33.9461,151.1772,11000
                        --shot-at 600 --shot "${_shot}" --trace
                RESULT_VARIABLE _rc OUTPUT_FILE "${_trace}" ERROR_VARIABLE _err)
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

# The Mach number and flight level must both apply, or this tests nothing.
file(STRINGS "${_trace}" _last REGEX "^trace tick 600 ")
if(NOT _last MATCHES " mach 0\\.([0-9][0-9])[0-9]* pa_ft ([0-9]+)\\.")
    message(FATAL_ERROR "no Mach number or pressure altitude at tick 600: ${_last}")
endif()
if(CMAKE_MATCH_1 LESS 40 OR CMAKE_MATCH_2 LESS 18000)
    message(FATAL_ERROR "the A320 is at Mach 0.${CMAKE_MATCH_1} and ${CMAKE_MATCH_2} ft: the "
                        "HUD would show neither its Mach number nor its flight level")
endif()

execute_process(COMMAND "${CHECK}" "${_shot}" "${_trace}" 600 dem imagery
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

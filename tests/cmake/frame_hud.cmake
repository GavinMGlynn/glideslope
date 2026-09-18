# frame_hud.cmake - the HUD shows the flight's state at the tick it was shot.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_hud_check> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -P frame_hud.cmake
#
# Flies the flight screen headless for 600 ticks - five seconds - in the
# weather reported at Sydney now, with --trace, shoots the last, and has
# glideslope_hud_check read the HUD back out of the frame and hold every number
# to the traced state, and the credits along the bottom to the DEM's and
# Open-Meteo's. The flight stands on the DEM, and draws it, so it needs the
# tiles and the geoid, fetched into CACHE, and the weather: without the network
# the test is skipped (exit 77) unless GLIDESLOPE_REQUIRE_NETWORK is set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(_shot "${WORK}/hud-${DRIVER}.bmp")
set(_trace "${WORK}/hud-${DRIVER}.trace.txt")
file(REMOVE "${_shot}" "${_trace}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

# glideslope_client fails a test outright; a missing network is a skip, so the
# run is made here and its failure looked at.
set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 640x480
                        --screen flight --weather YSSY --shot-at 600 --shot "${_shot}" --trace
                RESULT_VARIABLE _rc OUTPUT_FILE "${_trace}" ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 AND _err MATCHES "could not download")
    if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "the DEM or the weather could not be had: ${_err}")
        cmake_language(EXIT 77)
    endif()
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope exited ${_rc}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

execute_process(COMMAND "${CHECK}" "${_shot}" "${_trace}" 600 dem weather
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

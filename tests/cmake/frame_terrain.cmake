# frame_terrain.cmake - the terrain drawn is the terrain the DEM says is there.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_terrain_check> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -DDATA=<data dir> [-DIMAGERY=ON]
#         -P frame_terrain.cmake
#
# Shoots the terrain screen headless at 320x240 from 9 km east of Mount
# Taranaki's summit, 1,500 m up its slope, looking just below the summit - a
# cone 2,518 m high on a plain that runs to the sea, all in one DEM tile - and
# has glideslope_terrain_check ray-cast the same view through the DEM and hold
# the frame to it. With IMAGERY, at 640x480 with the open imagery draped on the
# terrain, the reference's ground is the imagery at each place, and the check
# also finds where the imagery matches best. The terrain
# comes from the DEM's tiles and the geoid, fetched into CACHE: without the
# network the test is skipped (exit 77) unless GLIDESLOPE_REQUIRE_NETWORK is
# set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_eye "-39.33,174.155,1500")
set(_summit "-39.2967,174.0634,2200")

file(MAKE_DIRECTORY "${WORK}")
if(IMAGERY)
    set(_name "terrain-imagery-${DRIVER}")
    set(_size 640x480)
    set(_imagery on)
    set(_check_imagery imagery)
else()
    set(_name "terrain-${DRIVER}")
    set(_size 320x240)
    set(_imagery off)
    set(_check_imagery "")
endif()
set(_shot "${WORK}/${_name}.bmp")
file(REMOVE "${_shot}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size ${_size}
                        --screen terrain --at "${_eye}" --toward "${_summit}"
                        --imagery ${_imagery}
                        --shot "${_shot}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0 AND _err MATCHES "could not download|without their imagery")
    if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "the DEM or the imagery could not be had: ${_err}")
        cmake_language(EXIT 77)
    endif()
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope exited ${_rc}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

execute_process(COMMAND "${CHECK}" "${_shot}" "${WORK}/${_name}-reference.bmp"
                        "${WORK}/${_name}-difference.bmp" "${_eye}" "${_summit}"
                        "${CACHE}" "${DATA}" ${_check_imagery}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

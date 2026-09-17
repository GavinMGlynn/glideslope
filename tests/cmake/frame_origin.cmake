# frame_origin.cmake - the camera-relative floating origin: a scene looks the
# same wherever on the Earth it is, and a still scene the same from frame to
# frame.
#
#   cmake -DPROGRAM=<glideslope> -DDRIVER=<vulkan|direct3d12|metal> -DWORK=<dir>
#         -P frame_origin.cmake
#
# The client's "origin" screen (src/frontend/client/scenes.hpp) - boxes from 7 cm
# to 2 m across, 2 to 15 m away - is shot at the Earth's centre, where every
# coordinate is small, and at three places where they are millions of metres:
# on the equator at the surface, at 45 N 45 E 10 km up, and at Sydney airport,
# there at ticks 2 and 120 - frames 1 and 60. Every frame must be the same file, byte for byte. A
# renderer whose floats held whole ECEF coordinates could not place anything
# there more finely than a quarter or half of a metre, and the boxes would move.
#
# And the frame must hold the scene: identical frames of nothing prove nothing,
# so at least a quarter of the pixels must not be sky.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/bmp.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(_places
    "centre|--at-ecef|0,0,0|2"
    "equator|--at-ecef|6378137,0,0|2"
    "45n45e|--at|45,45,10000|2"
    "sydney|--at|-33.9461,151.1772,6|2"
    "sydney-tick-120|--at|-33.9461,151.1772,6|120")
set(_reference "")
set(_different "")
foreach(_entry IN LISTS _places)
    string(REPLACE "|" ";" _fields "${_entry}")
    list(GET _fields 0 _place)
    list(GET _fields 1 _option)
    list(GET _fields 2 _value)
    list(GET _fields 3 _tick)
    set(_shot "${WORK}/origin-${DRIVER}-${_place}.bmp")
    file(REMOVE "${_shot}")
    glideslope_client(_out --headless --gpu-driver "${DRIVER}" --size 64x48
                      --screen origin ${_option} ${_value} --shot-at ${_tick}
                      --shot "${_shot}")
    if(NOT _out MATCHES "GPU driver ${DRIVER}")
        message(FATAL_ERROR "asked for ${DRIVER}, got something else:\n${_out}")
    endif()
    file(SHA256 "${_shot}" _hash)
    message(STATUS "${DRIVER} at ${_place}, tick ${_tick}: ${_hash}")
    if(_reference STREQUAL "")
        set(_reference "${_hash}")
        set(_reference_shot "${_shot}")
    elseif(NOT _hash STREQUAL _reference)
        string(APPEND _different "  ${_place}, tick ${_tick}\n")
    endif()
endforeach()

bmp_load("${_reference_shot}")
set(_scene 0)
foreach(_y RANGE 0 47)
    foreach(_x RANGE 0 63)
        bmp_pixel(_p ${_x} ${_y})
        bmp_near(_sky "${_p}" "115;166;230;255")
        if(NOT _sky)
            math(EXPR _scene "${_scene} + 1")
        endif()
    endforeach()
endforeach()
message(STATUS "${DRIVER}: ${_scene} of 3072 pixels are the scene rather than sky")
if(_scene LESS 768)
    message(FATAL_ERROR "only ${_scene} of 3072 pixels are not sky; the scene did not draw")
endif()
if(NOT _different STREQUAL "")
    message(FATAL_ERROR "the scene at the Earth's centre differs from:\n${_different}")
endif()

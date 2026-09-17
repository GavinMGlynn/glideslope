# frame_depth.cmake - reversed depth: a distant mountain and a nearby aircraft
# in one frame, and no z-fighting at either.
#
#   cmake -DPROGRAM=<glideslope> -DDRIVER=<vulkan|direct3d12|metal> -DWORK=<dir>
#         -P frame_depth.cmake
#
# The client's "depth" scene (src/frontend/client/scenes.hpp): the top half of
# the frame is a mountain face 40 km away with a second face 1 m in front of it;
# the bottom half an aircraft's skin 1 m away with a decal 1 mm in front of it.
# The nearer surface of each pair is drawn first. Every pixel of the top half
# must be the nearer face, (204, 204, 204), and every pixel of the bottom half
# the decal, (51, 153, 51) - a depth buffer that cannot separate a pair shows
# the surface behind, in some pixels or all of them. Shot at the Earth's centre
# and at Sydney airport.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/bmp.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(_failures "")
foreach(_place centre sydney)
    set(_shot "${WORK}/depth-${DRIVER}-${_place}.bmp")
    file(REMOVE "${_shot}")
    if(_place STREQUAL "sydney")
        set(_at --at -33.9461,151.1772,6)
    else()
        set(_at)
    endif()
    glideslope_client(_out --headless --gpu-driver "${DRIVER}" --size 64x48
                      --scene depth ${_at} --shot "${_shot}")
    if(NOT _out MATCHES "GPU driver ${DRIVER}")
        message(FATAL_ERROR "asked for ${DRIVER}, got something else:\n${_out}")
    endif()

    bmp_load("${_shot}")
    if(NOT BMP_WIDTH EQUAL 64 OR NOT BMP_HEIGHT EQUAL 48)
        message(FATAL_ERROR "expected a 64x48 frame, got ${BMP_WIDTH}x${BMP_HEIGHT}")
    endif()
    set(_behind_far 0)
    set(_behind_near 0)
    set(_other "")
    foreach(_y RANGE 47)
        if(_y LESS 24)
            set(_front "204;204;204;255")
            set(_back "102;102;102;255")
            set(_count _behind_far)
        else()
            set(_front "51;153;51;255")
            set(_back "204;51;51;255")
            set(_count _behind_near)
        endif()
        foreach(_x RANGE 63)
            bmp_pixel(_p ${_x} ${_y})
            bmp_near(_is_front "${_p}" "${_front}")
            bmp_near(_is_back "${_p}" "${_back}")
            if(_is_back)
                math(EXPR ${_count} "${${_count}} + 1")
            elseif(NOT _is_front AND _other STREQUAL "")
                set(_other "(${_x}, ${_y}) is ${_p}, neither surface")
            endif()
        endforeach()
    endforeach()
    message(STATUS "${DRIVER} at ${_place}: of 1536 pixels each, ${_behind_far} show the mountain face behind, ${_behind_near} the skin behind the decal")
    if(_behind_far GREATER 0 OR _behind_near GREATER 0 OR NOT _other STREQUAL "")
        string(APPEND _failures "  at ${_place}: ${_behind_far} pixels of the far pair and ${_behind_near} of the near pair show the surface behind ${_other}\n")
    endif()
endforeach()
if(NOT _failures STREQUAL "")
    message(FATAL_ERROR "z-fighting on ${DRIVER}:\n${_failures}")
endif()

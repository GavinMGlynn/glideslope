# frame_is_sky.cmake - the client renders a frame on a GPU driver and writes it,
# and every pixel of what it wrote is the sky it was cleared to.
#
#   cmake -DPROGRAM=<glideslope> -DDRIVER=<vulkan|direct3d12|metal> -DWORK=<dir>
#         [-DWINDOW=ON] -P frame_is_sky.cmake
#
# With WINDOW=ON the client opens a window and draws to it too, and must say
# that frames reached the window's swapchain. On Linux a window needs a display:
# without DISPLAY or WAYLAND_DISPLAY the test reports itself skipped (exit 77),
# unless GLIDESLOPE_REQUIRE_WINDOW is set in the environment, as CI sets it,
# where no display is a failure.
#
# The sky is (0.45, 0.65, 0.90, 1.0); each channel must be within 1 of that
# times 255, which allows for how a driver rounds.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/bmp.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(WINDOW)
    set(_mode --shot-at 10)
    set(_name "${DRIVER}-window")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND "$ENV{DISPLAY}" STREQUAL ""
       AND "$ENV{WAYLAND_DISPLAY}" STREQUAL "")
        if(NOT "$ENV{GLIDESLOPE_REQUIRE_WINDOW}" STREQUAL "")
            message(FATAL_ERROR "no DISPLAY or WAYLAND_DISPLAY, and GLIDESLOPE_REQUIRE_WINDOW is set")
        endif()
        message(STATUS "no DISPLAY or WAYLAND_DISPLAY; a window cannot open here")
        cmake_language(EXIT 77)
    endif()
else()
    set(_mode --headless)
    set(_name "${DRIVER}")
endif()

file(MAKE_DIRECTORY "${WORK}")
set(_shot "${WORK}/sky-${_name}.bmp")
file(REMOVE "${_shot}")
glideslope_client(_out ${_mode} --gpu-driver "${DRIVER}" --size 64x48 --shot "${_shot}")
if(NOT _out MATCHES "GPU driver ${DRIVER}")
    message(FATAL_ERROR "asked for ${DRIVER}, got something else:\n${_out}")
endif()
if(WINDOW AND NOT _out MATCHES "presented [1-9][0-9]* of [0-9]+ frames to the window")
    message(FATAL_ERROR "no frame reached the window's swapchain:\n${_out}")
endif()

bmp_load("${_shot}")
if(NOT BMP_WIDTH EQUAL 64 OR NOT BMP_HEIGHT EQUAL 48)
    message(FATAL_ERROR "expected a 64x48 frame, got ${BMP_WIDTH}x${BMP_HEIGHT}")
endif()
set(_wrong 0)
set(_first_wrong "")
foreach(_y RANGE 47)
    foreach(_x RANGE 63)
        bmp_pixel(_p ${_x} ${_y})
        bmp_near(_ok "${_p}" "115;166;230;255")
        if(NOT _ok)
            math(EXPR _wrong "${_wrong} + 1")
            if(_first_wrong STREQUAL "")
                set(_first_wrong "(${_x}, ${_y}) is ${_p}")
            endif()
        endif()
    endforeach()
endforeach()
message(STATUS "${DRIVER}: checked 3072 pixels of 64x48")
if(_wrong GREATER 0)
    message(FATAL_ERROR "${_wrong} pixels are not the sky (115, 166, 230, 255); first: ${_first_wrong}")
endif()

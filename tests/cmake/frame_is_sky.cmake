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
# Reads the BMP itself - header, bit masks, every pixel - rather than trusting
# that a file of the right size is a frame of the right colour. The sky is
# (0.45, 0.65, 0.90, 1.0); each channel must be within 1 of that times 255,
# which allows for how a driver rounds.

cmake_minimum_required(VERSION 3.28)

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
set(_shot "${WORK}/${_name}.bmp")
file(REMOVE "${_shot}")
execute_process(
    COMMAND "${PROGRAM}" ${_mode} --gpu-driver "${DRIVER}" --size 64x48 --shot "${_shot}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope on ${DRIVER} exited ${_rc}\n${_out}${_err}")
endif()
if(NOT _out MATCHES "GPU driver ${DRIVER}")
    message(FATAL_ERROR "asked for ${DRIVER}, got something else:\n${_out}")
endif()
if(WINDOW AND NOT _out MATCHES "presented [1-9][0-9]* of [0-9]+ frames to the window")
    message(FATAL_ERROR "no frame reached the window's swapchain:\n${_out}${_err}")
endif()
if(NOT EXISTS "${_shot}")
    message(FATAL_ERROR "glideslope said it wrote a frame and ${_shot} does not exist\n${_out}")
endif()

file(READ "${_shot}" _hex HEX)

# A little-endian unsigned integer of `bytes` bytes at byte `offset`.
function(le out offset bytes)
    math(EXPR _start "${offset} * 2")
    math(EXPR _len "${bytes} * 2")
    string(SUBSTRING "${_hex}" ${_start} ${_len} _raw)
    set(_value 0)
    math(EXPR _last "${bytes} - 1")
    foreach(_i RANGE ${_last} 0 -1)
        math(EXPR _c "${_i} * 2")
        string(SUBSTRING "${_raw}" ${_c} 2 _byte)
        math(EXPR _value "(${_value} << 8) + 0x${_byte}" OUTPUT_FORMAT DECIMAL)
    endforeach()
    set(${out} ${_value} PARENT_SCOPE)
endfunction()

string(SUBSTRING "${_hex}" 0 4 _magic)
if(NOT _magic STREQUAL "424d")
    message(FATAL_ERROR "${_shot} is not a BMP")
endif()
le(_offset 10 4)
le(_width 18 4)
le(_height 22 4)
le(_bpp 28 2)
le(_compression 30 4)
if(NOT _width EQUAL 64 OR NOT _height EQUAL 48 OR NOT _bpp EQUAL 32)
    message(FATAL_ERROR "expected a 64x48 32-bit frame, got ${_width}x${_height} at ${_bpp} bits")
endif()

# Which byte is which channel: from the bit masks when the BMP carries them
# (compression 3), otherwise BMP's own blue-green-red-alpha order.
set(_shift_r 16)
set(_shift_g 8)
set(_shift_b 0)
set(_shift_a 24)
if(_compression EQUAL 3)
    foreach(_ch r g b a)
        if(_ch STREQUAL "r")
            le(_mask 54 4)
        elseif(_ch STREQUAL "g")
            le(_mask 58 4)
        elseif(_ch STREQUAL "b")
            le(_mask 62 4)
        else()
            le(_mask 66 4)
        endif()
        foreach(_s 0 8 16 24)
            math(EXPR _m "0xff << ${_s}" OUTPUT_FORMAT DECIMAL)
            if(_mask EQUAL _m)
                set(_shift_${_ch} ${_s})
            endif()
        endforeach()
    endforeach()
endif()

set(_expect_r 115)
set(_expect_g 166)
set(_expect_b 230)
set(_expect_a 255)
math(EXPR _pixels "${_width} * ${_height}")
math(EXPR _last "${_pixels} - 1")
set(_wrong 0)
set(_first_wrong "")
foreach(_p RANGE ${_last})
    math(EXPR _at "${_offset} + ${_p} * 4")
    le(_pixel ${_at} 4)
    foreach(_ch r g b a)
        math(EXPR _v "(${_pixel} >> ${_shift_${_ch}}) & 0xff")
        math(EXPR _d "${_v} - ${_expect_${_ch}}")
        if(_d GREATER 1 OR _d LESS -1)
            math(EXPR _wrong "${_wrong} + 1")
            if(_first_wrong STREQUAL "")
                set(_first_wrong "pixel ${_p} channel ${_ch} is ${_v}, expected ${_expect_${_ch}}")
            endif()
        endif()
    endforeach()
endforeach()
message(STATUS "${DRIVER}: checked ${_pixels} pixels of ${_width}x${_height}")
if(_wrong GREATER 0)
    message(FATAL_ERROR "${_wrong} channel values are not the sky; first: ${_first_wrong}")
endif()

# bmp.cmake - reads the 32-bit BMPs the client writes, pixel by pixel.
#
#   include(bmp.cmake)
#   bmp_load(<path>)            sets BMP_WIDTH and BMP_HEIGHT, and what
#                               bmp_pixel needs, in the caller's scope
#   bmp_pixel(<out> <x> <y>)    <out> is "r;g;b;a"; (0, 0) is the top left
#
# Reads the header and the bit masks rather than trusting that a file of the
# right size is a frame of the right colours, and handles rows stored either
# way up.

# A little-endian unsigned integer of `bytes` bytes at byte `offset`.
function(bmp_le out offset bytes)
    math(EXPR _start "${offset} * 2")
    math(EXPR _len "${bytes} * 2")
    string(SUBSTRING "${BMP_HEX}" ${_start} ${_len} _raw)
    set(_value 0)
    math(EXPR _last "${bytes} - 1")
    foreach(_i RANGE ${_last} 0 -1)
        math(EXPR _c "${_i} * 2")
        string(SUBSTRING "${_raw}" ${_c} 2 _byte)
        math(EXPR _value "(${_value} << 8) + 0x${_byte}" OUTPUT_FORMAT DECIMAL)
    endforeach()
    set(${out} ${_value} PARENT_SCOPE)
endfunction()

macro(bmp_load path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${path} does not exist")
    endif()
    file(READ "${path}" BMP_HEX HEX)
    string(SUBSTRING "${BMP_HEX}" 0 4 _bmp_magic)
    if(NOT _bmp_magic STREQUAL "424d")
        message(FATAL_ERROR "${path} is not a BMP")
    endif()
    bmp_le(BMP_OFFSET 10 4)
    bmp_le(BMP_WIDTH 18 4)
    bmp_le(BMP_HEIGHT 22 4)
    bmp_le(_bmp_bpp 28 2)
    bmp_le(_bmp_compression 30 4)
    # A negative height - stored as its 32-bit two's complement - means the rows
    # run top to bottom.
    set(BMP_TOP_DOWN OFF)
    if(BMP_HEIGHT GREATER_EQUAL 2147483648)
        math(EXPR BMP_HEIGHT "4294967296 - ${BMP_HEIGHT}")
        set(BMP_TOP_DOWN ON)
    endif()
    if(NOT _bmp_bpp EQUAL 32)
        message(FATAL_ERROR "${path} is ${_bmp_bpp} bits a pixel, not 32")
    endif()
    # Which byte is which channel: from the bit masks when the BMP carries them
    # (compression 3), otherwise BMP's own blue-green-red-alpha order.
    set(BMP_SHIFT_r 16)
    set(BMP_SHIFT_g 8)
    set(BMP_SHIFT_b 0)
    set(BMP_SHIFT_a 24)
    if(_bmp_compression EQUAL 3)
        set(_bmp_at 54)
        foreach(_ch r g b a)
            bmp_le(_bmp_mask ${_bmp_at} 4)
            foreach(_s 0 8 16 24)
                math(EXPR _m "0xff << ${_s}" OUTPUT_FORMAT DECIMAL)
                if(_bmp_mask EQUAL _m)
                    set(BMP_SHIFT_${_ch} ${_s})
                endif()
            endforeach()
            math(EXPR _bmp_at "${_bmp_at} + 4")
        endforeach()
    endif()
endmacro()

function(bmp_pixel out x y)
    if(BMP_TOP_DOWN)
        set(_row ${y})
    else()
        math(EXPR _row "${BMP_HEIGHT} - 1 - ${y}")
    endif()
    math(EXPR _at "${BMP_OFFSET} + (${_row} * ${BMP_WIDTH} + ${x}) * 4")
    bmp_le(_pixel ${_at} 4)
    set(_rgba "")
    foreach(_ch r g b a)
        math(EXPR _v "(${_pixel} >> ${BMP_SHIFT_${_ch}}) & 0xff")
        list(APPEND _rgba ${_v})
    endforeach()
    set(${out} "${_rgba}" PARENT_SCOPE)
endfunction()

# Whether `rgba` is within 1 of `expected` in every channel - how far drivers
# round apart.
function(bmp_near out rgba expected)
    set(_near ON)
    foreach(_i 0 1 2 3)
        list(GET rgba ${_i} _v)
        list(GET expected ${_i} _e)
        math(EXPR _d "${_v} - ${_e}")
        if(_d GREATER 1 OR _d LESS -1)
            set(_near OFF)
        endif()
    endforeach()
    set(${out} ${_near} PARENT_SCOPE)
endfunction()

# cross_platform_flights.cmake - every CI platform flies the same flights and
# lands in the same place, within stated tolerances.
#
#   cmake -DDIR=<directory of *.figures.txt and *.selftest.txt> -P cross_platform_flights.cmake
#
# CI's release build on each platform and compiler writes what
# `glideslope_cli figures c172p` and `glideslope_cli selftest` print. The
# simulation is floating point and the machines differ, so the numbers are not
# expected to be identical, only close:
#
#   each published figure    within 1% of its published value, across every
#                            platform - ten times tighter than the handbook
#                            tolerance, so a platform that flies differently
#                            is caught long before it flies out of range
#   the selftest's end       within 300 ft horizontally and 30 ft vertically
#                            after five minutes, 1 kt of airspeed and 2 degrees
#                            of heading
#
# The platforms are named, and a missing one fails: a comparison of the
# platforms that happened to report is not a comparison of all of them.

cmake_minimum_required(VERSION 3.28)

set(_platforms ubuntu-gcc rocky-gcc macos-appleclang windows-msvc windows-clang-cl)
set(_failures "")

# Published values, from the range each figure line prints (its middle), so the
# tolerance scales with the figure.
foreach(_p IN LISTS _platforms)
    foreach(_kind figures selftest)
        if(NOT EXISTS "${DIR}/${_p}.${_kind}.txt")
            string(APPEND _failures "\n  ${_p}: no ${_kind} results")
        endif()
    endforeach()
endforeach()
if(_failures)
    message(FATAL_ERROR "missing platforms:${_failures}")
endif()

set(_figure_names "")
foreach(_p IN LISTS _platforms)
    file(STRINGS "${DIR}/${_p}.figures.txt" _lines)
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "^  ([a-z0-9_]+) +([-0-9.]+) +([-0-9.]+) \\.\\. ([-0-9.]+) ")
            set(_name "${CMAKE_MATCH_1}")
            set(_fig_${_name}_${_p} "${CMAKE_MATCH_2}")
            math(EXPR _dummy "0")
            set(_low "${CMAKE_MATCH_3}")
            set(_high "${CMAKE_MATCH_4}")
            set(_mid_${_name} "${_low};${_high}")
            if(NOT _name IN_LIST _figure_names)
                list(APPEND _figure_names "${_name}")
            endif()
        endif()
    endforeach()
endforeach()

list(LENGTH _figure_names _n_figures)
if(NOT _n_figures EQUAL 9)
    string(APPEND _failures "\n  expected 9 figures, found ${_n_figures}: ${_figure_names}")
endif()

# CMake's math() is integer-only, so the comparisons go through a tiny helper
# that works in millionths.
function(millionths value out)
    if(NOT value MATCHES "^(-?)([0-9]*)\\.?([0-9]*)$")
        message(FATAL_ERROR "not a number: ${value}")
    endif()
    set(_sign "${CMAKE_MATCH_1}")
    set(_int "${CMAKE_MATCH_2}")
    set(_frac "${CMAKE_MATCH_3}000000")
    string(SUBSTRING "${_frac}" 0 6 _frac)
    if(_int STREQUAL "")
        set(_int 0)
    endif()
    string(REGEX REPLACE "^0+([0-9])" "\\1" _frac "${_frac}")
    math(EXPR _v "${_int} * 1000000 + ${_frac}")
    if(_sign STREQUAL "-")
        math(EXPR _v "-${_v}")
    endif()
    set(${out} ${_v} PARENT_SCOPE)
endfunction()

set(_report "")
foreach(_name IN LISTS _figure_names)
    list(GET _mid_${_name} 0 _low)
    list(GET _mid_${_name} 1 _high)
    millionths("${_low}" _lo)
    millionths("${_high}" _hi)
    math(EXPR _published "(${_lo} + ${_hi}) / 2")
    math(EXPR _allowed "${_published} / 100")
    if(_allowed LESS 0)
        math(EXPR _allowed "-${_allowed}")
    endif()
    set(_min "")
    set(_max "")
    set(_values "")
    foreach(_p IN LISTS _platforms)
        if(NOT DEFINED _fig_${_name}_${_p})
            string(APPEND _failures "\n  ${_name}: ${_p} did not report it")
            continue()
        endif()
        millionths("${_fig_${_name}_${_p}}" _v)
        string(APPEND _values " ${_p}=${_fig_${_name}_${_p}}")
        if(_min STREQUAL "" OR _v LESS _min)
            set(_min ${_v})
        endif()
        if(_max STREQUAL "" OR _v GREATER _max)
            set(_max ${_v})
        endif()
    endforeach()
    math(EXPR _spread "${_max} - ${_min}")
    string(APPEND _report "\n  ${_name}: spread ${_spread} millionths, allowed ${_allowed};${_values}")
    if(_spread GREATER _allowed)
        string(APPEND _failures "\n  ${_name}: platforms disagree by more than 1% of the published value;${_values}")
    endif()
endforeach()

# The selftest's end.
foreach(_p IN LISTS _platforms)
    file(READ "${DIR}/${_p}.selftest.txt" _text)
    if(NOT _text MATCHES "ends at +([-0-9.]+), ([-0-9.]+), ([-0-9.]+) ft")
        string(APPEND _failures "\n  ${_p}: selftest output not understood")
        continue()
    endif()
    set(_lat_${_p} "${CMAKE_MATCH_1}")
    set(_lon_${_p} "${CMAKE_MATCH_2}")
    set(_alt_${_p} "${CMAKE_MATCH_3}")
    if(NOT _text MATCHES "heading ([-0-9.]+) deg")
        string(APPEND _failures "\n  ${_p}: no heading in the selftest output")
        continue()
    endif()
    set(_hdg_${_p} "${CMAKE_MATCH_1}")
    if(NOT _text MATCHES "airspeed +([-0-9.]+) KCAS")
        string(APPEND _failures "\n  ${_p}: no airspeed in the selftest output")
        continue()
    endif()
    set(_kcas_${_p} "${CMAKE_MATCH_1}")
    string(APPEND _report "\n  selftest ${_p}: ${_lat_${_p}}, ${_lon_${_p}}, ${_alt_${_p}} ft, heading ${_hdg_${_p}}, ${_kcas_${_p}} KCAS")
endforeach()

# Horizontal distance from the first platform, in feet: a degree of latitude is
# 364,000 ft and a degree of longitude at 34 degrees south about 302,000 ft,
# which is close enough to hold a 300 ft tolerance to within a percent.
list(GET _platforms 0 _ref)
millionths("${_lat_${_ref}}" _lat0)
millionths("${_lon_${_ref}}" _lon0)
millionths("${_alt_${_ref}}" _alt0)
millionths("${_hdg_${_ref}}" _hdg0)
millionths("${_kcas_${_ref}}" _kcas0)
foreach(_p IN LISTS _platforms)
    millionths("${_lat_${_p}}" _lat)
    millionths("${_lon_${_p}}" _lon)
    millionths("${_alt_${_p}}" _alt)
    millionths("${_hdg_${_p}}" _hdg)
    millionths("${_kcas_${_p}}" _kcas)
    # micro-degrees to feet: x 0.364 north, x 0.302 east
    math(EXPR _north "(${_lat} - ${_lat0}) * 364 / 1000")
    math(EXPR _east "(${_lon} - ${_lon0}) * 302 / 1000")
    math(EXPR _d2 "${_north} * ${_north} + ${_east} * ${_east}")
    math(EXPR _up "${_alt} - ${_alt0}")
    math(EXPR _dk "${_kcas} - ${_kcas0}")
    math(EXPR _dh "${_hdg} - ${_hdg0}")
    foreach(_v _up _dk _dh)
        if(${_v} LESS 0)
            math(EXPR ${_v} "-${${_v}}")
        endif()
    endforeach()
    if(_dh GREATER 180000000)
        math(EXPR _dh "360000000 - ${_dh}")
    endif()
    if(_d2 GREATER 90000 OR _up GREATER 30000000 OR _dk GREATER 1000000 OR _dh GREATER 2000000)
        string(APPEND _failures "\n  selftest ${_p} ends too far from ${_ref}: north ${_north} ft, east ${_east} ft, altitude ${_up} millionths ft, airspeed ${_dk} millionths kt, heading ${_dh} millionths deg")
    endif()
endforeach()

message(STATUS "cross-platform flights:${_report}")
if(_failures)
    message(FATAL_ERROR "the platforms do not fly alike:${_failures}")
endif()

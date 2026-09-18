# cross_platform_check.cmake - the cross-platform comparison accepts platforms
# that fly alike and refuses one that does not.
#
#   cmake -DPROGRAM=<glideslope_cli> -DROOT=<source dir> -DWORK=<scratch dir>
#         -P cross_platform_check.cmake
#
# Five "platforms" are made from this build's own figures and selftest output:
# identical, and with small differences well inside the tolerances. The check
# must accept them. Then, one at a time: a figure moved by 2%, the selftest's end
# moved 400 ft, one sample of the air 1e-8 m/s off, and a platform missing -
# each must be refused.

cmake_minimum_required(VERSION 3.28)

execute_process(COMMAND "${PROGRAM}" figures c172p OUTPUT_VARIABLE _figures RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope_cli figures c172p exited ${_rc}")
endif()
execute_process(COMMAND "${PROGRAM}" selftest OUTPUT_VARIABLE _selftest RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope_cli selftest exited ${_rc}")
endif()
execute_process(COMMAND "${PROGRAM}" air OUTPUT_VARIABLE _air RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope_cli air exited ${_rc}")
endif()

set(_platforms ubuntu-gcc rocky-gcc macos-appleclang windows-msvc windows-clang-cl)

function(write_platforms dir)
    file(REMOVE_RECURSE "${dir}")
    foreach(_p IN LISTS _platforms)
        file(WRITE "${dir}/${_p}.figures.txt" "${_figures}")
        file(WRITE "${dir}/${_p}.selftest.txt" "${_selftest}")
        file(WRITE "${dir}/${_p}.air.txt" "${_air}")
    endforeach()
    # One platform a little different: its climb rate 0.3% higher.
    string(REGEX MATCH "climb_rate +([0-9]+)\\.([0-9][0-9]) " _m "${_figures}")
    math(EXPR _hundredths "(${CMAKE_MATCH_1} * 100 + ${CMAKE_MATCH_2}) * 1003 / 1000")
    math(EXPR _int "${_hundredths} / 100")
    math(EXPR _frac "${_hundredths} % 100")
    if(_frac LESS 10)
        set(_frac "0${_frac}")
    endif()
    string(REGEX REPLACE "climb_rate +[0-9.]+ " "climb_rate ${_int}.${_frac} " _near "${_figures}")
    file(WRITE "${dir}/rocky-gcc.figures.txt" "${_near}")
endfunction()

function(expect label dir want)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDIR=${dir}"
                            -P "${ROOT}/tests/cmake/cross_platform_flights.cmake"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(want STREQUAL "ACCEPT" AND NOT _rc EQUAL 0)
        message(FATAL_ERROR "${label}: refused, should have been accepted\n${_out}${_err}")
    elseif(want STREQUAL "REFUSE" AND _rc EQUAL 0)
        message(FATAL_ERROR "${label}: accepted, should have been refused\n${_out}${_err}")
    endif()
    message(STATUS "${label}: ${want}ed as it should be")
endfunction()

write_platforms("${WORK}/agree")
expect("platforms that agree" "${WORK}/agree" ACCEPT)

write_platforms("${WORK}/figure")
# The CLI prints figures with two decimals, so the arithmetic is in hundredths.
string(REGEX MATCH "glide_ratio +([0-9]+)\\.([0-9][0-9]) " _m "${_figures}")
math(EXPR _hundredths "${CMAKE_MATCH_1} * 100 + ${CMAKE_MATCH_2}")
math(EXPR _moved "${_hundredths} * 102 / 100")
math(EXPR _moved_int "${_moved} / 100")
math(EXPR _moved_frac "${_moved} % 100")
if(_moved_frac LESS 10)
    set(_moved_frac "0${_moved_frac}")
endif()
string(REGEX REPLACE "glide_ratio +[0-9.]+ " "glide_ratio ${_moved_int}.${_moved_frac} " _off "${_figures}")
file(WRITE "${WORK}/figure/macos-appleclang.figures.txt" "${_off}")
expect("one figure 2% off" "${WORK}/figure" REFUSE)

write_platforms("${WORK}/air")
# The 500th sample's north wind, 1000 units of 1e-11 m/s - 1e-8 m/s - off.
string(REGEX MATCH "\nair 500 (-?[0-9]+) " _m "${_air}")
math(EXPR _moved "${CMAKE_MATCH_1} + 1000")
string(REGEX REPLACE "\nair 500 -?[0-9]+ " "\nair 500 ${_moved} " _off_air "${_air}")
file(WRITE "${WORK}/air/rocky-gcc.air.txt" "${_off_air}")
expect("one sample of the air 1e-8 m/s off" "${WORK}/air" REFUSE)

write_platforms("${WORK}/selftest")
string(REGEX MATCH "ends at +(-?[0-9]+)\\.([0-9]+)" _m "${_selftest}")
set(_lat_int "${CMAKE_MATCH_1}")
set(_lat_frac "${CMAKE_MATCH_2}")
# 0.0011 degrees of latitude is 400 ft.
math(EXPR _moved_frac "${_lat_frac} + 11000")
string(REGEX REPLACE "ends at +-?[0-9]+\\.[0-9]+" "ends at    ${_lat_int}.${_moved_frac}" _far "${_selftest}")
file(WRITE "${WORK}/selftest/windows-msvc.selftest.txt" "${_far}")
expect("the selftest ending 400 ft away" "${WORK}/selftest" REFUSE)

write_platforms("${WORK}/missing")
file(REMOVE "${WORK}/missing/windows-clang-cl.figures.txt")
expect("a platform missing" "${WORK}/missing" REFUSE)

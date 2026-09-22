# server_ai.cmake - the server runs the number of AI aircraft it is given.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -P server_ai.cmake
#
# **The item's own verification**: "a server runs the number it is given, and
# four when given none." Both halves are run here, and the count is read back
# out of the server rather than out of its settings, so that a server which
# printed a number and made a different number of aeroplanes would fail.
#
# The AI aircraft fly a plan over real terrain, so without the network this
# reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

# `asked` is what is put on the command line and `want` what must come back,
# written "asked:want" because a CMake list cannot hold a list. The case with
# nothing asked is the default, which the item says is four.
set(_cases "2:2" "1:1" ":4")
set(_walked 0)
foreach(_case IN LISTS _cases)
    string(REPLACE ":" ";" _pair "${_case}")
    list(GET _pair 0 _asked)
    list(GET _pair 1 _want)
    if(_asked STREQUAL "")
        set(_asked "(none)")
        set(_args "")
    else()
        set(_args --ai ${_asked})
    endif()

    execute_process(
        COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --seconds 1 ${_args}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(STATUS "the server could not run its AI aircraft: ${_err}")
        cmake_language(EXIT 77)
    endif()
    if(NOT _out MATCHES "ran ([0-9]+) AI aircraft")
        message(FATAL_ERROR "the server did not say how many AI aircraft it ran:\n${_out}")
    endif()
    if(NOT CMAKE_MATCH_1 EQUAL _want)
        message(FATAL_ERROR "asked for ${_asked} AI aircraft and it ran "
                            "${CMAKE_MATCH_1}, not ${_want}")
    endif()
    # And it really made them: one line per aeroplane it flew.
    string(REGEX MATCHALL "flew [^\n]+" _lines "${_out}")
    list(LENGTH _lines _count)
    if(NOT _count EQUAL _want)
        message(FATAL_ERROR "it said it ran ${_want} AI aircraft but reported "
                            "${_count} flying")
    endif()
    message(STATUS "asked ${_asked}, ran ${CMAKE_MATCH_1} AI aircraft")
    math(EXPR _walked "${_walked} + 1")
endforeach()

if(NOT _walked EQUAL 3)
    message(FATAL_ERROR "only ${_walked} of the three cases were run")
endif()
message(STATUS "the server runs the number of AI aircraft it is given, and four "
               "when given none")

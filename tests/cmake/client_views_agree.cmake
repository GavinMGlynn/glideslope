# client_views_agree.cmake - every view flew the same flight.
#
#   cmake -DWORK=<dir> -DDRIVER=<driver> -P client_views_agree.cmake
#
# **Changing the view steps nothing in the flight.** Each of the seven view
# tests (client_views.cmake) leaves in WORK a one-line account of the flight
# it flew to the shot: how many steps it traced, and the last state it traced.
# A trace line is numbered by the flight's own tick, so a view that stepped the
# flight differently would still have a line numbered for the shot's tick
# saying the same thing - it is where the flight had got to when the frame was
# shot, and after how many steps, that a view must not change.
#
# The seven are this test's fixture, so ctest runs them first wherever this
# test runs, and an account missing is a failure, never a skip.

cmake_minimum_required(VERSION 3.28)

set(_views cockpit ahead behind left right above orbit)
set(_first "")
set(_first_view "")
foreach(_view IN LISTS _views)
    set(_file "${WORK}/view-${DRIVER}-${_view}.flight")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "the ${_view} view left no account of its flight: ${_file}")
    endif()
    file(STRINGS "${_file}" _flight LIMIT_COUNT 1)
    if(_first STREQUAL "")
        set(_first "${_flight}")
        set(_first_view ${_view})
    elseif(NOT _flight STREQUAL _first)
        message(FATAL_ERROR "the ${_view} view flew a different flight from the "
                            "${_first_view} view:\n  ${_flight}\n  against\n  ${_first}")
    endif()
endforeach()
list(LENGTH _views _n)
message(STATUS "all ${_n} views flew the same flight: ${_first}")

# run_then_say_exited.cmake - runs a program, then writes a file saying it
# has exited, and how.
#
#   cmake -DEXITED=<file> -P run_then_say_exited.cmake -- <program> [args...]
#
# For a script that waits on a program started beside it - a stub's port
# file - and must not wait on one that has already gone: it waits for the
# port file or EXITED, whichever comes first (frame_hud.cmake). The
# program's standard output and error are its caller's; EXITED holds its
# exit code, written whole by renaming it into place.

cmake_minimum_required(VERSION 3.28)

set(_command "")
set(_after FALSE)
math(EXPR _last "${CMAKE_ARGC} - 1")
foreach(_i RANGE ${_last})
    if(_after)
        list(APPEND _command "${CMAKE_ARGV${_i}}")
    elseif(CMAKE_ARGV${_i} STREQUAL "--")
        set(_after TRUE)
    endif()
endforeach()
if(NOT DEFINED EXITED OR _command STREQUAL "")
    message(FATAL_ERROR "usage: cmake -DEXITED=<file> -P run_then_say_exited.cmake -- <program> [args...]")
endif()
execute_process(COMMAND ${_command} RESULT_VARIABLE _rc)
file(WRITE "${EXITED}.part" "${_rc}\n")
file(RENAME "${EXITED}.part" "${EXITED}")

# client.cmake - runs the client for a test.
#
#   include(client.cmake)
#   glideslope_client(<out-var> <argument>...)
#
# Runs PROGRAM with the arguments, fails the test if it exits non-zero, and
# puts what it printed in <out-var>.
#
# **Leaks, in the sanitized build.** A GPU driver or windowing library that
# the client loads at run time can leak, and is unloaded before the process
# exits, so LeakSanitizer reports allocations whose every frame is in a shared
# library or a module it can no longer name. Those are reported and ignored.
# A leak with any frame in the client itself - glideslope's code or SDL's,
# which is linked in - fails the test.

function(glideslope_client out)
    # Leaks are judged below rather than by LeakSanitizer's exit code.
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    execute_process(COMMAND "${PROGRAM}" ${ARGN}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope ${ARGN} exited ${_rc}\n${_out}${_err}")
    endif()

    string(REPLACE ";" "," _text "${_err}")
    string(REPLACE "[" "<" _text "${_text}")
    string(REPLACE "]" ">" _text "${_text}")
    string(REPLACE "\n" ";" _lines "${_text}")
    set(_block "")
    set(_ours OFF)
    set(_foreign 0)
    set(_report "")
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "(Direct|Indirect) leak of")
            if(_ours)
                string(APPEND _report "${_block}\n")
            elseif(NOT _block STREQUAL "")
                math(EXPR _foreign "${_foreign} + 1")
            endif()
            set(_block "${_line}\n")
            set(_ours OFF)
        elseif(NOT _block STREQUAL "" AND _line MATCHES "^ *#([0-9]+) ")
            string(APPEND _block "${_line}\n")
            if(NOT CMAKE_MATCH_1 EQUAL 0
               AND NOT _line MATCHES "\\(<unknown module>\\) *$"
               AND NOT _line MATCHES "\\(/[^()]*\\.so[.0-9]*\\+0x[0-9a-f]+\\)")
                set(_ours ON)
            endif()
        endif()
    endforeach()
    if(_ours)
        string(APPEND _report "${_block}\n")
    elseif(NOT _block STREQUAL "")
        math(EXPR _foreign "${_foreign} + 1")
    endif()
    if(NOT _report STREQUAL "")
        message(FATAL_ERROR "glideslope ${ARGN} leaked, with frames in glideslope:\n${_report}")
    endif()
    if(_foreign GREATER 0)
        message(STATUS "${_foreign} leak reports lie wholly inside libraries loaded at run time; not glideslope's")
    endif()
    set(${out} "${_out}" PARENT_SCOPE)
endfunction()

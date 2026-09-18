# client.cmake - runs the client for a test.
#
#   include(client.cmake)
#   glideslope_client(<out-var> <argument>...)
#
# Runs PROGRAM with the arguments, fails the test if it exits non-zero, and
# puts what it printed in <out-var>.
#
# **Leaks, in the sanitized build.** A GPU driver or windowing library that
# the client loads at run time can leak - Xlib keeps its resource database and
# input method, Mesa a worker thread's state - and is unloaded before the
# process exits. What decides whose leak it is is who allocated it: the first
# frame of the stack outside the sanitizer's and the C and C++ runtimes' - a
# frame LeakSanitizer could name, since frames of an unloaded library are
# sometimes mislabelled as the sanitizer's, at offsets far past its end. If
# that frame is in a shared library, or a module LeakSanitizer can no longer
# name, the leak is the library's, reported and ignored, even when glideslope or
# SDL called the library. So is a frame given as the client's with no function
# named: the client is built with its symbols, and its frames are named, but a
# driver unloaded before the report - Mesa's lavapipe, whose threads start
# straight from the sanitizer - is sometimes given as the client, at offsets
# past the end of its image. If the frame is in the client itself -
# glideslope's code, SDL's, Cesium Native's, or a standard container inlined
# into any of them - the test fails.

# Judges the leak reports in a run's standard error, as described above: fails
# the test for a leak allocated in the client, and says how many were not.
function(glideslope_judge_leaks stderr_text)
    string(REPLACE ";" "," _text "${stderr_text}")
    string(REPLACE "[" "<" _text "${_text}")
    string(REPLACE "]" ">" _text "${_text}")
    string(REPLACE "\n" ";" _lines "${_text}")
    set(_block "")
    set(_ours OFF)
    set(_decided OFF)
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
            set(_decided OFF)
        elseif(NOT _block STREQUAL "" AND _line MATCHES "^ *#([0-9]+) ")
            string(APPEND _block "${_line}\n")
            if(NOT _decided AND NOT CMAKE_MATCH_1 EQUAL 0
               AND NOT (_line MATCHES " in [^ ]"
                        AND _line MATCHES "libsanitizer|libasan|liblsan|libubsan|libstdc\\+\\+|libc\\+\\+|/libc\\.so|libgcc_s"))
                set(_decided ON)
                if(NOT _line MATCHES "\\(<unknown module>\\) *$"
                   AND NOT _line MATCHES "\\(/[^()]*\\.so[.0-9]*\\+0x[0-9a-f]+\\)"
                   AND NOT _line MATCHES "^ *#[0-9]+ 0x[0-9a-f]+ +\\(")
                    set(_ours ON)
                endif()
            endif()
        endif()
    endforeach()
    if(_ours)
        string(APPEND _report "${_block}\n")
    elseif(NOT _block STREQUAL "")
        math(EXPR _foreign "${_foreign} + 1")
    endif()
    if(NOT _report STREQUAL "")
        message(FATAL_ERROR "glideslope leaked, with frames in glideslope:\n${_report}")
    endif()
    if(_foreign GREATER 0)
        message(STATUS "${_foreign} leak reports were allocated by libraries loaded at run time; not glideslope's")
    endif()
endfunction()

function(glideslope_client out)
    # Leaks are judged below rather than by LeakSanitizer's exit code.
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    execute_process(COMMAND "${PROGRAM}" ${ARGN}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope ${ARGN} exited ${_rc}\n${_out}${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
    set(${out} "${_out}" PARENT_SCOPE)
endfunction()

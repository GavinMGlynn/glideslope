# expect_run.cmake - run a program, and check both how it exits and what it says.
#
# ctest's PASS_REGULAR_EXPRESSION ignores the exit code, so a program that
# printed the right thing and then failed would pass under it. This checks both.
#
#   cmake -DPROGRAM=<path> [-DARGS="<args>"] -DEXIT=<code>
#         [-DSTDOUT_IS=<text>] [-DSTDOUT_MATCHES=<regex>]
#         [-DSTDERR_MATCHES=<regex>] -P expect_run.cmake
#
# STDOUT_IS compares the whole of standard output, less one trailing newline.
# Windows line endings are folded to "\n" first, so the same expectation holds on
# every platform.

if(NOT DEFINED PROGRAM OR NOT DEFINED EXIT)
    message(FATAL_ERROR "expect_run.cmake needs PROGRAM and EXIT")
endif()

separate_arguments(_args NATIVE_COMMAND "${ARGS}")
execute_process(COMMAND "${PROGRAM}" ${_args}
                RESULT_VARIABLE _exit
                OUTPUT_VARIABLE _out
                ERROR_VARIABLE  _err)

string(REPLACE "\r\n" "\n" _out "${_out}")
string(REPLACE "\r\n" "\n" _err "${_err}")

set(_failures "")
if(NOT "${_exit}" STREQUAL "${EXIT}")
    list(APPEND _failures "exit code: expected ${EXIT}, got ${_exit}")
endif()
if(DEFINED STDOUT_IS)
    string(REGEX REPLACE "\n$" "" _out_line "${_out}")
    if(NOT "${_out_line}" STREQUAL "${STDOUT_IS}")
        list(APPEND _failures "stdout: expected exactly '${STDOUT_IS}'")
    endif()
endif()
if(DEFINED STDOUT_MATCHES AND NOT "${_out}" MATCHES "${STDOUT_MATCHES}")
    list(APPEND _failures "stdout: expected to match '${STDOUT_MATCHES}'")
endif()
if(DEFINED STDERR_MATCHES AND NOT "${_err}" MATCHES "${STDERR_MATCHES}")
    list(APPEND _failures "stderr: expected to match '${STDERR_MATCHES}'")
endif()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR
        "${PROGRAM} ${ARGS}\n  ${_report}\n"
        "--- stdout ---\n${_out}--- stderr ---\n${_err}")
endif()

# impair_gives_up.cmake - the relay, told to stop when its input ends, says so
# when it stops for the time instead.
#
#   cmake -DIMPAIR=<glideslope_impair> -P impair_gives_up.cmake
#
# It listens on a port the system chooses (0): nothing is sent to it.
#
# Last in a network test's pipeline, the relay stops when the server before it
# has gone. Were the server still running when the relay's --seconds ran out,
# it once died of SIGPIPE for it, which failed the test; now every program
# ignores SIGPIPE (platform/closed_pipes.hpp), so the relay must fail it
# instead, with exit code 1.
#
# **Built, not hoped for**: its input is held open by `cmake -E sleep 5` for
# five times the one second it is given - and, second, is ended at once by
# `cmake -E true`, which must still be an exit of 0.

cmake_minimum_required(VERSION 3.28)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E sleep 5
    COMMAND "${IMPAIR}" 0 "127.0.0.1:9" --delay 0 --jitter 0 --loss 0 --seed 1
            --until-input-ends --seconds 1
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;1" OR NOT _out MATCHES "gave up after 1 s with its input still open")
    message(FATAL_ERROR "with its input open past its time, the relay exited ${_rcs}, not "
                        "0;1:\n${_out}${_err}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E true
    COMMAND "${IMPAIR}" 0 "127.0.0.1:9" --delay 0 --jitter 0 --loss 0 --seed 1
            --until-input-ends --seconds 60
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0" OR _out MATCHES "gave up")
    message(FATAL_ERROR "with its input ended, the relay exited ${_rcs}, not 0;0:\n"
                        "${_out}${_err}")
endif()
message(STATUS "the relay fails when it gives up with its input open, and not when it ends")

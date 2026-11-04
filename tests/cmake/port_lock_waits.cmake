# port_lock_waits.cmake - a second run of a test on a port another run holds
# waits for it, and a script the holder runs on the same port does not.
#
#   cmake -DWORK=<scratch> -P port_lock_waits.cmake
#
# **Built, not hoped for**: two cmake processes started together, each in a
# role of port_lock_party.cmake, on a port no test listens on (1, below the
# tests' block and every server's). The holder takes the port's lock through
# port_lock.cmake and says so; only then does the waiter try it, without
# waiting, and must be refused; it tells the holder to go on, and waits for
# the lock through port_lock.cmake as a test does. The holder, still
# holding, runs a script of its own on the same port, which must take the
# lock at once - it is the holder's - rather than wait on its own parent;
# then the holder says it is done and ends. The waiter must have the lock
# only after that.
#
# A deadlock there - a nested script waiting on its parent - is a hang, which
# the test's TIMEOUT ends. Each party holds PORT + 1 as well, as a test holds
# the relay port it derives (port_lock_hold), and the waiter is refused that
# too while the holder runs.
#
# **And port_listened fails a test whose program could not listen**: the
# server's words, the relay's, and a script's run by a script each fail it,
# naming the port; a program that listened passes.

cmake_minimum_required(VERSION 3.28)

foreach(_v WORK)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "port_lock_waits.cmake needs -D${_v}")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(_party "${CMAKE_CURRENT_LIST_DIR}/port_lock_party.cmake")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPORT=1 -DROLE=holder -DWORK=${WORK} -P "${_party}"
    COMMAND "${CMAKE_COMMAND}" -DPORT=1 -DROLE=waiter -DWORK=${WORK} -P "${_party}"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}${_err}")
list(GET _rcs 0 _holder_rc)
list(GET _rcs 1 _waiter_rc)
if(NOT _holder_rc EQUAL 0 OR NOT _waiter_rc EQUAL 0)
    message(FATAL_ERROR "the holder ended ${_holder_rc} and the waiter ${_waiter_rc}:\n"
                        "${_out}${_err}")
endif()
foreach(_said IN ITEMS "holder: held 1 and its relay 2"
                       "waiter: refused both while they were held"
                       "nested: took it and its relay at once, under its holder"
                       "waiter: had both only after the holder was done")
    string(FIND "${_out}${_err}" "${_said}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "nobody said '${_said}':\n${_out}${_err}")
    endif()
endforeach()
message(STATUS "a port held, and its relay, were waited for, by a second run and not "
               "by the holder's own")

foreach(_case IN ITEMS "glideslope_server: cannot listen on port 7=fails"
                       "impair: cannot listen on port 7=fails"
                       "the server could not listen on port 7=fails"
                       "glideslope_server: listening on port 7=passes")
    string(REGEX REPLACE "=[a-z]+$" "" _said "${_case}")
    string(REGEX REPLACE "^.*=" "" _expected "${_case}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DPORT=1 -DROLE=listened "-DSAID=${_said}" -P "${_party}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(_expected STREQUAL "fails")
        if(_rc EQUAL 0 OR NOT _err MATCHES "could not listen on port 7")
            message(FATAL_ERROR "port_listened passed '${_said}' (${_rc}):\n${_out}${_err}")
        endif()
    elseif(NOT _rc EQUAL 0)
        message(FATAL_ERROR "port_listened failed '${_said}' (${_rc}):\n${_out}${_err}")
    endif()
endforeach()
message(STATUS "port_listened failed the three ways of not listening, and passed one that listened")

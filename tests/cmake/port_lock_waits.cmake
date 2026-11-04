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
# the test's TIMEOUT ends.

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
foreach(_said IN ITEMS "holder: held" "waiter: refused while it was held"
                       "nested: took it at once, under its holder"
                       "waiter: had it only after the holder was done")
    string(FIND "${_out}${_err}" "${_said}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "nobody said '${_said}':\n${_out}${_err}")
    endif()
endforeach()
message(STATUS "a port held was waited for, by a second run and not by the holder's own")

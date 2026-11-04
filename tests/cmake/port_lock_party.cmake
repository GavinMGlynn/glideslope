# port_lock_party.cmake - one party to port_lock_waits.cmake: the holder,
# the waiter, or the script the holder runs on its own port (nested).
#
#   cmake -DPORT=<port> -DROLE=holder|waiter|nested -DWORK=<scratch>
#         -P port_lock_party.cmake

cmake_minimum_required(VERSION 3.28)

# Waits on a file another party writes: an event, not a time.
function(wait_for _file)
    while(NOT EXISTS "${_file}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
endfunction()

if(ROLE STREQUAL "holder")
    include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
    file(WRITE "${WORK}/held" "${_port_lock}")
    message(NOTICE "holder: held ${_port_lock}")
    wait_for("${WORK}/go")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DPORT=${PORT} -DROLE=nested -DWORK=${WORK}
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    message(NOTICE "${_out}${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "holder: the nested script ended ${_rc}")
    endif()
    # Said before this process ends, which is when the lock is let go.
    file(WRITE "${WORK}/holder-done" "")
    message(NOTICE "holder: done")
elseif(ROLE STREQUAL "nested")
    include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
    message(NOTICE "nested: took it at once, under its holder")
elseif(ROLE STREQUAL "waiter")
    wait_for("${WORK}/held")
    file(READ "${WORK}/held" _held)
    file(LOCK "${_held}" GUARD PROCESS TIMEOUT 0 RESULT_VARIABLE _rc)
    # The holder is told to go on whatever this found, so that a failure
    # here ends the test rather than leaving the holder waiting.
    file(WRITE "${WORK}/go" "")
    if(_rc EQUAL 0)
        message(FATAL_ERROR "waiter: took ${_held} while the holder held it")
    endif()
    message(NOTICE "waiter: refused while it was held (${_rc})")
    include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
    if(NOT EXISTS "${WORK}/holder-done")
        message(FATAL_ERROR "waiter: had the lock before the holder was done")
    endif()
    message(NOTICE "waiter: had it only after the holder was done")
else()
    message(FATAL_ERROR "port_lock_party.cmake: ROLE is holder, waiter or nested, not '${ROLE}'")
endif()

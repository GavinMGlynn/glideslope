# port_lock_party.cmake - one party to port_lock_waits.cmake: the holder,
# the waiter, or the script the holder runs on its own port (nested). Each
# holds PORT, and PORT + 1 as a test holds a relay port it derives.
#
#   cmake -DPORT=<port> -DROLE=holder|waiter|nested -DWORK=<scratch>
#         -P port_lock_party.cmake
#   cmake -DPORT=<port> -DROLE=listened -DSAID=<a program's words> -P port_lock_party.cmake

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
    math(EXPR _relay "${PORT} + 1")
    port_lock_hold(${_relay})
    # Twice, as a loop holds its port each time round: the second is nothing.
    port_lock_hold(${_relay})
    file(WRITE "${WORK}/held-relay" "${_port_lock}")
    message(NOTICE "holder: held ${PORT} and its relay ${_relay}")
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
    math(EXPR _relay "${PORT} + 1")
    port_lock_hold(${_relay})
    message(NOTICE "nested: took it and its relay at once, under its holder")
elseif(ROLE STREQUAL "waiter")
    wait_for("${WORK}/held-relay")
    file(READ "${WORK}/held" _held)
    file(READ "${WORK}/held-relay" _held_relay)
    file(LOCK "${_held}" GUARD PROCESS TIMEOUT 0 RESULT_VARIABLE _rc)
    file(LOCK "${_held_relay}" GUARD PROCESS TIMEOUT 0 RESULT_VARIABLE _relay_rc)
    # The holder is told to go on whatever this found, so that a failure
    # here ends the test rather than leaving the holder waiting.
    file(WRITE "${WORK}/go" "")
    if(_rc EQUAL 0)
        message(FATAL_ERROR "waiter: took ${_held} while the holder held it")
    endif()
    if(_relay_rc EQUAL 0)
        message(FATAL_ERROR "waiter: took the relay's ${_held_relay} while the holder held it")
    endif()
    message(NOTICE "waiter: refused both while they were held (${_rc}; ${_relay_rc})")
    include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
    math(EXPR _relay "${PORT} + 1")
    port_lock_hold(${_relay})
    if(NOT EXISTS "${WORK}/holder-done")
        message(FATAL_ERROR "waiter: had the locks before the holder was done")
    endif()
    message(NOTICE "waiter: had both only after the holder was done")
elseif(ROLE STREQUAL "listened")
    # What a test does with what its programs said, SAID standing for it.
    include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
    port_listened("a client's words" "${SAID}")
    message(NOTICE "listened: passed")
else()
    message(FATAL_ERROR "port_lock_party.cmake: ROLE is holder, waiter, nested or listened, "
                        "not '${ROLE}'")
endif()

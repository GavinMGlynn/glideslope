# port_lock.cmake - a test that listens on its fixed port holds that port's
# lock, machine-wide, for as long as it runs; a second run of a test on the
# same port, from another build or working copy, waits for it.
#
#   include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)   # with PORT defined
#
# **Why**: no two tests of one build share a port (test_ports.cmake), but two
# builds on one machine are the same tests on the same ports. On 2026-11-04 a
# full linux-release suite in one working copy ran the spiral hand-over test
# while another agent's ctest, in another working copy, ran the same test on
# the same port, 24940. The second server could not listen ("cannot listen on
# port 24940"), and its client's initiation reached the first one's server,
# whose key is another: refused BAD_HANDSHAKE ("refused, reason 6") 2.5 s
# into a 125 s test. The lock makes the second run wait instead.
#
# **The lock is the process's** (`GUARD PROCESS`): it is let go when this
# cmake -P ends, however it ends, and a test killed by its timeout holds
# nothing after. It waits with no time limit of its own: the test's timeout
# is the bound, and nothing gives up sooner than a slow machine needs.
#
# **A script run by a script on the same port** (client_server_gone.cmake
# runs server_started_again.cmake) does not take it again: the holder lists
# the ports it holds in GLIDESLOPE_PORT_LOCK_HELD, which its children
# inherit, and a second wait there would be on itself.
#
# **A port derived from PORT** (a relay at PORT + K) is held the same way,
# by `port_lock_hold(${_relay})` straight after the math() that makes it:
# two working copies whose port numbering differs - one test's relay port
# another version's test's own - wait for each other there too. Holding a
# port this process already holds is nothing, so a loop may hold its port
# each time round.
#
# **And a test whose server cannot listen fails as that** (`port_listened`),
# before anything its client heard is judged: a server that could not listen
# leaves its client talking to whatever holds the port, and the client's
# refusal names the wrong fault (the spiral hand-over test, 2026-11-04:
# "refused, reason 6", another run's server's answer). Called straight after
# each execute_process that runs a program on a held port, with every stream
# that call caught:
#
#   port_listened("${_out}" "${_err}")
#
# The server and the relay (glideslope_impair) both say "cannot listen on
# port N"; a script run by a script says "could not listen on port N" through
# this, and its parent's check reads that too. test_ports.cmake checks that
# every call that listens on a held port is followed by this before any other
# check.

# One place for every working copy on the machine: the system's temporary
# directory, not one of the build's own - TEMP, else TMP, else /tmp.
function(port_lock_hold _port_lock_port)
    if(NOT _port_lock_port MATCHES "^[0-9]+$")
        message(FATAL_ERROR "port_lock_hold needs a port, a number, not '${_port_lock_port}'")
    endif()
    # Held already by this process, or by the script that runs it.
    string(REPLACE "," ";" _port_lock_held "$ENV{GLIDESLOPE_PORT_LOCK_HELD}")
    if("${_port_lock_port}" IN_LIST _port_lock_held)
        return()
    endif()
    set(_port_lock_dir "")
    foreach(_port_lock_try IN ITEMS "$ENV{TEMP}" "$ENV{TMP}" "/tmp")
        if(NOT _port_lock_try STREQUAL "" AND IS_DIRECTORY "${_port_lock_try}")
            set(_port_lock_dir "${_port_lock_try}")
            break()
        endif()
    endforeach()
    if(_port_lock_dir STREQUAL "")
        message(FATAL_ERROR "port_lock.cmake: none of TEMP ('$ENV{TEMP}'), TMP ('$ENV{TMP}') "
                            "or /tmp is a directory to hold port ${_port_lock_port}'s lock in")
    endif()
    set(_port_lock "${_port_lock_dir}/glideslope-test-port-${_port_lock_port}.lock")
    file(LOCK "${_port_lock}" GUARD PROCESS TIMEOUT 0 RESULT_VARIABLE _port_lock_rc)
    if(NOT _port_lock_rc EQUAL 0)
        message(STATUS "port ${_port_lock_port} is held by another run of a test on it "
                       "(${_port_lock}); waiting for it")
        file(LOCK "${_port_lock}" GUARD PROCESS RESULT_VARIABLE _port_lock_rc)
        if(NOT _port_lock_rc EQUAL 0)
            message(FATAL_ERROR "could not take ${_port_lock}: ${_port_lock_rc}")
        endif()
        message(STATUS "port ${_port_lock_port} is ours now")
    endif()
    list(APPEND _port_lock_held "${_port_lock_port}")
    list(JOIN _port_lock_held "," _port_lock_held)
    set(ENV{GLIDESLOPE_PORT_LOCK_HELD} "${_port_lock_held}")
    set(_port_lock "${_port_lock}" PARENT_SCOPE)
endfunction()

# Fails if any of the texts says a program could not listen on its port.
function(port_listened)
    string(JOIN "\n" _port_listened_said ${ARGN})
    if(_port_listened_said MATCHES "(cannot|could not) listen on port ([0-9]+)")
        message(FATAL_ERROR "a program could not listen on port ${CMAKE_MATCH_2}: something "
                            "else holds it, and what a client heard there is not this "
                            "test's server's:\n${_port_listened_said}")
    endif()
endfunction()

if(NOT DEFINED PORT OR NOT PORT MATCHES "^[0-9]+$")
    message(FATAL_ERROR "port_lock.cmake needs PORT, a number, not '${PORT}'")
endif()
port_lock_hold(${PORT})

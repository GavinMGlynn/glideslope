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
# runs server_started_again.cmake) does not take it again: the holder says
# which port it holds in GLIDESLOPE_PORT_LOCK_HELD, which its children
# inherit, and a second wait there would be on itself.
#
# Ports derived from PORT (a relay at PORT + K) are not locked apart: two runs
# of one test are kept apart by PORT's lock, and within a build no two tests
# share any port. Two working copies whose port numbering differs - one test's
# relay port another version's test's own - are not covered.

if(NOT DEFINED PORT OR NOT PORT MATCHES "^[0-9]+$")
    message(FATAL_ERROR "port_lock.cmake needs PORT, a number, not '${PORT}'")
endif()

if(NOT "$ENV{GLIDESLOPE_PORT_LOCK_HELD}" STREQUAL "${PORT}")
    # One place for every working copy on the machine: the system's temporary
    # directory, not one of the build's own - TEMP, else TMP, else /tmp.
    set(_port_lock_dir "")
    foreach(_port_lock_try IN ITEMS "$ENV{TEMP}" "$ENV{TMP}" "/tmp")
        if(NOT _port_lock_try STREQUAL "" AND IS_DIRECTORY "${_port_lock_try}")
            set(_port_lock_dir "${_port_lock_try}")
            break()
        endif()
    endforeach()
    if(_port_lock_dir STREQUAL "")
        message(FATAL_ERROR "port_lock.cmake: none of TEMP ('$ENV{TEMP}'), TMP "
                            "('$ENV{TMP}') or /tmp is a directory to hold port ${PORT}'s lock in")
    endif()
    set(_port_lock "${_port_lock_dir}/glideslope-test-port-${PORT}.lock")
    file(LOCK "${_port_lock}" GUARD PROCESS TIMEOUT 0 RESULT_VARIABLE _port_lock_rc)
    if(NOT _port_lock_rc EQUAL 0)
        message(STATUS "port ${PORT} is held by another run of a test on it "
                       "(${_port_lock}); waiting for it")
        file(LOCK "${_port_lock}" GUARD PROCESS RESULT_VARIABLE _port_lock_rc)
        if(NOT _port_lock_rc EQUAL 0)
            message(FATAL_ERROR "could not take ${_port_lock}: ${_port_lock_rc}")
        endif()
        message(STATUS "port ${PORT} is ours now")
    endif()
    set(ENV{GLIDESLOPE_PORT_LOCK_HELD} "${PORT}")
endif()

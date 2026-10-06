# server_started_again.cmake - the server of client_server_gone.cmake, run
# so that what it says is heard. Not a test: the middle program of that
# test's pipeline.
#
#   cmake -DSERVER=<glideslope_server> -DPORT=<its port> -DDATA=<data>
#         -DSTORE=<its store> -DREADY=<ready file> -DMODE=restart|lost
#         -P server_started_again.cmake
#
# **restart**: a server, and then the same server started again on its port
# with its key, under the client flying on it. The first stops, telling
# nobody, once it has flown a player's input and twenty seconds of its clock
# have gone (`--stop-once-flown 20`) - so that the second's clock, started
# again from nought, is behind where the first's was; the
# second is started only when the first has gone - they run one after the
# other here - and stops once everybody who joined it has gone
# (`--until-empty`). Events, not times.
#
# **lost**: one server that drops the first player it has flown, sending no
# goodbye (`--drop-once-flown --lose-goodbyes`), and stops once everybody who
# joined has gone.
#
# **What they say goes to standard error**, once each has gone: in the
# test's pipeline this script's standard output is the client's standard
# input, where it would be lost.

cmake_minimum_required(VERSION 3.28)

if(MODE STREQUAL "restart")
    execute_process(
        COMMAND "${SERVER}" --port ${PORT} --seconds 300 --ai 1 --headless --data "${DATA}"
                --timeout 3 --store "${STORE}" --ready-file "${READY}" --stop-once-flown 20
        RESULT_VARIABLE _first OUTPUT_VARIABLE _said ERROR_VARIABLE _said)
    message(NOTICE "${_said}")
    execute_process(
        COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
                --data "${DATA}" --timeout 3 --store "${STORE}"
        RESULT_VARIABLE _second OUTPUT_VARIABLE _said ERROR_VARIABLE _said)
    message(NOTICE "${_said}")
    if(NOT _first EQUAL 0 OR NOT _second EQUAL 0)
        message(FATAL_ERROR "the servers exited ${_first} and ${_second}")
    endif()
elseif(MODE STREQUAL "lost")
    execute_process(
        COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
                --data "${DATA}" --timeout 3 --store "${STORE}" --ready-file "${READY}"
                --drop-once-flown --lose-goodbyes
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _said ERROR_VARIABLE _said)
    message(NOTICE "${_said}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the server exited ${_rc}")
    endif()
else()
    message(FATAL_ERROR "server_started_again.cmake: MODE is restart or lost, not '${MODE}'")
endif()

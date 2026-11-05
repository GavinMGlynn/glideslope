# server_rejoin_layer.cmake - a player who joins after another has left is
# put on a layer no aircraft still near the start holds.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_rejoin_layer.cmake
#
# **Built, not hoped for.** A server with two AI aircraft on the plan file
# and room for two players, whose aircraft is taken out of the sky when its
# player goes (`--on-leave remove`, the default). Three clients:
# - A joins at once and leaves 6 s later;
# - B joins 1 s in and stays 40 s, untouched, so it is still within 1.5 nm
#   of the start when C joins;
# - C joins 12 s in, after A has gone, and stays 10 s.
# Each layer was once worked out from a count of aircraft. With A's aircraft
# gone from the sky, C was counted onto B's layer, at the start: the two
# AI aircraft and B make three, so C went on layer four, which is B's.
# **Checked**: the server says where each of the three starts, and no two
# share a layer - C's above B's - and nothing is wrecked.
#
# Without the DEM's tiles it reports itself skipped (exit 77).

cmake_minimum_required(VERSION 3.28)

include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/rejoin-layer.sqlite")
file(REMOVE "${_store}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 6
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 40 --after 1
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 10 --after 12
    COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --players 2 --headless
            --data "${DATA}" --store "${_store}" --ai 2
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")
if(_out MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was wrecked:\n${_out}")
endif()
string(REGEX MATCHALL "a player's aircraft starts on layer [0-9]+" _starts "${_out}")
list(LENGTH _starts _count)
if(NOT _count EQUAL 3)
    message(FATAL_ERROR "${_count} players' starts were said, not the three joins:\n${_out}")
endif()
set(_layers)
foreach(_start IN LISTS _starts)
    string(REGEX REPLACE ".* layer " "" _layer "${_start}")
    list(APPEND _layers "${_layer}")
endforeach()
list(GET _layers 1 _b)
list(GET _layers 2 _c)
message(STATUS "A, B and C started on layers ${_layers}")
if(_c EQUAL _b)
    message(FATAL_ERROR "C, joining after A left, was put on B's layer ${_b}, at the start "
                        "while B was still near it:\n${_out}")
endif()
if(NOT _rcs STREQUAL "0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
message(STATUS "a player joining after another left was put on a layer of their own")

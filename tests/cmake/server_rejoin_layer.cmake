# server_rejoin_layer.cmake - a player who joins after another has left is
# put on a layer no aircraft still near the start holds.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_rejoin_layer.cmake
#
# **Built, not hoped for**, on events, not on a clock. A server with two AI
# aircraft on the plan file and room for two players, whose aircraft is
# taken out of the sky when its player goes (`--on-leave remove`, the
# default). Three clients, each waiting on the one before:
# - A joins once the server is flying (`--after-ready` its `--ready-file`),
#   and leaves once B is in the session (`--until-exists` B's
#   `--told-file`), writing a file when it has gone (`--done`);
# - B joins once A is in (`--after-ready` A's `--told-file`), and stays,
#   untouched, until C is in - so it is still within 1.5 nm of the start
#   when C joins;
# - C joins once A has gone (`--after-ready` A's `--done` file), and leaves
#   once it is in.
# Its joins were once on a clock - A for 6 s, B 1 s in, C 12 s in - and a
# debug server that took 7 s to start flying on a loaded runner (nightly run
# 43507588675) still had A when C knocked: C was refused, the server full.
# Each layer was once worked out from a count of aircraft. With A's aircraft
# gone from the sky, C was counted onto B's layer, at the start: the two
# AI aircraft and B make three, so C went on layer four, which is B's.
# **Checked**: the situation was built - A, B and C started in that order,
# with A let go between B's start and C's - and no two share a layer, C's
# above B's, and nothing is wrecked.
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

set(_ready "${WORK}/ready.txt")
set(_a_in "${WORK}/a-in.txt")
set(_a_gone "${WORK}/a-gone.txt")
set(_b_in "${WORK}/b-in.txt")
set(_c_in "${WORK}/c-in.txt")
file(REMOVE "${_ready}" "${_a_in}" "${_a_gone}" "${_b_in}" "${_c_in}")
execute_process(
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_ready}" --told-file "${_a_in}" --until-exists "${_b_in}"
            --done "${_a_gone}"
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_a_in}" --told-file "${_b_in}" --until-exists "${_c_in}"
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_a_gone}" --told-file "${_c_in}" --until-exists "${_c_in}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --players 2 --headless
            --data "${DATA}" --store "${_store}" --ai 2 --ready-file "${_ready}"
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
# **The situation, built**: the server's starts and lettings-go in the order
# it said them - B's start, then A let go, then C's: C joined with A gone and
# B still there, the case the rule is for.
string(REGEX MATCHALL "(a player's aircraft starts on layer|let go)" _said "${_out}")
list(TRANSFORM _said REPLACE "a player's aircraft starts on layer" "start")
list(TRANSFORM _said REPLACE "let go" "gone")
list(SUBLIST _said 0 4 _first_four)
if(NOT _first_four STREQUAL "start;start;gone;start")
    message(FATAL_ERROR "the joins and leavings came as ${_said}, not A and B in, A gone, "
                        "then C in:\n${_out}")
endif()
set(_layers)
foreach(_start IN LISTS _starts)
    string(REGEX REPLACE ".* layer " "" _layer "${_start}")
    list(APPEND _layers "${_layer}")
endforeach()
list(GET _layers 0 _a)
list(GET _layers 1 _b)
list(GET _layers 2 _c)
message(STATUS "A, B and C started on layers ${_layers}")
if(_b EQUAL _a)
    message(FATAL_ERROR "B was put on A's layer ${_a}:\n${_out}")
endif()
if(NOT _c GREATER _b)
    message(FATAL_ERROR "C, joining after A left, was put on layer ${_c}, not above B's "
                        "${_b}, at the start while B was still near it:\n${_out}")
endif()
if(NOT _rcs STREQUAL "0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
message(STATUS "a player joining after another left was put on a layer of their own")

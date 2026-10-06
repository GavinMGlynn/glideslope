# server_ground.cmake - a client told the server's collision ground refuses
# other ground than its own and leaves; a client on the same ground stays.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DDATA=<data dir> -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_ground.cmake
#
# **Other ground.** A client whose data says the collision ground is
# other than the server's - its runway strips a line longer - is told the
# server's, says it refuses it, naming both, and leaves with exit 1; a client
# on the same ground is told it and stays.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.
cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/weather.sqlite")
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

# Sydney Airport, where the AI's plan flies.

# Other ground: this build's coverage and a runway strips file a line
# longer - all the client reads to know its ground.
set(_other "${WORK}/other-data")
file(REMOVE_RECURSE "${_other}")
file(MAKE_DIRECTORY "${_other}/dem" "${_other}/runways")
file(COPY "${DATA}/dem/coverage.txt" DESTINATION "${_other}/dem")
file(READ "${DATA}/runways/strips.csv" _strips)
file(WRITE "${_other}/runways/strips.csv" "${_strips}\n")
set(_refused "${WORK}/refused.txt")
set(_kept "${WORK}/kept.txt")
set(_ready "${WORK}/ready.txt")
set(_done "${WORK}/done.txt")
file(REMOVE "${_refused}" "${_kept}" "${_ready}" "${_done}")
execute_process(
    # Each joins once the server is flying (--ready-file), not a second after
    # it was started: a debug server on CI's macOS was not yet answering, and
    # the client on the same ground, staying three seconds, left unanswered.
    COMMAND "${CLIENT}" --data "${_other}" connect "127.0.0.1:${PORT}" "${_key}" 60
            --after 0 --after-ready "${_ready}" --heard "${_refused}" --done "${_done}"
    # **And the one on the same ground stays until it has been told the ground
    # and answered a knock** (`--until-told-ground`), not five seconds: a
    # server slow on CI's macOS had said nothing by then, and the client left
    # untold, or having answered nothing, exit 1 (reproduced with the server
    # held to three seconds a step, `--test-step-ms 3000`). **And until the
    # client on other ground is done** (`--until-exists`): leaving as soon as
    # it was told, it emptied the server, which stopped (--until-empty)
    # before the other's initiation was read - "no answer from" on CI's
    # macOS debug (2026-10-07), reproduced by starting that client two
    # seconds late.
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 280 --after 0
            --after-ready "${_ready}" --heard "${_kept}" --until-told-ground
            --until-exists "${_done}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --ready-file "${_ready}"
            --players 2 --data "${DATA}" --timeout 5 --store "${_store}"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "1;0;0")
    # Each client's own words with it, which its exit code alone does not say.
    file(READ "${_refused}" _refused_said)
    file(READ "${_kept}" _kept_said)
    message(FATAL_ERROR "the exit codes were ${_rcs}, not 1 for the client on other "
                        "ground and 0 for the rest:\n${_out}\n${_err}\n"
                        "the client on other ground said:\n${_refused_said}\n"
                        "the client on the same ground said:\n${_kept_said}")
endif()
file(READ "${_refused}" _said)
if(NOT _said MATCHES "refused the server's collision ground: it collides on ([^\n]*), and this client's is ([^\n]*)\n")
    message(FATAL_ERROR "the client on other ground did not refuse it:\n${_said}")
endif()
set(_theirs "${CMAKE_MATCH_1}")
set(_ours "${CMAKE_MATCH_2}")
if(NOT _out MATCHES "collision ground: ([^\n]*)\n" OR NOT CMAKE_MATCH_1 STREQUAL _theirs)
    message(FATAL_ERROR "what the client was told is not the server's ground:\n${_out}")
endif()
if(_ours STREQUAL _theirs)
    message(FATAL_ERROR "the client's ground was said to be the server's: ${_ours}")
endif()
# It left: its exit code was 1, above, long before its 20 s were up -
# the server stopped once both clients had gone (--until-empty).
file(READ "${_kept}" _said)
if(NOT _said MATCHES "told the collision ground: [^\n]*, this client's too\n" OR
   _said MATCHES "refused")
    message(FATAL_ERROR "the client on the same ground was not told it:\n${_said}")
endif()

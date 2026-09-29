# server_one_key_two_addresses.cmake - one key on two addresses is one player
# with one aircraft: a second session for a key takes over the first's slot and
# aircraft.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_one_key_two_addresses.cmake
#
# **Why.** A client started again from a new port while its old session was
# still live - before the server had let it go for silence - was given a second
# aircraft, and the player had two until the old one timed out.
#
# **The situation is built, not hoped for.** Two clients hold one key. The
# first joins and flies; the second starts only once the first is in its
# session (`--after-ready` on the first's `--heard` file, which it opens as its
# session begins), and joins from a port of its own while the first is still
# flying. The first stays until the second has gone (`--until-exists` on the
# second's `--done`), so without the take-over both are live together; the
# server runs until everybody has gone (`--until-empty`).
#
# **What must hold**: the key admitted twice, once from each address; the
# second session taking over from the first exactly once; the first told it is
# leaving, so that it stops rather than joining again and taking the aircraft
# back; and one player's aircraft in all, not two.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/one_key.sqlite")
set(_ready "${WORK}/ready.txt")
set(_first "${WORK}/first.txt")
set(_second_done "${WORK}/second_done.txt")
file(REMOVE "${_store}" "${_ready}" "${_first}" "${_second_done}")

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

# Both clients hold the key whose public half begins 11fc7622.
set(_secret 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2)
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300 --fly
            --after-ready "${_ready}" --heard "${_first}" --until-exists "${_second_done}"
            --key ${_secret}
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8 --fly
            --after-ready "${_first}" --done "${_second_done}" --key ${_secret}
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 30 --store "${_store}"
            --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

string(REGEX MATCHALL "admitted 11fc7622" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the key was admitted ${_admissions} times, not twice - once "
                        "from each address:\n${_out}\n${_err}")
endif()

string(REGEX MATCHALL "took over from 127\\.0\\.0\\.1:[0-9]+" _took "${_out}")
list(LENGTH _took _takes)
if(NOT _takes EQUAL 1)
    message(FATAL_ERROR "the second session took over ${_takes} times, not once:\n"
                        "${_out}\n${_err}")
endif()

file(READ "${_first}" _heard)
if(NOT _heard MATCHES "the server ended this session")
    message(FATAL_ERROR "the first client was not told it was leaving:\n${_heard}\n"
                        "${_out}\n${_err}")
endif()

string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees"
       _players "${_out}")
list(LENGTH _players _count)
if(NOT _count EQUAL 1)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft for one key, not "
                        "one:\n${_out}\n${_err}")
endif()
message(STATUS "one key on two addresses was one player: admitted twice, taken over "
               "once, one aircraft")

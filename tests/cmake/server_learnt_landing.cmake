# server_learnt_landing.cmake - a player's Cessna 172P on final is handed to
# the learnt landing, and the server lands it; a player's aeroplane without
# one is refused.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_learnt_landing.cmake
#
# **Built, not hoped for.** The server starts every player on final to
# Sydney's 16R (`--players-on-final YSSY/16R`): the first two miles out, at
# the learnt landing's gate, and the next half a mile further. The first
# client flies the plan's aeroplane, the C172P, and asks a second after
# joining for the learnt landing (`--learnt-landing-at`), and stays until
# the server has said the learnt landing has it and an update shows it at
# rest (`--until-landed`) - waiting on those events, not on the clock. The
# second asks for a Cessna 182S, which has no learnt landing, and asks the
# same. The server must say it handed the first to the learnt landing on
# final to YSSY 16R, and that the learnt landing touched it down within 5 m
# of the centreline under 300 ft/min and stopped it on the runway; and must
# refuse the second, saying the c182 has none - and the second client must
# never hear its aircraft handed over. The third flies a C172P too, started
# three miles out, and asks two seconds after joining: outside the gate, it
# is refused, the runway named and how far out it is. **Each refused client
# is told why** (`LEARNT_LANDING_REFUSED`, since protocol version 9), in the
# server's own words, and the one landed is told nothing of the kind.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/learnt-landing.sqlite")
set(_first "${WORK}/first.txt")
set(_second "${WORK}/second.txt")
set(_third "${WORK}/third.txt")
file(REMOVE "${_store}" "${_first}" "${_second}" "${_third}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
            --players-on-final YSSY/16R
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    # Each writes down the pipe to the next; the server, last, to here.
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 600 --after 1
            --learnt-landing-at 1 --until-landed --heard "${_first}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 12 --after 3
            --aircraft c182 --learnt-landing-at 2 --heard "${_second}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 12 --after 5
            --learnt-landing-at 2 --heard "${_third}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 900 --until-empty --ai 1 --headless
            --data "${DATA}" --store "${_store}" --players-on-final YSSY/16R
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()

if(NOT _out MATCHES "aircraft ([0-9]+) handed to the learnt landing, on final to YSSY 16R")
    message(FATAL_ERROR "the server never handed the C172P to the learnt landing:\n${_out}")
endif()
set(_first_number "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "aircraft ${_first_number}: the learnt landing touched down on YSSY 16R at ([0-9]+) ft/min, ([-+][0-9.]+) m across the centreline, and stopped ([0-9]+) m along, ([-+][0-9.]+) m across")
    message(FATAL_ERROR "the server never said the learnt landing had landed it:\n${_out}")
endif()
set(_sink "${CMAKE_MATCH_1}")
set(_across "${CMAKE_MATCH_2}")
set(_along "${CMAKE_MATCH_3}")
set(_stopped_across "${CMAKE_MATCH_4}")
string(REGEX REPLACE "^[-+]" "" _across_abs "${_across}")
string(REGEX REPLACE "^[-+]" "" _stopped_abs "${_stopped_across}")
if(_sink GREATER_EQUAL 300 OR _across_abs GREATER_EQUAL 5 OR _stopped_abs GREATER_EQUAL 15
   OR _along GREATER_EQUAL 3959)
    message(FATAL_ERROR "the learnt landing was outside its limits: ${_sink} ft/min, ${_across} "
                        "m across, stopped ${_along} m along and ${_stopped_across} m across")
endif()
file(READ "${_first}" _first_said)
foreach(_heard IN ITEMS "aircraft ${_first_number} handed to the learnt landing"
                        "aircraft ${_first_number} is at rest, landed by the learnt landing")
    if(NOT _first_said MATCHES "${_heard}")
        message(FATAL_ERROR "the first client never heard '${_heard}':\n${_first_said}")
    endif()
endforeach()

if(NOT _out MATCHES "aircraft ([0-9]+) not handed to the learnt landing: the c182 has no learnt landing")
    message(FATAL_ERROR "the server did not refuse the C182S the learnt landing:\n${_out}")
endif()
set(_second_number "${CMAKE_MATCH_1}")
file(READ "${_second}" _second_said)
if(_second_said MATCHES "aircraft ${_second_number} handed to")
    message(FATAL_ERROR "the C182S was handed over:\n${_second_said}")
endif()
if(NOT _second_said MATCHES "aircraft ${_second_number} refused the learnt landing: the c182 has no learnt landing")
    message(FATAL_ERROR "the C182S's client was not told why it was refused:\n${_second_said}")
endif()

# **Outside the gate, refused**: the third, a C172P three miles out.
if(NOT _out MATCHES "aircraft ([0-9]+) not handed to the learnt landing: not at the learnt landing's gate: YSSY 16R: [23]\\.[0-9] miles out")
    message(FATAL_ERROR "the server did not refuse the C172P three miles out:\n${_out}")
endif()
set(_third_number "${CMAKE_MATCH_1}")
file(READ "${_third}" _third_said)
if(_third_said MATCHES "aircraft ${_third_number} handed to")
    message(FATAL_ERROR "the C172P outside the gate was handed over:\n${_third_said}")
endif()
if(NOT _third_said MATCHES "aircraft ${_third_number} refused the learnt landing: not at the learnt landing's gate: YSSY 16R: [23]\\.[0-9] miles out")
    message(FATAL_ERROR "the client outside the gate was not told why it was refused:\n${_third_said}")
endif()
if(_first_said MATCHES "refused the learnt landing")
    message(FATAL_ERROR "the client landed was told it was refused:\n${_first_said}")
endif()
message(STATUS "the C172P landed by the learnt landing on YSSY 16R: ${_sink} ft/min, ${_across} m "
               "across, stopped ${_along} m along, ${_stopped_across} m across; the C182S "
               "and the C172P outside the gate refused")

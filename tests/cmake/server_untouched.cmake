# server_untouched.cmake - a player's aircraft the server holds untouched
# holds her height: through a real server, an F-35B and a C172P whose players
# send nothing for thirty seconds of the session's clock are within 100 ft of
# where they were first heard.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_untouched.cmake
#
# **Untouched is built, not hoped for.** Neither client is given `--fly`, so
# neither sends a single input: the server flies each on what it holds until
# her player's first input, for the whole stay - the slow join, made as long as
# the test. Held at six tenths of her throttle with the stick neutral, as the
# server held her until 2026-10-20, an F-35B was in a dive; trimmed level, she
# holds. Every aircraft in the catalogue is flown the same way, untouched for
# thirty seconds, by the unit test
# every_aircraft_a_player_is_given_and_leaves_untouched_holds_her_start_height;
# these two are the ones the item names, through the server itself.
#
# **On the session's clock and its events**: each client stays until it has
# heard its own aircraft for thirty simulated seconds (`--until-held 30`),
# and the server until both have gone (`--until-empty`). SECONDS is only the
# most either will wait.
#
# One AI aircraft flies the plan, because a server with none and nothing
# `--fly`ing builds no fleet and gives a player nothing; the players' are
# stacked above it, and only their own are measured.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/untouched.sqlite")
set(_ready "${WORK}/ready.txt")
set(_f35b "${WORK}/f35b.txt")
set(_c172p "${WORK}/c172p.txt")
file(REMOVE "${_store}" "${_ready}" "${_f35b}" "${_c172p}")

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
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 600 --aircraft f35b
            --until-held 30 --after-ready "${_ready}" --heard "${_f35b}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 600 --aircraft c172p
            --until-held 30 --after-ready "${_ready}" --heard "${_c172p}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 900 --until-empty --ai 1 --headless
            --data "${DATA}" --store "${_store}" --ready-file "${_ready}"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()

# **Each is flown as the aeroplane asked for, and neither was touched**: the
# server says it gave each, and says nothing of an input applied.
set(_covered 0)
foreach(_id IN ITEMS f35b c172p)
    if(NOT _out MATCHES "asked for ${_id}, and flies it\n")
        message(FATAL_ERROR "the server did not give the ${_id} asked for:\n${_out}")
    endif()
    file(READ "${WORK}/${_id}.txt" _said)
    if(NOT _said MATCHES "my aircraft heard from ([0-9.]+) s to ([0-9.]+) s: ([-0-9.]+) m at first, ([-0-9.]+) m to ([-0-9.]+) m")
        message(FATAL_ERROR "the ${_id}'s client did not say how high it heard her:\n${_said}")
    endif()
    set(_from ${CMAKE_MATCH_1})
    set(_to ${CMAKE_MATCH_2})
    set(_first ${CMAKE_MATCH_3})
    set(_low ${CMAKE_MATCH_4})
    set(_high ${CMAKE_MATCH_5})
    # 100 ft is 30.48 m. CMake has no floating point: the figures are
    # compared in centimetres.
    foreach(_v IN ITEMS from to first low high)
        string(REGEX REPLACE "\\.([0-9])$" "\\10" _cm "${_${_v}}")
        string(REGEX REPLACE "\\.([0-9][0-9])[0-9]*$" "\\1" _cm "${_cm}")
        set(_${_v}_c "${_cm}")
    endforeach()
    math(EXPR _heard_c "${_to_c} - ${_from_c}")
    math(EXPR _fell_c "${_first_c} - ${_low_c}")
    math(EXPR _rose_c "${_high_c} - ${_first_c}")
    message(STATUS "the ${_id}, untouched from ${_from} s to ${_to} s of the session: "
                   "${_first} m at first, ${_low} m to ${_high} m")
    if(_heard_c LESS 3000)
        message(FATAL_ERROR "the ${_id} was heard for ${_from} s to ${_to} s, not thirty")
    endif()
    if(_fell_c GREATER 3048 OR _rose_c GREATER 3048)
        message(FATAL_ERROR "the ${_id}, untouched for thirty seconds, went from ${_first} m "
                            "to between ${_low} m and ${_high} m: more than 100 ft (30.48 m) "
                            "from where she began")
    endif()
    math(EXPR _covered "${_covered} + 1")
endforeach()
if(NOT _covered EQUAL 2)
    message(FATAL_ERROR "${_covered} aircraft were checked, not the two the item names")
endif()

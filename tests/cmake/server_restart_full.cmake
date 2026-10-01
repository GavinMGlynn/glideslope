# server_restart_full.cmake - a player started again on a full server flies
# again at once, with one aircraft and no ghost of the old session.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_restart_full.cmake
#
# **Why.** A full server refused every initiation `SERVER_FULL` before reading
# it, sparing a stranger the asymmetric work. A player whose client crashed
# and was started again - a new port, the same key - was refused like a
# stranger until the old session's `--timeout` had let it go: on a server of
# one player, the player could not fly at all for that long.
#
# **The situation is built, not hoped for.** A server of one player
# (`--players 1`), so that the player's own session fills it. The first client
# joins and flies, then stops without a goodbye (`--no-goodbye`), as a crashed
# client does: its session is live at the server, and silent. The second, the
# same key from a port of its own, starts as the first ends (`--after-ready`
# on the first's `--done`) and flies. The timeout is a minute, 7,200 steps,
# so nothing here passes by it: had the server waited for the old session to
# be let go for silence, the second would have been refused `SERVER_FULL` and
# ended, and the server says "of silence" before anything else.
#
# **What is counted is simulated steps**: each client writes to its `--heard`
# file which steps of the simulation it was told of, and the gap from the
# first's last to the second's first is how long the player was off the
# server. It must be less than the timeout; it is reported, and there is no
# least but nought - a slow machine may take its time starting a process.
#
# **What must hold**: the key admitted twice, once from each port; the
# second refused nothing; the second taking over the first's session exactly
# once, and the server letting no session go for silence; the second's
# inputs applied, and its aircraft banked; one player's aircraft in all, not
# two; and the server ending when the second leaves (`--until-empty`), which
# it would not with a ghost of the first still connected.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/restart.sqlite")
set(_ready "${WORK}/ready.txt")
set(_first "${WORK}/first.txt")
set(_first_done "${WORK}/first_done.txt")
set(_second "${WORK}/second.txt")
file(REMOVE "${_store}" "${_ready}" "${_first}" "${_first_done}" "${_second}")

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
set(_timeout_s 60)
math(EXPR _timeout_steps "${_timeout_s} * 120")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --fly --no-goodbye
            --after-ready "${_ready}" --heard "${_first}" --done "${_first_done}"
            --key ${_secret}
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --fly
            --after-ready "${_first_done}" --heard "${_second}" --key ${_secret}
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 1 --data "${DATA}" --timeout ${_timeout_s} --store "${_store}"
            --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(_out MATCHES "of silence")
    message(FATAL_ERROR "the server let a session go for silence, so the player waited "
                        "out the timeout:\n${_out}\n${_err}")
endif()

file(READ "${_second}" _heard_second)
if(_heard_second MATCHES "refused")
    message(FATAL_ERROR "the player started again was refused:\n${_heard_second}\n"
                        "${_out}\n${_err}")
endif()

string(REGEX MATCHALL "admitted 11fc7622" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the key was admitted ${_admissions} times, not twice - once "
                        "from each port:\n${_out}\n${_err}")
endif()

string(REGEX MATCHALL "took over from 127\\.0\\.0\\.1:[0-9]+" _took "${_out}")
list(LENGTH _took _takes)
if(NOT _takes EQUAL 1)
    message(FATAL_ERROR "the second session took over ${_takes} times, not once:\n"
                        "${_out}\n${_err}")
endif()

# **The gap, in simulated steps**, from the last the first was told of to the
# first the second was.
file(READ "${_first}" _heard_first)
if(NOT _heard_first MATCHES "state updates from step [0-9]+ to step ([0-9]+)")
    message(FATAL_ERROR "the first client was told of no steps:\n${_heard_first}\n"
                        "${_out}\n${_err}")
endif()
set(_first_last "${CMAKE_MATCH_1}")
if(NOT _heard_second MATCHES "state updates from step ([0-9]+) to step [0-9]+")
    message(FATAL_ERROR "the player started again was told of no steps:\n"
                        "${_heard_second}\n${_out}\n${_err}")
endif()
set(_second_first "${CMAKE_MATCH_1}")
math(EXPR _gap "${_second_first} - ${_first_last}")
if(_gap LESS 0 OR NOT _gap LESS _timeout_steps)
    message(FATAL_ERROR "the player was off the server for ${_gap} steps, not fewer than "
                        "the timeout's ${_timeout_steps}:\n${_out}\n${_err}")
endif()

if(NOT _heard_second MATCHES "the server applied ([0-9]+)" OR CMAKE_MATCH_1 EQUAL 0)
    message(FATAL_ERROR "the server applied none of the player's inputs once started "
                        "again:\n${_heard_second}\n${_out}\n${_err}")
endif()
string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees"
       _players "${_out}")
list(LENGTH _players _count)
if(NOT _count EQUAL 1)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft for one key, not "
                        "one:\n${_out}\n${_err}")
endif()
message(STATUS "the player started again on a full server flew again ${_gap} steps "
               "after the old session's last (the timeout is ${_timeout_steps}): "
               "admitted twice, taken over once, one aircraft")

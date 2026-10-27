# starved_f35b_placed.cmake - a predicting F-35B whose client stops for three
# seconds between flying and sending, on a machine too slow for her
# prediction, is placed within four steps' travel.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P starved_f35b_placed.cmake
#
# **Built, not waited for.** Five clients and servers on two CPUs stalled an
# F-35B's client 1.7 to 3 s, and her updates were placed 9 to 64 steps off
# (docs/status, 2026-10-21). The stall that does it is one between flying the
# steps owed on an input and sending the next, on a machine slow at her
# prediction: the next input leaves seconds after the step it is flown from,
# the server applies it that much late, and a clocks' difference sampled from
# that step says the clocks are that much further apart. So the client is
# made slow (`--slow-steps 2000`: 1.5 ms more of its own CPU on every step it
# flies, forward or again) and is stopped once, 3,000 ms, at 12 s on the
# session's clock, after flying and before sending (`--stopped-sending`).
# On every run. One case: the F-35B, joined from what the server held her on
# and touched not at all (`--from-held`), through a relay holding every
# datagram 100 ms each way.
#
# **And the server's start slow, as a loaded machine's is** (`--test-slow-start
# 2000`): a debug server's first steps took 1.7 to 2.2 s on CI's Ubuntu, and
# it caught that time up at 1.4 times real time while this client joined,
# whose clocks' difference was then learnt 5 to 15 steps wrong and an update
# put 23 m off (CI run 40087273404). Its start is not owed now
# (glideslope_server's loop); built, so that it is tested on every run, not
# on whichever machine happens to be slow.
#
# **The bound is the item's**: every update compared - across the stall's
# words as well as outside them - within four steps' travel at her speed,
# 1.378 m a step, 5.51 m (server_first_input.cmake's figure). And the
# situation is asserted built: the stall named the machine's, 2.9 s or more,
# and an input sent at least a second's steps after the step it is flown
# from, its sample taken.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/starved_f35b.sqlite")
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

# A figure in metres, as centimetres: CMake has no floating point.
function(centimetres value out)
    string(REGEX REPLACE "^([0-9]+)\\.([0-9][0-9]).*" "\\1\\2" _c "${value}")
    math(EXPR _c "${_c} + 0")
    set(${out} "${_c}" PARENT_SCOPE)
endfunction()

math(EXPR _relay "${PORT} + 1")
set(_heard "${WORK}/f35b-stopped-sending.txt")
file(REMOVE "${_heard}")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 600 --after 1
            --aircraft f35b --predict --from-held --until-held 30
            --slow-steps 2000 --stopped-sending 12 3000 --heard "${_heard}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 900 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" --test-slow-start 2000
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 900
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(NOT _err MATCHES "asked for f35b, and flies it\n")
    message(FATAL_ERROR "the server did not give the F-35B asked for:\n${_err}")
endif()
file(READ "${_heard}" _said)

# The situation, built.
if(NOT _said MATCHES "the machine's stall 1: ([0-9]+) ms[^\n]*updates compared, the worst ([0-9.]+) m, back")
    message(FATAL_ERROR "the client did not name the stall built:\n${_said}")
endif()
set(_stall_ms ${CMAKE_MATCH_1})
set(_stall_worst ${CMAKE_MATCH_2})
if(_stall_ms LESS 2900)
    message(FATAL_ERROR "the stall named was ${_stall_ms} ms, not the 3,000 built:\n${_said}")
endif()
if(NOT _said MATCHES "sent late: an input sent at most ([0-9]+) steps after")
    message(FATAL_ERROR "the client did not say how late it sent its inputs:\n${_said}")
endif()
set(_late ${CMAKE_MATCH_1})
if(_late LESS 120)
    message(FATAL_ERROR "no input was sent a second or more after the step it is flown from "
                        "(at most ${_late} steps): the situation was not built:\n${_said}")
endif()

# The verification: every update within four steps' travel.
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
set(_compared ${CMAKE_MATCH_1})
set(_worst ${CMAKE_MATCH_2})
if(NOT _said MATCHES "prediction error 95th percentile: ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say its 95th percentile:\n${_said}")
endif()
set(_p95 ${CMAKE_MATCH_1})
message(STATUS "stopped ${_stall_ms} ms between flying and sending, an input sent ${_late} "
               "steps late; ${_compared} updates compared, the worst ${_worst} m outside the "
               "stall's words and ${_stall_worst} m across them, the 95th percentile ${_p95} m")
if(_compared LESS 300)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
# **At most two of the machine's stalls**, as machine_stalls_check.cmake caps
# them: the one built, and the pass after it that catches up - a slow
# client's own, but the machine's too where its CPU went to the server and
# relay (833 ms seen). Not its "back within a metre": one step of an F-35B's
# is 1.378 m, and the bound here is four.
if(NOT _said MATCHES "the machine's stalls: ([0-9]+) frames")
    message(FATAL_ERROR "the client did not say its machine's stalls:\n${_said}")
endif()
if(CMAKE_MATCH_1 GREATER 2)
    message(FATAL_ERROR "the machine stalled ${CMAKE_MATCH_1} times, past the two allowed:\n"
                        "${_said}")
endif()
centimetres("${_worst}" _worst_c)
centimetres("${_stall_worst}" _stall_worst_c)
if(_worst_c GREATER 551 OR _stall_worst_c GREATER 551)
    message(FATAL_ERROR "an update was placed ${_worst} m off outside the stall's words and "
                        "${_stall_worst} m across them: past four steps' travel (5.51 m):\n"
                        "${_said}")
endif()

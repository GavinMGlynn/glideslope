# server_weather_gusts.cmake - a client predicting in gusting air meets every
# gust at the moment the server met it, and predicts within the steady air's
# bound.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DSAME_M=<m> -P server_weather_gusts.cmake
#
# A server flies 30 kt gusting 45 from the west (`--metar`), and twelve
# seconds in 35 gusting 50 from the south-west, blended in over five
# (`--metar-then`, `--weather-blend`): server_weather.cmake's flight, in
# gusts. One player flies it, predicting its own aircraft through a relay
# holding every datagram 250 ms each way.
#
# **Why the moment is held, not only the error**: a gust is of the session's
# time, and a client meets it at the moment its clocks' difference says the
# server flies the step. With that moment five seconds wrong the median
# error was 0.150 m (2026-10-08), under the steady air's 0.25 m bound: the
# error alone does not tell a wrong gust clock. So the client keeps, for
# every step it flies forward, the moment it asked the air at, and holds it
# against the word's own step when the word comes (`gust clock:`). **Built,
# not hoped for**:
#
# - the client was told both gusting weathers - from 0 s, and from 12 s over 5;
# - each update is judged at its own step: the step of the client's the
#   difference the word itself shows places it at (the server's step its
#   input began on, less the client's), not the estimate - the least over a
#   window - that places it for the error. It is one of three: flown at the
#   server's moment; flown on an estimate other than the word's difference,
#   out by exactly as much - which a client cannot know until the word says
#   so, and is the open part of COMPLETION_PLAN.md's item; or out otherwise,
#   which is a wrong gust clock. At least 300 judged; none out otherwise; and
#   none flown later than the server flew it where the estimate it was flown
#   on stood above ten or more of the 50 words' own differences before it.
#   Since 2026-11-22 the estimate is held, once settled, against a sample now
#   and then a step low: it is lowered only when ten of the window's samples
#   are below it (sim::ClockDifference, offset_lowered_after), so it stands
#   above up to nine, and a step flown for one of those words is flown later
#   than the server by design - that is how it stops moving on a busy
#   machine. What is asserted is the estimate's own rule seen from outside:
#   each sample is no more than its word's difference (taken from when the
#   input was sent, not begun), so fewer than ten samples below means fewer
#   than ten words' own differences below, and a later step with ten or more
#   below is an estimate held where its rule would have lowered it. Until
#   2026-11-22 the estimate was the least and this asserted that no step was
#   flown later but for a word setting a new least. **Not caught**:
#   an estimate wrong early, by a step or more - it is out that way by
#   design in about half the updates, by 1 to 3 steps. Those not judged (an
#   input not begun here, a step no longer kept) are counted and said;
# - the prediction error's median is within SAME_M, server_weather.cmake's
#   steady bound.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake): another
# build's run of this test on the same port waits for it.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/weather_gusts.sqlite")
set(_ready "${WORK}/ready")
file(REMOVE "${_store}" "${_ready}")

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
set(_station -33.9461,151.1772,6)
set(_west "YSSY 020600Z 27030G45KT 9999 FEW030 20/10 Q1012")
set(_south_west "YSSY 020630Z 23035G50KT 9999 FEW030 19/10 Q1010")

set(_said_file "${WORK}/gusts.txt")
file(REMOVE "${_said_file}")
math(EXPR _relay "${PORT} + 4")
port_lock_hold(${_relay})
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 25
            --after-ready "${_ready}" --predict --heard "${_said_file}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 1 --data "${DATA}" --timeout 30 --store "${_store}"
            --ready-file "${_ready}"
            --metar "${_west}" --station ${_station}
            --metar-then 12 "${_south_west}" --weather-blend 5
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 250 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
file(READ "${_said_file}" _said)
foreach(_told IN ITEMS
        "told the weather: ${_west} \\(weather 1, from 0.000 s over 5 s\\)\n"
        "told the weather: ${_south_west} \\(weather 2, from 12.000 s over 5 s\\)\n")
    if(NOT _said MATCHES "${_told}")
        message(FATAL_ERROR "the client was not told '${_told}':\n${_said}")
    endif()
endforeach()
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9.]+) m[^\n]*, ([0-9.]+) s in")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
set(_worst ${CMAKE_MATCH_2})
set(_worst_at ${CMAKE_MATCH_3})
if(NOT _said MATCHES "prediction error median: ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say its median error:\n${_said}")
endif()
set(_median ${CMAKE_MATCH_1})
set(_clock_said "gust clock: ([0-9]+) updates judged in air, ([0-9]+) not; ([0-9]+) flown at ")
string(APPEND _clock_said "the server's moment, the worst error ([0-9.]+) m; ([0-9]+) flown on ")
string(APPEND _clock_said "another estimate of the clocks' difference than the word's, ([0-9]+) of ")
string(APPEND _clock_said "them later than the server with the estimate held above too many words, at most ")
string(APPEND _clock_said "(-?[0-9]+) steps out, the worst ([0-9.]+) m; ([0-9]+) out otherwise, ")
string(APPEND _clock_said "at most (-?[0-9]+) steps")
if(NOT _said MATCHES "${_clock_said}")
    message(FATAL_ERROR "the client did not say its gust clock:\n${_said}")
endif()
set(_judged ${CMAKE_MATCH_1})
set(_unjudged ${CMAKE_MATCH_2})
set(_at_moment ${CMAKE_MATCH_3})
set(_worst_at_moment ${CMAKE_MATCH_4})
set(_estimate_off ${CMAKE_MATCH_5})
set(_later ${CMAKE_MATCH_6})
set(_most_estimate_off ${CMAKE_MATCH_7})
set(_worst_estimate_off ${CMAKE_MATCH_8})
set(_wrong ${CMAKE_MATCH_9})
message(STATUS "in gusts, ${_judged} updates judged in air (${_unjudged} not): ${_at_moment} "
               "flown at the server's moment, the worst error ${_worst_at_moment} m; "
               "${_estimate_off} on another estimate than the word's (${_later} later than the server, at most "
               "${_most_estimate_off} steps out), the worst ${_worst_estimate_off} m; ${_wrong} "
               "out otherwise. The median ${_median} m, the worst ${_worst} m at ${_worst_at} s")
math(EXPR _kinds "${_at_moment} + ${_estimate_off} + ${_wrong}")
if(NOT _kinds EQUAL _judged)
    message(FATAL_ERROR "of ${_judged} updates judged in air, ${_kinds} were told apart")
endif()
if(_judged LESS 300)
    message(FATAL_ERROR "${_judged} updates were judged in air, fewer than the 300 a flight of "
                        "20 s gives:\n${_said}")
endif()
if(NOT _later EQUAL 0)
    message(FATAL_ERROR "${_later} steps were flown later than the server flew them on an "
                        "estimate standing above ten or more of the 50 words' own differences "
                        "before it - where its rule (sim::ClockDifference) would have lowered "
                        "it:\n${_said}")
endif()
if(NOT _wrong EQUAL 0)
    message(FATAL_ERROR "${_wrong} of ${_judged} steps judged met the gusts at another moment "
                        "than the server's, and not for the "
                        "estimate of the clocks' difference:\n${_said}")
endif()
if(_median GREATER SAME_M)
    message(FATAL_ERROR "in gusts the prediction's median error was ${_median} m, more than "
                        "the steady air's ${SAME_M}")
endif()

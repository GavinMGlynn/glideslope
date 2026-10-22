# server_stovl_vertical_landing.cmake - a player's F-35B, given a loading
# light enough to hover, converted to jet-borne flight, hovered and landed
# vertically through a real server, the client's prediction agreeing.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P server_stovl_vertical_landing.cmake
#
# **Built, not hoped for.** As server_stovl_landing.cmake - a server with one
# AI and a plan that starts its player at 2,200 ft over Sydney airport
# heading west, over land, and the command-line client predicting its own
# aircraft through a relay that holds every datagram 100 ms each way - but
# the client asks for the F-35B **at her `vertical_landing` loading**
# (`--aircraft f35b --loading vertical_landing`, the initiation's payload
# since protocol 14): 37,530 lb, under the 39,837 lb she is flown into a
# hover at (`fcs/stovl-hover-most-lbs`). At her model's own 45,400 lb, which
# every server gave a player before, her STOVL mode holds her wing-borne
# (server_stovl_landing.cmake). The server gives the loading her figures
# file names and says so; it tells every client her loading with her
# `AIRCRAFT`, and the client's prediction is loaded so before it flies.
#
# Its pilot (`--stovl-land`) flies the power the server holds for a player
# who has not touched anything until its prediction has settled and 150
# inputs are sent, then selects STOVL mode and holds the stick full forward,
# the lever still: her transition law converts her, slows her to a hover,
# and the stick lets her down onto the ground. The client stays until its
# prediction is down after a hover and the server's word has her at rest:
# the event, not a time.
#
# **What must hold**: the server gives the loading asked for; the client is
# told it and predicts her at it (under her hover limit); no wreck; the
# client's prediction converts her, hovers and lands her, and the server's
# word has her hovering and then at rest; and through all of it the
# prediction agrees with the server - no correction too large to hide once
# joined, and the bounds server_stovl_landing.cmake holds wing-borne, where
# its reasons are given, set from every platform's CI by #214 and #219: the
# median error under 60 mm - CI's medians reached 28 mm on windows-clang with
# nothing broken, and a law that has wandered is caught by the pitch check,
# not the median - the 95th percentile within four steps' travel at her
# conversion speed (3.4 m), the worst within
# four at 330 kt (5.6 m), and her predicted pitch within half a degree of
# the server's - over every update compared, at least 3,000, from the
# selector through the conversion, the hover and the landing. Measured
# first (linux-release, idle): converted at 63.7 s, hovering at 164.4 s,
# down at 339.5 s; 8,469 updates compared, the median 0 mm, the 95th
# percentile 2 mm, the worst 1.357 m (a step of the clocks' difference),
# her pitch within 0.109 degrees; 343 s in all.
#
# RUN_SERIAL: a runner busy beside it would hold the server up further,
# unsteadily, as server_paced_prediction.cmake says of itself.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/stovl.sqlite")
set(_heard "${WORK}/heard.txt")
set(_plan "${WORK}/stovl.plan")
file(REMOVE "${_store}" "${_heard}")
# The AI - a server with none gives nobody an aircraft - flies the plan from
# its start, north; the player starts two layers (2,000 ft) above it, west.
file(WRITE "${_plan}" "aircraft c172p\n"
                      "start -33.9461 151.1772 200 270 100\n"
                      "waypoint NORTH -33.5000 151.1772 1500 100\n")
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

# The server is in the middle, and its standard output is the relay's
# input (`--until-input-ends`), so what it says is not read here: that it
# gave the loading is held by what the client is told (`AIRCRAFT`) and by
# the server's word having her hover at all - at her model's own weight
# its law holds her wing-borne. The client's words are in its `--heard`
# file.
math(EXPR _relay "${PORT} + 1")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 1500 --after 1
            --aircraft f35b --loading vertical_landing --predict --stovl-land
            --heard "${_heard}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 1500 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" --plan "${_plan}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 1500
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(_out MATCHES "is a wreck" OR _err MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was a wreck:\n${_out}\n${_err}")
endif()
# **The loading given, and told.**
file(READ "${_heard}" _said)
if(NOT _said MATCHES "aircraft [0-9]+ is f35b, loaded vertical_landing\n")
    message(FATAL_ERROR "the client was not told her loading:\n${_said}")
endif()
if(NOT _said MATCHES "stovl: predicted at ([0-9]+) lb as it began")
    message(FATAL_ERROR "the client did not say what its prediction weighed:\n${_said}")
endif()
set(_weight "${CMAKE_MATCH_1}")
# Under her hover limit (39,837 lb) and no more than her loading, less what
# she burned before the client's first word: at her model's own 45,400 she
# would be held wing-borne.
if(_weight GREATER 37530 OR _weight LESS 37000)
    message(FATAL_ERROR "her prediction began at ${_weight} lb, not her vertical_landing "
                        "37,530 less a little fuel:\n${_said}")
endif()
if(NOT _said MATCHES "stovl: converted at ([-0-9.]+) s, hovering at ([-0-9.]+) s, down at ([-0-9.]+) s; the server's word: hovering (yes|no), at rest (yes|no); lowest ([-0-9]+) ft above the ground, ([0-9]+) KCAS at the end")
    message(FATAL_ERROR "the client never said how its STOVL flight went:\n${_said}\n${_err}")
endif()
set(_converted "${CMAKE_MATCH_1}")
set(_hovering "${CMAKE_MATCH_2}")
set(_down "${CMAKE_MATCH_3}")
set(_server_hovered "${CMAKE_MATCH_4}")
set(_server_at_rest "${CMAKE_MATCH_5}")
if(_converted STREQUAL "-1.0" OR _hovering STREQUAL "-1.0" OR _down STREQUAL "-1.0")
    message(FATAL_ERROR "her prediction was not converted, hovered and put down (converted at "
                        "${_converted} s, hovering at ${_hovering} s, down at ${_down} s):\n${_said}")
endif()
if(NOT _server_hovered STREQUAL "yes" OR NOT _server_at_rest STREQUAL "yes")
    message(FATAL_ERROR "the server's word did not have her hovering (${_server_hovered}) and "
                        "then at rest (${_server_at_rest}):\n${_said}")
endif()
if(NOT _said MATCHES "stovl: selected with input ([0-9]+), its prediction settled")
    message(FATAL_ERROR "the client never selected STOVL mode, its prediction settled:\n${_said}")
endif()
if(NOT _said MATCHES "stovl: predicted pitch within ([0-9]+)\\.([0-9][0-9][0-9]) degrees")
    message(FATAL_ERROR "the client did not say its pitch error:\n${_said}")
endif()
set(_pitch "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
math(EXPR _pitch_mdeg "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_pitch_mdeg GREATER 500)
    message(FATAL_ERROR "her predicted pitch was ${_pitch} degrees off the server's, over half a degree:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
set(_compared "${CMAKE_MATCH_1}")
math(EXPR _worst_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
if(_compared LESS 3000)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
if(_worst_mm GREATER 5600)
    message(FATAL_ERROR "the worst prediction error was ${_worst_mm} mm, over four steps' travel "
                        "at 330 kt (5.6 m):\n${_said}")
endif()
if(NOT _said MATCHES "predicted, joined: ([0-9]+) corrections, the worst ([0-9.]+) m, ([0-9]+) too large to hide")
    message(FATAL_ERROR "the client did not say its corrections once joined:\n${_said}")
endif()
set(_joined "${CMAKE_MATCH_1}")
set(_joined_worst "${CMAKE_MATCH_2}")
if(NOT CMAKE_MATCH_3 EQUAL 0)
    message(FATAL_ERROR "${CMAKE_MATCH_3} corrections once joined were too large to hide:\n${_said}")
endif()
math(EXPR _joined_least "${_compared} * 9 / 10")
if(_joined LESS _joined_least)
    message(FATAL_ERROR "only ${_joined} corrections once joined, under ${_joined_least} - nine in "
                        "ten of the ${_compared} updates compared:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error 95th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its 95th percentile:\n${_said}")
endif()
math(EXPR _p95_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_p95_mm GREATER 3400)
    message(FATAL_ERROR "the 95th percentile prediction error was ${_p95_mm} mm, over four steps' "
                        "travel at 200 KCAS (3.4 m):\n${_said}")
endif()
if(NOT _said MATCHES "prediction error median: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its median error:\n${_said}")
endif()
math(EXPR _median_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_median_mm GREATER 60)
    message(FATAL_ERROR "the median prediction error was ${_median_mm} mm, over 60 mm:\n${_said}")
endif()
message(STATUS "at ${_weight} lb: converted at ${_converted} s, hovering at ${_hovering} s, "
               "down at ${_down} s, the server's word hovering and at rest; ${_compared} "
               "updates compared, the worst ${_worst_mm} mm, the 95th percentile ${_p95_mm} mm, "
               "the median ${_median_mm} mm; ${_joined} corrections once joined (at least "
               "${_joined_least}), the worst ${_joined_worst} m; her pitch within ${_pitch} "
               "degrees of the server's")
# **The machine's stalls**, capped and each recovered from
# (machine_stalls_check.cmake): CI's Windows clang once went 5,214 ms between
# two of this client's frames and its worst went to 6.2 m (run 39701548318).
include("${CMAKE_CURRENT_LIST_DIR}/machine_stalls_check.cmake")
check_machine_stalls("${_said}" updates "${_compared}" _stalls)
message(STATUS "${_stalls}")

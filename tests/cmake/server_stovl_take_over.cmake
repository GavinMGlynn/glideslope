# server_stovl_take_over.cmake - a client taking over an AI's F-35B, or
# taking back its own from the AI, while her STOVL law hovers her, predicts
# her from the server's law state (`LAW_STATE`) from its first update.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port>
#         -DSCENE=take_over|take_back|take_back_wing_borne
#         -P server_stovl_take_over.cmake
#
# **Built, not hoped for.** The server runs one AI F-35B on a plan that
# names her `vertical_landing` loading (`loading vertical_landing`), light
# enough to hover: from 1,000 ft south of Sydney airport heading north at
# 210 kt, by a waypoint 1.1 km on, to a vertical landing 8.9 km beyond it,
# hovering 600 s over its point first - converted from the waypoint, slowed
# to a hover over the point by her transition law. A player given the
# plan's aircraft and asking for no loading is given the plan's too.
#
# One client, through a relay that holds every datagram 100 ms each way,
# predicting (`--predict --stovl-land --stovl-level`): its selector on once
# its prediction has settled, the stick centred, so that its own F-35B
# converts at the height she was at, slows jet-borne and hovers.
#
#   take_over  It rides in the AI's F-35B (`--watch-ai`) and takes her
#              over once she has hovered over the point for three
#              seconds (`--take-over-at 0 --when-hovering`, an event); its own goes to the AI.
#   take_back  It hands its own F-35B to the AI 150 s in, converted and
#              jet-borne, slowing (`--hand-over-at 150`), and takes her
#              back once she has hovered for three seconds (`--take-back-at
#              0 --when-hovering`). Handed to the AI with no plan given her, she is held
#              in STOVL mode: the AI holds the hover she is slowing to at
#              the height asked (sim::Controller), and her law is never
#              let go.
#   take_back_wing_borne
#              The same hand-over, and 5 s later a route sent for the AI
#              to fly (`--send-route`, a waypoint 21 km north at 2,500 ft
#              and 250 kt): the AI converts her back and flies it
#              wing-borne, and her law is let go. Taken back 300 s in,
#              wing-borne at 250 kt, its selector still on: the server
#              makes her a new law on the step it gives her back, and
#              tells it (stage 0). Before the server told a law on a
#              take-back, the client made its own from its own state, a
#              step or more apart: 0.858 degrees of pitch off
#              (PROJECT_STATUS.md, 2026-10-22).
#
# Either way it stays until 750 updates are compared since its prediction
# began on the law told (`--until-compared-on-law`): the event, not a time.
#
# **What must hold**: no wreck; the AI flies the plan's loading; the
# take-over or the take-back is done, and the server told her law to the
# client once, hovering (stage 3; wing-borne, stage 0, made afresh on the
# take-back, for take_back_wing_borne); the client began its prediction on it,
# hovering, and from its first update compared, its predicted pitch within
# half a degree of the server's at every update, the median within 60 mm,
# the 95th percentile within 3.4 m and the worst within 5.6 m:
# server_stovl_vertical_landing.cmake's bounds. Taking her over, the law
# came ahead of the update saying she was the client's, while it still
# predicted the one it left, and was dropped: the client waited for it for
# ever (PROJECT_STATUS.md, 2026-10-22).
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.
cmake_minimum_required(VERSION 3.28)
if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/stovl.sqlite")
set(_heard "${WORK}/heard.txt")
set(_plan "${WORK}/hover.plan")
set(_route "${WORK}/north.route")
file(REMOVE "${_store}" "${_heard}")
file(WRITE "${_route}" "waypoint NORTH -33.7000 151.1772 2500 250\n")
file(WRITE "${_plan}" "aircraft f35b\n"
                      "loading vertical_landing\n"
                      "start -33.9800 151.1772 1000 0 210\n"
                      "waypoint W -33.9700 151.1772 1000 210\n"
                      "land_vertically PAD -33.8900 151.1772 600\n")
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
math(EXPR _relay "${PORT} + 1")
set(_flies 900 --after 1 --predict --stovl-land
           --stovl-level --until-compared-on-law 750 --heard "${_heard}")
if(SCENE STREQUAL "take_over")
    list(APPEND _flies --watch-ai --take-over-at 0 --when-hovering)
elseif(SCENE STREQUAL "take_back")
    list(APPEND _flies --hand-over-at 150 --take-back-at 0 --when-hovering)
elseif(SCENE STREQUAL "take_back_wing_borne")
    list(APPEND _flies --hand-over-at 150 --send-route "${_route}" --copilot-at 155
                       --copilot-stay 600 --take-back-at 300)
else()
    message(FATAL_ERROR "SCENE is take_over, take_back or take_back_wing_borne, not "
                        "'${SCENE}'")
endif()
execute_process(
    # The address here, not in set(): test_ports.cmake reads the ports.
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" ${_flies}
    COMMAND "${SERVER}" --port ${PORT} --seconds 1200 --until-empty --ai 1 --headless
            --players 1 --data "${DATA}" --timeout 5 --store "${_store}" --plan "${_plan}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 1200
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
list(REMOVE_ITEM _rcs 0)
if(_rcs)
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(_out MATCHES "is a wreck" OR _err MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was a wreck:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
# The server's stdout reaches standard error through the pipe, a line at a
# time.
if(NOT _err MATCHES "AI 1 flies the plan's vertical_landing loading, 37530 lb")
    message(FATAL_ERROR "the AI did not fly the plan's loading:\n${_err}")
endif()
if(SCENE STREQUAL "take_over")
    if(NOT _err MATCHES "aircraft ([0-9]+) taken over; aircraft [0-9]+, left, now the AI's")
        message(FATAL_ERROR "the AI's F-35B was not taken over:\n${_err}")
    endif()
    set(_hers "${CMAKE_MATCH_1}")
else()
    # Two tests, not one: `${CMAKE_MATCH_1}` in a single condition is
    # expanded before its first MATCHES has run.
    if(NOT _err MATCHES "aircraft ([0-9]+) handed to the AI\n")
        message(FATAL_ERROR "her F-35B was not handed to the AI:\n${_err}")
    endif()
    set(_hers "${CMAKE_MATCH_1}")
    if(NOT _err MATCHES "aircraft ${_hers} handed to its pilot\n")
        message(FATAL_ERROR "her F-35B was not taken back:\n${_err}")
    endif()
endif()
set(_stage 3)
if(SCENE STREQUAL "take_back_wing_borne")
    set(_stage 0)
    # Her law made twice: as her pilot selected STOVL mode, and again as
    # she was taken back with it still selected, the first let go.
    string(REGEX MATCHALL "aircraft ${_hers}'s STOVL law made at step" _made "${_err}")
    list(LENGTH _made _made)
    if(NOT _err MATCHES "aircraft ${_hers} flies its copilot's route of 1" OR
       NOT _made EQUAL 2)
        message(FATAL_ERROR "the AI did not fly her route, or no law was made afresh as she "
                            "was taken back:\n${_err}")
    endif()
endif()
string(REGEX MATCHALL "aircraft [0-9]+'s STOVL law told to its client[^\n]*" _told "${_err}")
list(LENGTH _told _times)
if(NOT _times EQUAL 1 OR NOT _told MATCHES "^aircraft ${_hers}'s .*: stage ${_stage},")
    message(FATAL_ERROR "the server told her law ${_times} times, not once for aircraft "
                        "${_hers} in stage ${_stage}:\n${_err}")
endif()
if(NOT _said MATCHES "stovl: prediction begun on the server's LAW_STATE at [0-9.]+ s: stage ([0-9])")
    message(FATAL_ERROR "the client never began its prediction on the server's law:\n${_said}")
endif()
if(NOT CMAKE_MATCH_1 EQUAL _stage)
    message(FATAL_ERROR "the client was given her in stage ${CMAKE_MATCH_1}, not ${_stage}:\n"
                        "${_said}")
endif()
if(NOT _said MATCHES "stovl: predicted pitch within ([0-9]+)\\.([0-9][0-9][0-9]) degrees")
    message(FATAL_ERROR "the client did not say its pitch error:\n${_said}")
endif()
set(_pitch "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
math(EXPR _pitch_mdeg "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_pitch_mdeg GREATER 500)
    message(FATAL_ERROR "her predicted pitch was ${_pitch} degrees off the server's, over half "
                        "a degree:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
set(_compared "${CMAKE_MATCH_1}")
set(_worst "${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
math(EXPR _worst_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
if(_compared LESS 750)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
if(_worst_mm GREATER 5600)
    message(FATAL_ERROR "the worst prediction error was ${_worst} m, over 5.6 m:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error median: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its median:\n${_said}")
endif()
math(EXPR _median_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_median_mm GREATER 60)
    message(FATAL_ERROR "the median was ${_median_mm} mm, over 60 mm:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error 95th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its 95th percentile:\n${_said}")
endif()
math(EXPR _p95_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_p95_mm GREATER 3400)
    message(FATAL_ERROR "the 95th percentile was ${_p95_mm} mm, over 3.4 m:\n${_said}")
endif()
message(STATUS "${SCENE}: aircraft ${_hers} told in stage ${_stage}; pitch within ${_pitch} degrees, "
               "median ${_median_mm} mm, 95th percentile ${_p95_mm} mm, worst ${_worst} m "
               "over ${_compared} updates")

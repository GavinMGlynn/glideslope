# server_stovl_join.cmake - a client given an F-35B while her STOVL law
# already flies her predicts her from the server's law state (`LAW_STATE`).
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DSCENE=join
#         -P server_stovl_join.cmake
#
# **Built, not hoped for.** The server and plan of server_stovl_at_once.cmake.
# Client A asks for the F-35B at her vertical-landing loading and selects
# STOVL mode once its prediction has settled, the stick centred
# (`--stovl-land --stovl-level`), so that she converts at the height she
# was at, slows jet-borne and hovers. Then:
#
#   join       A holds the stick full right for two seconds 2,100 inputs
#              (70 s) after selecting (`--stovl-bank-at 2100`) - slowed from
#              her start, converted and jet-borne by then - and centres it:
#              her law holds the 20 degrees of bank that leaves. A leaves
#              without a word (`--no-goodbye`) once her law holds that bank
#              and the server has applied the input that centred the stick
#              (`--leave-once-bank-held`) - the event, not a time: left at
#              95 s, a slow Windows runner had not finished the roll, and B
#              was told 0 and 14 degrees - and writes that it has; client B, on A's key (`--key`), joins then
#              (`--after-ready`), before the server lets A's session go, and
#              is given her as she is - mid-bank, turning, jet-borne.
#
# **Not a take-over or a take-back**: server_stovl_take_over.cmake builds
# those.
#
# B predicts (`--predict --stovl-land --stovl-level`), its selector on, the
# stick centred, as A left them, through a relay that holds every datagram
# 100 ms each way; it stays until 750 updates are compared since it began on
# her law (`--until-compared`, counted afresh from there): the event, not a
# time.
#
# **And joining with the first sealed datagrams from the server lost**
# (`-DSCENE=lost`, impair's `--drop-first-sealed 6`): B's first updates and
# the law told with them are gone, the law sent again, and B joins on it
# all the same.
#
# **What must hold**: no wreck; the server said her law to B once
# (`aircraft N's STOVL law told to its client`), B heard it and began its
# prediction on it - jet-borne with the 20 degrees of bank held - and from
# its first update compared - told exactly the stage and bank the server
# says it held at the step of the message - its
# predicted pitch within half a degree of the server's at every update, the
# median within 60 mm, the 95th percentile within 3.4 m and the worst within
# 5.6 m: server_stovl_vertical_landing.cmake's bounds. Begun on a model just
# loaded, the fan's doors shut, B's prediction made its own law wing-borne
# and level (PROJECT_STATUS.md, 2026-10-22).
#
# **The lost scene's median within 120 mm, measured** (2026-10-29): 60 mm
# was copied, never measured for it, and CI's Windows runners went over it
# three times - 72 mm (windows-clang, run 40743161009), 77 and 63 mm
# (windows-debug, runs 40750172534 and 40769073518), the worst 0.74 to
# 1.26 m and the 95th percentile 0.59 to 0.65 m, inside their bounds. Its
# error is along her path, not off it (0.056 to 0.147 m off at worst):
# updates placed a step off by the clocks' difference, which Windows'
# timers spread (COMPLETION_PLAN, Phase 12a). Why the lost scene spreads it
# more than the join scene is not known. Five runs of each scene on
# Windows debug, the development machine: the lost scene's median 54 to
# 61 mm, the join scene's 35 to 48 mm, every 95th percentile 0.54 to
# 1.27 m. Linux debug, on a busy machine: 19 to 45 mm and 13 to 50 mm,
# the 95th percentiles 26 to 108 mm. 120 mm is half again Windows' worst, and half the 246 mm of a
# client begun without the law (docs/status, 2026-10-22), which the
# pitch check catches first besides.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.
cmake_minimum_required(VERSION 3.28)
if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/stovl.sqlite")
set(_heard_a "${WORK}/a.txt")
set(_heard_b "${WORK}/b.txt")
set(_gone "${WORK}/a-gone.txt")
set(_plan "${WORK}/stovl.plan")
file(REMOVE "${_store}" "${_heard_a}" "${_heard_b}" "${_gone}")
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
math(EXPR _relay "${PORT} + 1")
set(_player "e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26")
set(_b_flies --predict --stovl-land --stovl-level --until-compared 750 --heard "${_heard_b}")
set(_lose)
if(SCENE STREQUAL "lost")
    # **The first sealed datagrams from the server lost on their way to B**:
    # its first updates, and the law told with them, sent again by the
    # reliable layer; B joins all the same.
    set(_lose --drop-first-sealed 6)
endif()
if(SCENE STREQUAL "join" OR SCENE STREQUAL "lost")
    set(_a 600 --after 1 --key "${_player}"
               --aircraft f35b --loading vertical_landing --predict --stovl-land --stovl-level
               --stovl-bank-at 2100 --leave-once-bank-held --no-goodbye --done "${_gone}"
               --heard "${_heard_a}")
    set(_b 600 --after-ready "${_gone}"
               --key "${_player}" --aircraft f35b --loading vertical_landing ${_b_flies})
else()
    message(FATAL_ERROR "SCENE is join or lost, not '${SCENE}'")
endif()
# The addresses are written in the commands, not in set(): test_ports.cmake
# reads every port a script derives, and a set() naming one it cannot.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" ${_b}
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" ${_a}
    COMMAND "${SERVER}" --port ${PORT} --seconds 900 --until-empty --ai 1 --headless
            --players 2 --data "${DATA}" --timeout 30 --store "${_store}" --plan "${_plan}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 900 ${_lose}
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
list(REMOVE_ITEM _rcs 0)
if(_rcs)
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(_out MATCHES "is a wreck" OR _err MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was a wreck:\n${_out}\n${_err}")
endif()
if(SCENE STREQUAL "lost" AND NOT "${_out}${_err}" MATCHES "impair: 6 of the first 6 sealed datagrams from the server dropped")
    message(FATAL_ERROR "the relay did not drop the first six sealed datagrams:\n${_out}\n${_err}")
endif()
file(READ "${_heard_b}" _said)
# The server's stdout reaches standard error through the pipe, a line at a
# time.
string(REGEX MATCHALL "aircraft [0-9]+'s STOVL law told to its client" _told "${_err}")
list(LENGTH _told _times)
if(NOT _times EQUAL 1)
    message(FATAL_ERROR "the server told her law ${_times} times, not once:\n${_err}")
endif()
if(NOT _said MATCHES "stovl: prediction begun on the server's LAW_STATE at [0-9.]+ s: stage ([0-9]), bank held ([-0-9.]+) degrees")
    message(FATAL_ERROR "B never began its prediction on the server's law:\n${_said}")
endif()
set(_stage "${CMAKE_MATCH_1}")
set(_bank "${CMAKE_MATCH_2}")
# **Begun on the law told, not on giving up waiting for it.**
if(NOT _said MATCHES "stovl: waited for the server's law [01] times; 1 LAW_STATE told; gave up waiting 0 times")
    message(FATAL_ERROR "B was told the law once and never gave up waiting for it:\n${_said}")
endif()
# **What the server held at the step of the message, to the digit**: not
# only some 20 degrees, but the server's own.
if(NOT _err MATCHES "STOVL law told to its client at [0-9.]+ s: stage ([0-9]+), bank held ([-0-9.]+) degrees")
    message(FATAL_ERROR "the server did not say what it told:\n${_err}")
endif()
if(NOT _stage STREQUAL CMAKE_MATCH_1 OR NOT _bank STREQUAL CMAKE_MATCH_2)
    message(FATAL_ERROR "B was told stage ${_stage}, ${_bank} degrees of bank held; the server "
                        "held stage ${CMAKE_MATCH_1}, ${CMAKE_MATCH_2} degrees:\n${_said}")
endif()
# **And the situation built**: A left once her law held its bank and the
# server had the input that centred the stick, jet-borne, 20 degrees.
file(READ "${_heard_a}" _a_said)
if(NOT _a_said MATCHES "stovl: left once her bank was held, ([-0-9.]+) degrees")
    message(FATAL_ERROR "A did not leave on her bank held:\n${_a_said}")
endif()
if(NOT _stage EQUAL 2 OR NOT _bank STREQUAL "20.000" OR NOT CMAKE_MATCH_1 STREQUAL "20.000")
    message(FATAL_ERROR "B was given her in stage ${_stage} with ${_bank} degrees of bank held, "
                        "A having left at ${CMAKE_MATCH_1}: not jet-borne with 20:\n${_said}")
endif()
if(NOT _said MATCHES "stovl: predicted pitch within ([0-9]+)\\.([0-9][0-9][0-9]) degrees")
    message(FATAL_ERROR "B did not say its pitch error:\n${_said}")
endif()
set(_pitch "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
math(EXPR _pitch_mdeg "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_pitch_mdeg GREATER 500)
    message(FATAL_ERROR "her predicted pitch was ${_pitch} degrees off the server's, over half "
                        "a degree:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "B did not say its prediction error:\n${_said}")
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
if(NOT _said MATCHES "prediction error 95th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "B did not say its 95th percentile:\n${_said}")
endif()
math(EXPR _p95_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_p95_mm GREATER 3400)
    message(FATAL_ERROR "the 95th percentile was ${_p95_mm} mm, over 3.4 m:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error median: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "B did not say its median error:\n${_said}")
endif()
math(EXPR _median_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
# **The lost scene's median, measured** (2026-10-29): see the header.
set(_median_bound_mm 60)
if(SCENE STREQUAL "lost")
    set(_median_bound_mm 120)
endif()
if(_median_mm GREATER _median_bound_mm)
    message(FATAL_ERROR "the median prediction error was ${_median_mm} mm, over "
                        "${_median_bound_mm} mm:\n${_said}")
endif()
message(STATUS "${SCENE}: B began on her law in stage ${_stage} with ${_bank} degrees of bank "
               "held; over ${_compared} updates her pitch within ${_pitch} degrees, the median "
               "${_median_mm} mm, the 95th percentile ${_p95_mm} mm, the worst ${_worst} m")

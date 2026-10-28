# server_stovl_at_once.cmake - a player who joins already selecting STOVL
# mode: her client's prediction seeds her transition law as the server does.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P server_stovl_at_once.cmake
#
# **Built, not hoped for.** The server and plan of server_stovl_landing.cmake:
# one AI, the player started at 2,200 ft over Sydney airport heading west,
# her F-35B at her model's own loading, too heavy to hover, so that her law
# is made wing-borne with its autopilot. The command-line client joins
# through a relay that holds every datagram 100 ms each way, asking for the
# F-35B and predicting her (`--predict --stovl-land`), and selects STOVL
# mode with its very first input (`--stovl-at-once`) - joining as the
# windowed client does: it asks the server for the levers it holds her on
# (`WATCH` of its own) and sends nothing until it has them. Until her first
# input the server holds her on what her trim found; the law it makes on
# that input is seeded from those controls (sim::Controller), and the
# prediction's, made on the same input, from the levers it was told
# (sim::Prediction::held_by_server). Before that the prediction's law was
# seeded from a model just loaded, its throttle integral nought where the
# server's was her trim's (PROJECT_STATUS.md, 2026-10-14). The client stays
# until 3,000 updates are compared (`--until-compared`), two minutes held
# wing-borne: the event, not a time.
#
# **What must hold**: no wreck heard; the client selected with input 1, the
# server's levers said; the server says the throttle integral its law was
# made with (`aircraft N's STOVL law made at step S`), and the client's
# prediction made its law on input 1 with the same throttle integral, to
# the printed 17 digits - the levers are 16-bit fractions, and the server
# holds her on them to the bit (glideslope_server, `Fleet::give`). And the
# law flying her prediction when it leaves was made with that integral too:
# each word puts her back and flies her again, the law made again in the
# replay, and a law made again from the newest step's controls rather than
# from those she had as the step began would be seeded otherwise. **And her
# predicted pitch within PITCH_BOUND_MDEG thousandths of a degree of the
# server's at every update compared** - twice the most each test was seen
# off on any platform (tests/CMakeLists.txt says which, from which run), as server_stovl_landing.cmake holds it - here with her
# selector on before the prediction has settled, before any word of the
# server's had placed it on the client's clock (COMPLETION_PLAN, "A
# client's prediction of an F-35B in STOVL mode drifts from the server's
# flight-control laws").
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
file(REMOVE "${_store}" "${_heard}" "${WORK}/figures.txt")
if(NOT DEFINED PITCH_BOUND_MDEG)
    message(FATAL_ERROR "server_stovl_at_once.cmake needs -DPITCH_BOUND_MDEG")
endif()
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
set(_slow "")
if(DEFINED SLOW_MS)
    set(_slow --slow-preparation ${SLOW_MS})
endif()
if(SPEND)
    list(APPEND _slow --spend-requests-first)
endif()
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 900 --after 1
            --aircraft f35b --predict --stovl-land --stovl-at-once --until-compared 3000
            --heard "${_heard}" ${_slow}
    COMMAND "${SERVER}" --port ${PORT} --seconds 1000 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 30 --store "${_store}" --plan "${_plan}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 1000
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(_out MATCHES "is a wreck" OR _err MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was a wreck:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
# Built, not hoped for (`SPEND`): the server says it ignored a request of
# this client's past its rate - the first asking for her levers.
if(SPEND AND NOT _err MATCHES "dropped past [0-9]+ a second, [1-9][0-9]* of [0-9]+ requests ignored")
    message(FATAL_ERROR "the server ignored none of the client's requests, so its first asking "
                        "for her levers was answered:\n${_err}")
endif()
if(NOT _said MATCHES "stovl: selected with input 1, at once, on the server's levers \\(said\\)")
    message(FATAL_ERROR "the client did not select STOVL mode with its first input, on the "
                        "server's levers:\n${_said}\n${_err}")
endif()
# The server's stdout reaches standard error through the relay, a line at a
# time.
if(NOT _err MATCHES "aircraft [0-9]+'s STOVL law made at step ([0-9]+): throttle integral ([-0-9.e+]+); pitch trim ([-0-9.e+]+)\n")
    message(FATAL_ERROR "the server never said it made her STOVL law:\n${_err}")
endif()
set(_server_step "${CMAKE_MATCH_1}")
set(_server "${CMAKE_MATCH_2}")
set(_server_trim "${CMAKE_MATCH_3}")
if(NOT _said MATCHES "stovl: law made with input ([0-9]+) at step ([0-9]+): throttle integral ([-0-9.e+]+); the law flying her made with ([-0-9.e+]+); pitch trim ([-0-9.e+]+), flying her ([-0-9.e+]+)")
    message(FATAL_ERROR "the client never said it made her STOVL law:\n${_said}")
endif()
set(_input "${CMAKE_MATCH_1}")
set(_client_step "${CMAKE_MATCH_2}")
set(_client "${CMAKE_MATCH_3}")
set(_flying "${CMAKE_MATCH_4}")
set(_client_trim "${CMAKE_MATCH_5}")
set(_flying_trim "${CMAKE_MATCH_6}")
if(NOT _input EQUAL 1)
    message(FATAL_ERROR "the prediction made her law on input ${_input}, not her first:\n${_said}")
endif()
if(NOT _client STREQUAL _server)
    message(FATAL_ERROR "the prediction's law was made with a throttle integral of ${_client}, "
                        "the server's with ${_server}:\n${_said}")
endif()
if(NOT _flying STREQUAL _server)
    message(FATAL_ERROR "the law flying her prediction at the end was made with a throttle "
                        "integral of ${_flying}, the server's with ${_server}:\n${_said}")
endif()
# **And the pitch trim each law flies her on**, which the law keeps from the
# controls it was made from: the server's made from her first input's
# (nought), the prediction's from the trim the server held her on (0.075),
# her prediction flew every replay on another trim than the server's, and
# pitched away from it at 0.0055 degree a step - 0.58 and 0.96 degrees on
# CI's Windows, whose replays are longer (docs/status, 2026-10-28).
# Not nought: her trim found one, and a check of two noughts would pass
# whichever controls the laws were made from.
if(_server_trim STREQUAL "0" OR _server_trim MATCHES "^-?0(\\.0*)?(e[-+]?[0-9]+)?$")
    message(FATAL_ERROR "the server's law flew her on a pitch trim of ${_server_trim}, nought: "
                        "her trim found none, and the trims compared say nothing:\n${_err}")
endif()
if(NOT _client_trim STREQUAL _server_trim OR NOT _flying_trim STREQUAL _server_trim)
    message(FATAL_ERROR "the prediction's law flew her on a pitch trim of ${_client_trim} "
                        "(${_flying_trim} at the end), the server's on ${_server_trim}:\n${_said}")
endif()
message(STATUS "her STOVL law made on her first input with a throttle integral of ${_server} "
               "on the server (its step ${_server_step}) and in the prediction (its step "
               "${_client_step}), and the law flying her at the end made with the same; "
               "both flying her on a pitch trim of ${_server_trim}")
if(NOT _said MATCHES "stovl: predicted pitch within ([0-9]+)\\.([0-9][0-9][0-9]) degrees")
    message(FATAL_ERROR "the client did not say its pitch error:\n${_said}")
endif()
# **Said as a figure before it is judged**, for CI's "The figures said" step:
# the bound is set from every platform's runs, not from those that failed it.
# Into a file of its own as well as the output: a test that runs ctest in the
# same build (the ports check, the shards check) writes LastTest.log afresh,
# and the figures of the tests before it in its shard went with it (run
# 40733120388: Rocky's and one of Ubuntu debug's).
get_filename_component(_which "${WORK}" NAME)
set(_figure "figure (${_which}): predicted pitch within ${CMAKE_MATCH_1}.${CMAKE_MATCH_2} degrees")
message(STATUS "${_figure}")
file(WRITE "${WORK}/figures.txt" "${_figure}\n")
math(EXPR _mdeg "${CMAKE_MATCH_1} * 1000 + 1${CMAKE_MATCH_2} - 1000")
if(_mdeg GREATER PITCH_BOUND_MDEG)
    message(FATAL_ERROR "her predicted pitch was ${CMAKE_MATCH_1}.${CMAKE_MATCH_2} degrees off "
                        "the server's, over ${PITCH_BOUND_MDEG} thousandths:\n${_said}")
endif()
set(_pitch "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared")
    message(FATAL_ERROR "the client did not say how many updates it compared:\n${_said}")
endif()
if(CMAKE_MATCH_1 LESS 3000)
    message(FATAL_ERROR "only ${CMAKE_MATCH_1} updates compared, not 3,000:\n${_said}")
endif()
message(STATUS "selected before the prediction had settled, her predicted pitch within "
               "${_pitch} degrees of the server's over ${CMAKE_MATCH_1} updates")

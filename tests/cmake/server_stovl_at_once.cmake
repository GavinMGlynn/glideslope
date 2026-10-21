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
# until 250 updates are compared (`--until-compared`): the event, not a
# time.
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
# from those she had as the step began would be seeded otherwise.
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
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 900 --after 1
            --aircraft f35b --predict --stovl-land --stovl-at-once --until-compared 250
            --heard "${_heard}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 1000 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" --plan "${_plan}"
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
if(NOT _said MATCHES "stovl: selected with input 1, at once, on the server's levers \\(said\\)")
    message(FATAL_ERROR "the client did not select STOVL mode with its first input, on the "
                        "server's levers:\n${_said}\n${_err}")
endif()
# The server's stdout reaches standard error through the relay, a line at a
# time.
if(NOT _err MATCHES "aircraft [0-9]+'s STOVL law made at step ([0-9]+): throttle integral ([-0-9.e+]+)\n")
    message(FATAL_ERROR "the server never said it made her STOVL law:\n${_err}")
endif()
set(_server_step "${CMAKE_MATCH_1}")
set(_server "${CMAKE_MATCH_2}")
if(NOT _said MATCHES "stovl: law made with input ([0-9]+) at step ([0-9]+): throttle integral ([-0-9.e+]+); the law flying her made with ([-0-9.e+]+)")
    message(FATAL_ERROR "the client never said it made her STOVL law:\n${_said}")
endif()
set(_input "${CMAKE_MATCH_1}")
set(_client_step "${CMAKE_MATCH_2}")
set(_client "${CMAKE_MATCH_3}")
set(_flying "${CMAKE_MATCH_4}")
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
message(STATUS "her STOVL law made on her first input with a throttle integral of ${_server} "
               "on the server (its step ${_server_step}) and in the prediction (its step "
               "${_client_step}), and the law flying her at the end made with the same")

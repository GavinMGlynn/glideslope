# server_slowed_prediction.cmake - a client predicting its own aircraft
# against a server behind real time is off by centimetres, not metres.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DDATA=<data dir> -DCACHE=<downloads dir> -DWORK=<scratch>
#         -DPORT=<a port> -P server_slowed_prediction.cmake
#
# **The situation is built**: every step of the server is made to take 10 ms
# (`--test-step-ms 10`) where real time gives it 8.3 - about 80% of real
# time in a release build, 57% in a sanitized one: a loaded CI runner's debug
# server, made on purpose - and a client joins it, predicting its own
# aircraft (`connect --predict --fly`) for twenty seconds of its own clock.
# The server must say it ended behind, and the client's updates must span
# under 85% of the twenty seconds it stayed: or the server was not slowed,
# and the bound below tested nothing. (Not slower: the session clock's rate
# is held to a half and more (net::SessionClock), so a server under half of
# real time is still flown here too fast.)
#
# **What must hold**: the client's median prediction error, once the clocks'
# difference is known, under 10 cm, and its worst under 2 m. Flown at this
# machine's pace, each of its inputs flown here for more steps than the
# server flew it, they were 1.3 and 4.1 m here, and 2.3 and 5.6 m at half of
# real time; flown at the session's pace (net::SessionClock::rate), nought
# and 0.46 to 0.84 m (PROJECT_STATUS.md, 2026-10-07). The worst is not held
# to a metre: half a second at a time, every few seconds, a predicting client
# is put right by a metre or so whatever the server's pace - the clocks'
# estimate, an open tail.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/slowed.sqlite")
set(_heard "${WORK}/heard.txt")
set(_ready "${WORK}/flying")
file(REMOVE "${_store}" "${_heard}" "${_ready}")
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

set(_stay 20)
# The server is last, so its words are on standard output; the client's are
# in its --heard file.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" ${_stay} --fly --predict
            --after-ready "${_ready}" --heard "${_heard}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" --ready-file "${_ready}"
            --test-step-ms 10
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)

# Behind, as built.
if(NOT _out MATCHES "was ([0-9]+) steps behind at the end")
    message(FATAL_ERROR "the server did not say how far behind it was:\n${_out}")
endif()
set(_behind "${CMAKE_MATCH_1}")
if(NOT _said MATCHES "state updates from step ([0-9]+) to step ([0-9]+)")
    message(FATAL_ERROR "the client did not say which steps it heard of:\n${_said}")
endif()
math(EXPR _spanned "${CMAKE_MATCH_2} - ${CMAKE_MATCH_1}")
math(EXPR _real "${_stay} * 120 * 85 / 100")
if(_spanned GREATER_EQUAL _real)
    message(FATAL_ERROR "the client heard of ${_spanned} steps in its ${_stay} s, not under "
                        "85% of real time's: the server was not slowed:\n${_said}")
endif()

if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say how far off its prediction was:\n${_said}")
endif()
set(_compared "${CMAKE_MATCH_1}")
math(EXPR _worst_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
if(_compared LESS 100)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error median: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its median prediction error:\n${_said}")
endif()
math(EXPR _median_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_median_mm GREATER_EQUAL 100 OR _worst_mm GREATER_EQUAL 2000)
    message(FATAL_ERROR "against a server at ${_spanned} steps in ${_stay} s, the median "
                        "prediction error was ${_median_mm} mm and the worst ${_worst_mm} mm, "
                        "the bounds 100 and 2000:\n${_said}")
endif()
message(STATUS "against a server ${_behind} steps behind at the end, ${_spanned} steps in "
               "${_stay} s, the median prediction error was ${_median_mm} mm and the worst "
               "${_worst_mm} mm over ${_compared} updates")

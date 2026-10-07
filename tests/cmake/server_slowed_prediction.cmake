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
# aircraft (`connect --predict --fly`) until 300 of its updates have been
# compared with what it predicted (`--until-compared 300`): the event, not a
# time - a 20 s stay compared only 59 and 86 on CI's macOS runners. The
# server must say it ended two seconds behind and more: or it was not
# slowed, and the bound below tested nothing. (Not slower: the session
# clock's rate is held to a half and more (net::SessionClock), so a server
# under half of real time is still flown here too fast.)
#
# **What must hold**: the client's median prediction error, once the clocks'
# difference is known, under 10 cm. Flown at this
# machine's pace, each of its inputs flown here for more steps than the
# server flew it, they were 1.3 and 4.1 m here, and 2.3 and 5.6 m at half of
# real time; flown at the session's pace (net::SessionClock::rate), nought
# and 0.46 to 0.84 m (PROJECT_STATUS.md, 2026-10-07). **The worst is said,
# not held**: half a second at a time, every few seconds, a predicting client
# is put right by a metre or more whatever the server's pace - the clocks'
# estimate, an open tail - and CI's Windows runners measured 2.8 and 4.2 m
# with the median 1 mm (run 37593355936).
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

# The server is last, so its words are on standard output; the client's are
# in its --heard file.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 600 --fly --predict
            --until-compared 300 --after-ready "${_ready}" --heard "${_heard}"
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
if(_behind LESS 240)
    message(FATAL_ERROR "the server was only ${_behind} steps behind at the end: not "
                        "slowed, and the bound tests nothing:\n${_out}")
endif()

if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say how far off its prediction was:\n${_said}")
endif()
set(_compared "${CMAKE_MATCH_1}")
math(EXPR _worst_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
if(_compared LESS 300)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error median: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its median prediction error:\n${_said}")
endif()
math(EXPR _median_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_median_mm GREATER_EQUAL 100)
    message(FATAL_ERROR "against a server ${_behind} steps behind, the median prediction "
                        "error was ${_median_mm} mm (the worst ${_worst_mm} mm), the bound "
                        "100:\n${_said}")
endif()
message(STATUS "against a server ${_behind} steps behind at the end, the median prediction "
               "error was ${_median_mm} mm and the worst ${_worst_mm} mm over ${_compared} "
               "updates")

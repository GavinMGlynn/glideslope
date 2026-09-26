# server_take_over.cmake - a player takes over an AI aircraft through a worse
# network with no step; a player's aircraft, or on a server that forbids it,
# is refused.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DDELAY=<ms> -DJITTER=<ms> -DLOSS=<percent>
#         -DSEED=<n> -DPREDICT_M=<m> -P server_take_over.cmake
#
# **Built, not hoped for.** A server with room for three players and one AI
# Cessna, numbered 4. The first player joins straight to it and is given
# aircraft 0; the second, through `glideslope_impair` (DELAY ms, JITTER ms
# more, LOSS% lost), predicting, rides along in the AI and is given aircraft 1;
# the third joins straight to it three seconds after the second, is given
# aircraft 2, and five seconds in hands it to the AI pilot. (Seconds are the
# client's own, counted from when it joined.)
#
# - Six seconds in, the third asks to take over aircraft 1 - the second
#   player's, flown by hand. From seven seconds in, and once an update has
#   shown the AI flying it, the first asks for aircraft 2 - the third
#   player's. The server must refuse both: never a player's, whoever is
#   flying it.
# - Fourteen seconds in, after both, the second takes over the AI's. The
#   server must say so, the client that its aircraft is now that one, and
#   what it showed must not step across the change - under 5 m in a frame, the bound the swaps are
#   held to - with nothing corrected too far to hide. **And the server must be
#   flying it by that client's inputs**: the updates after it, which the
#   server sends only once it has applied an input sent since, must number at
#   least fifty, with the prediction error within PREDICT_M.
#
# Then a server started with `--no-take-over` must refuse a take-over outright.
#
# **Not built here**: a request for a wreck, or for an aircraft nobody at all
# is flying. The AI's aircraft flies its plan well clear of the ground for the
# length of a test, and every aircraft a server runs is flown by a player or
# the AI; each refusal is a line of `Fleet::take_over` beside the two built.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/take-over.sqlite")
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

math(EXPR _relay "${PORT} + 1")
set(_taker "${WORK}/taker.txt")
set(_other "${WORK}/other.txt")
set(_third "${WORK}/third.txt")
file(REMOVE "${_taker}" "${_other}" "${_third}")
execute_process(
    # Each before one that outlives it (server_impaired.cmake says why).
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 22 --after 2
            --predict --watch-ai --take-over-at 14 --heard "${_taker}"
            --long-frame-after-switch --late-update-after-take-over
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 22 --after 5
            --hand-over-at 5 --take-over-at 6 --take-over-aircraft 1 --heard "${_third}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 28 --after 1
            --take-over-at 7 --take-over-aircraft 2 --once-the-ai-flies-it --heard "${_other}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 3 --data "${DATA}" --timeout 3 --store "${_store}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay ${DELAY} --jitter ${JITTER}
            --loss ${LOSS} --seed ${SEED} --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()

# **A player's aircraft is refused**, flown by hand or by the AI: what the
# server said, which the relay passed on.
file(READ "${_third}" _third_said)
if(NOT _third_said MATCHES "aircraft 2 handed to the AI")
    message(FATAL_ERROR "the third player's aircraft was not handed to the AI:\n${_third_said}")
endif()
foreach(_asked IN ITEMS 2 1)
    if(NOT _err MATCHES "aircraft ${_asked} not taken over: aircraft ${_asked} is a player's")
        message(FATAL_ERROR "the server did not refuse player's aircraft ${_asked}:\n${_err}")
    endif()
endforeach()
file(READ "${_other}" _other_said)
if(_other_said MATCHES "took over" OR _third_said MATCHES "took over")
    message(FATAL_ERROR "a player took over a player's aircraft:\n${_other_said}\n${_third_said}")
endif()

# **The AI's is taken over**, and the client told.
if(NOT _err MATCHES "aircraft ([0-9]+) taken over; aircraft ([0-9]+), left, now the AI's")
    message(FATAL_ERROR "the server never said the AI's aircraft was taken over:\n${_err}")
endif()
set(_taken "${CMAKE_MATCH_1}")
file(READ "${_taker}" _said)
if(NOT _said MATCHES "took over aircraft ${_taken}\n")
    message(FATAL_ERROR "the client never heard it had taken over aircraft ${_taken}:\n${_said}")
endif()

# **With no step**, and flown on as its own.
if(NOT _said MATCHES "another taken over 1; the largest step at a switch ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say how it showed the change:\n${_said}")
endif()
set(_step "${CMAKE_MATCH_1}")
if(_step GREATER_EQUAL 5)
    message(FATAL_ERROR "what it showed stepped ${_step} m across the take-over:\n${_said}")
endif()
# **An update from before the take-over, heard after it**, as a network that
# reorders them delivers one - built every run, not left to the relay's
# jitter, which did it one run in several: "took over aircraft 4, then 1,
# then 4" (2026-09-26). It is not taken for a take-over back.
string(REGEX MATCHALL "an update from before the take-over is heard again after it" _late "${_said}")
list(LENGTH _late _late)
if(NOT _late EQUAL 1)
    message(FATAL_ERROR "an update from before the take-over was heard again ${_late} times, "
                        "not once:\n${_said}")
endif()
# With a long frame of 0.4 s after every switch (server_impaired.cmake says
# why), at least the take-over one, and at most one overtaken: prediction
# starting a frame or two after it.
if(NOT _said MATCHES "long frames: one of 0.4 s built after ([0-9]+) of ([0-9]+) switches, and ([0-9]+) overtaken")
    message(FATAL_ERROR "the client did not say its long frames:\n${_said}")
endif()
math(EXPR _had "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3}")
if(NOT _had EQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_3 GREATER 1 OR CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "a long frame was built after ${CMAKE_MATCH_1} of ${CMAKE_MATCH_2} "
                        "switches, ${CMAKE_MATCH_3} overtaken by another:\n${_said}")
endif()
if(NOT _said MATCHES "predicted: [0-9]+ corrections, the worst ([0-9.]+) m, 0 too large to hide")
    message(FATAL_ERROR "a correction was too large to hide:\n${_said}")
endif()
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
if(CMAKE_MATCH_2 GREATER_EQUAL PREDICT_M)
    message(FATAL_ERROR "the prediction error was ${CMAKE_MATCH_2} m, the bound ${PREDICT_M}:\n${_said}")
endif()
if(NOT _said MATCHES "after the take-over: ([0-9]+) updates compared, the worst ([0-9.]+) m, ([0-9]+) corrections too large")
    message(FATAL_ERROR "the client did not say how it flew after the take-over:\n${_said}")
endif()
set(_since "${CMAKE_MATCH_1}")
set(_since_m "${CMAKE_MATCH_2}")
if(_since LESS 50 OR _since_m GREATER_EQUAL PREDICT_M OR NOT CMAKE_MATCH_3 EQUAL 0)
    message(FATAL_ERROR "after the take-over ${_since} updates were compared, the worst "
                        "${_since_m} m, ${CMAKE_MATCH_3} too large to hide: the server is not "
                        "flying it by this client's inputs, or not within bound:\n${_said}")
endif()

# **A server that forbids it refuses.**
set(_forbidden "${WORK}/forbidden.txt")
file(REMOVE "${_forbidden}")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 8 --after 1 --predict
            --take-over-at 3 --heard "${_forbidden}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --no-take-over --data "${DATA}" --timeout 3 --store "${_store}"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _served ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_served}\n${_err}")
endif()
if(NOT _served MATCHES "not taken over: this server does not allow it")
    message(FATAL_ERROR "a server started with --no-take-over did not refuse:\n${_served}\n${_err}")
endif()
file(READ "${_forbidden}" _refused)
if(_refused MATCHES "took over")
    message(FATAL_ERROR "a client took over on a server that forbids it:\n${_refused}")
endif()
message(STATUS "through ${DELAY} ms: took over aircraft ${_taken}, a step of ${_step} m, then "
               "${_since} updates within ${_since_m} m; a player's refused, flown by hand and "
               "by the AI, and a server that forbids it refused")

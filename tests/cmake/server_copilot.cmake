# server_copilot.cmake - a player's copilot asked on the player's machine,
# and its route flown by the server; or a route the server cannot fly,
# refused by it.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         (-DPLAYBACK=<recording> | -DRECORD=<recording>
#          | -DROUTE=<route file> -DEXPECT=<regex> [-DFOR=another|wreck] [-DPLAN=<plan>])
#         [-DENGINE_AT=<s>] [-DTAKE_BACK_AT=<s>]
#         [-DPROVIDER=openai|anthropic -DMODEL=<model>]
#         -P server_copilot.cmake
#
# **Asked of the model** (PLAYBACK or RECORD): the client flies its own
# aircraft, predicting it, and five seconds into the session asks its copilot
# - with the player's key, on this side, or from the recording - to fly to
# Manly and orbit over it. The route it answers goes to the server as a
# `COPILOT_ROUTE`, and nothing else does. The server must hand the aircraft
# to the AI (the client must hear it), say it flies the copilot's route, and
# fly it: of the lines it prints each half minute, the last must be nearer the
# waypoint it is flying to than the first, or past it. The client stays 60 s
# of the session's clock after sending it, then leaves, and the server runs
# until it has gone.
#
# **With ENGINE_AT**, the server stops the player's engine that many seconds
# in (`--fail-engine-at`), and says so in its updates; the client's copilot
# is asked again, "the engine has stopped", and must answer with a glide the
# server flies: its last half-minute line nearer the glide's waypoint than
# its first after the failure, at the glide's airspeed within 5 kt.
#
# **With TAKE_BACK_AT**, the player takes the aircraft back that many seconds
# after joining, and the copilot, asked for a routine look every 20 s, must
# stand by from then: the answer that comes after the take-back must not be
# sent, and the server hands the aircraft to the AI once, not again. The
# recording played back for it (data/copilot/take_back-server-anthropic.jsonl)
# is Claude Haiku's, recorded, but for its second answer, which was `keep`
# and is written in by hand as a route - its line says so, in a "note" the
# playback does not read - since a `keep` is sent by nobody, and would test
# nothing. Asked now, a model that answers `keep` there makes the test skip.
#
# **Sent as it is** (ROUTE): a route the client's copilot never checked, one
# the server cannot fly. The server must refuse it, saying why (EXPECT), and
# neither hand the aircraft to the AI nor fly it - and go on: both programs
# end as they should. FOR=another sends it for another player's aircraft - a
# second client's, joined first and flying its own - and
# FOR=wreck once the client's own is a wreck (it dives into the sea, as
# server_swap_wreck.cmake's does); PLAN gives the server a plan whose aircraft
# - the players' too - is one whose figures give no speeds to check against.
#
# Asked of PROVIDER now costs money, so only with GLIDESLOPE_LIVE_MODEL=1; else
# skipped (exit 77), before anything else. Without the DEM's tiles, skipped.

cmake_minimum_required(VERSION 3.28)

if(DEFINED RECORD AND NOT "$ENV{GLIDESLOPE_LIVE_MODEL}" STREQUAL "1")
    message(STATUS "${PROVIDER} is not asked: a live model call costs money, and is made only "
                   "with GLIDESLOPE_LIVE_MODEL=1 set")
    cmake_language(EXIT 77)
endif()

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/copilot.sqlite")
set(_heard "${WORK}/heard.txt")
file(REMOVE "${_store}" "${_heard}")

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

set(_plan)
if(DEFINED PLAN)
    set(_plan --plan "${PLAN}")
endif()
if(DEFINED ROUTE)
    set(_asking --send-route "${ROUTE}" --copilot-stay 10)
    if(FOR STREQUAL "another")
        list(APPEND _asking --route-for-another)
    elseif(FOR STREQUAL "wreck")
        list(APPEND _asking --route-when-wrecked --dive-after 1)
    endif()
else()
    set(_asking --copilot c172p "fly to Manly at 3,000 ft, then orbit over Manly beach"
                --copilot-provider ${PROVIDER} --copilot-stay 60)
    if(DEFINED ENGINE_AT)
        list(APPEND _asking --copilot-answers 2 --copilot-stay 100)
        list(APPEND _plan --fail-engine-at ${ENGINE_AT})
    endif()
    if(DEFINED TAKE_BACK_AT)
        list(APPEND _asking --copilot-routine 20 --take-back-at ${TAKE_BACK_AT})
    endif()
    if(DEFINED MODEL)
        list(APPEND _asking --copilot-model "${MODEL}")
    endif()
    if(DEFINED PLAYBACK)
        list(APPEND _asking --copilot-playback "${PLAYBACK}")
    else()
        list(APPEND _asking --copilot-record "${RECORD}")
    endif()
endif()

# The client first and the server last, so that what the server prints is
# what comes out of the pipeline. SECONDS, 300, is only the most the client
# waits: it leaves on the session's clock. With FOR=another, another player
# first, flying its own aircraft, whose number the route is sent for.
set(_other)
set(_players)
if(FOR STREQUAL "another")
    # (A list, not a set(): the address holds ${PORT} itself, and
    # test_ports.cmake reads a set() of it as a port derived.)
    list(APPEND _other COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}"
                "${_key}" 40 --fly)
    set(_players --players 2)
endif()
execute_process(
    ${_other}
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 300 --after 1
            --predict --copilot-at 5 ${_asking} --heard "${_heard}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 400 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" ${_plan} ${_players}
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _served ERROR_VARIABLE _err)
if(NOT EXISTS "${_heard}")
    message(FATAL_ERROR "the client heard nothing (exit codes ${_rcs}):\n${_served}\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _rcs STREQUAL "0;0" AND NOT _rcs STREQUAL "0;0;0")
    if(DEFINED RECORD AND _err MATCHES "no (OpenAI|Anthropic) key|no credits|credit balance|insufficient_quota")
        message(STATUS "${PROVIDER} cannot be asked here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_said}\n${_served}\n${_err}")
endif()
if(NOT _said MATCHES "sent its copilot's route of ([0-9]+)")
    message(FATAL_ERROR "the client sent no route:\n${_said}\n${_err}")
endif()

if(DEFINED ROUTE)
    if(NOT _served MATCHES "aircraft [0-9]+: a copilot's route refused: ([^\n]*)")
        message(FATAL_ERROR "the server did not refuse the route:\n${_served}")
    endif()
    set(_why "${CMAKE_MATCH_1}")
    if(DEFINED EXPECT AND NOT _why MATCHES "${EXPECT}")
        message(FATAL_ERROR "the server refused the route, but not because ${EXPECT}: ${_why}")
    endif()
    if(_served MATCHES "flies its copilot's route" OR _served MATCHES "handed to the AI")
        message(FATAL_ERROR "the server refused the route and flew it anyway:\n${_served}")
    endif()
    message(STATUS "the server refused the route: ${_why}")
    return()
endif()

foreach(_what IN ITEMS "asked its copilot, the pilot has asked" "its copilot answered with a route")
    if(NOT _said MATCHES "${_what}")
        message(FATAL_ERROR "the client never said \"${_what}\":\n${_said}\n${_err}")
    endif()
endforeach()
if(NOT _said MATCHES "aircraft [0-9]+ handed to the AI")
    message(FATAL_ERROR "the client never heard its aircraft handed to the AI:\n${_said}")
endif()
if(NOT _served MATCHES "aircraft ([0-9]+) flies its copilot's route of [0-9]+:([^\n]*)")
    message(FATAL_ERROR "the server never flew the copilot's route:\n${_served}")
endif()
set(_names "${CMAKE_MATCH_2}")
if(_served MATCHES "copilot's route refused" AND NOT REFUSED_FIRST)
    message(FATAL_ERROR "the server refused the copilot's route:\n${_served}")
endif()
# **With REFUSED_FIRST**, the engine stops while the first question is out:
# its route, no glide, must be refused by the server, and the copilot asked
# about the engine all the same.
if(REFUSED_FIRST AND NOT _served MATCHES "copilot's route refused: the engine has stopped")
    message(FATAL_ERROR "the first route was not refused for the stopped engine, so this "
                        "tested nothing:\n${_served}")
endif()
# Its progress, each half minute: the first line and the last - but for a
# route its pilot took back, which ends when it is.
if(NOT DEFINED TAKE_BACK_AT)
    string(REGEX MATCHALL "on its copilot's route: to [A-Za-z0-9_]+, [0-9]+ of [0-9]+, [0-9]+ m"
           _progress "${_served}")
    list(LENGTH _progress _n)
    if(_n LESS 2)
        message(FATAL_ERROR "the server said where the route had got to ${_n} times, not twice:\n"
                            "${_served}")
    endif()
    list(GET _progress 0 _first)
    list(GET _progress -1 _last)
    string(REGEX MATCH "to ([A-Za-z0-9_]+), ([0-9]+) of [0-9]+, ([0-9]+) m" _m "${_first}")
    set(_first_to "${CMAKE_MATCH_1}")
    set(_first_leg "${CMAKE_MATCH_2}")
    set(_first_m "${CMAKE_MATCH_3}")
    string(REGEX MATCH "to ([A-Za-z0-9_]+), ([0-9]+) of [0-9]+, ([0-9]+) m" _m "${_last}")
    set(_last_to "${CMAKE_MATCH_1}")
    set(_last_leg "${CMAKE_MATCH_2}")
    set(_last_m "${CMAKE_MATCH_3}")
    if(_last_to STREQUAL _first_to AND _last_leg EQUAL _first_leg AND NOT _last_m LESS _first_m)
        message(FATAL_ERROR "the aircraft came no nearer its waypoint: ${_first}, then ${_last}\n"
                            "${_served}")
    endif()
    message(STATUS "the server flew the copilot's route,${_names}: ${_first}; then ${_last}")
endif()

if(DEFINED TAKE_BACK_AT)
    string(REGEX MATCHALL "aircraft [0-9]+ handed to the AI" _handed "${_served}")
    list(LENGTH _handed _times)
    if(NOT _times EQUAL 1)
        message(FATAL_ERROR "the server handed the aircraft to the AI ${_times} times, not once: "
                            "the copilot took it back from its pilot:\n${_served}\n${_said}")
    endif()
    string(FIND "${_said}" "its pilot has taken it back: the copilot stands by" _back)
    if(_back LESS 0)
        message(FATAL_ERROR "the copilot did not stand by when taken back:\n${_said}")
    endif()
    # **The rule itself**: the answer that came after the take-back - a
    # route - was not sent. A `keep` there tests nothing, being sent by
    # nobody: asked now, a model that said it leaves the rule untested, which
    # is a skip, not a pass; played back, the recording is wrong.
    string(SUBSTRING "${_said}" ${_back} -1 _after_back)
    if(_after_back MATCHES "its copilot answered keep, and was not heard")
        if(DEFINED RECORD)
            message(STATUS "the model answered keep; the rule was not tested")
            cmake_language(EXIT 77)
        endif()
        message(FATAL_ERROR "the answer after the take-back was keep, so the rule was not "
                            "tested - the recording must hold a route there:\n${_said}")
    endif()
    if(NOT _after_back MATCHES "its copilot answered with a route of [0-9]+, and was not heard")
        message(FATAL_ERROR "no route came after the take-back, so the rule was not "
                            "tested:\n${_said}")
    endif()
    if(_after_back MATCHES "sent its copilot's route")
        message(FATAL_ERROR "a route was sent after its pilot took the aircraft back:\n${_said}")
    endif()
    message(STATUS "taken back, the copilot stood by, and the AI was given the aircraft once")
endif()

if(DEFINED ENGINE_AT)
    if(NOT _said MATCHES "aircraft [0-9]+'s engine has stopped")
        message(FATAL_ERROR "the client never heard its engine stop:\n${_said}")
    endif()
    if(NOT _said MATCHES "asked its copilot, the engine has stopped")
        message(FATAL_ERROR "the copilot was not asked when the engine stopped:\n${_said}")
    endif()
    if(NOT _served MATCHES "flies its copilot's route of [0-9]+:[^\n]*, gliding at ([0-9]+) kt, ([0-9]+) s in")
        message(FATAL_ERROR "the server never flew a glide:\n${_served}")
    endif()
    set(_glide "${CMAKE_MATCH_1}")
    math(EXPR _settled "${CMAKE_MATCH_2} + 45")
    # The half-minute lines after the glide was taken.
    string(FIND "${_served}" ", gliding at" _from)
    string(SUBSTRING "${_served}" ${_from} -1 _after)
    string(REGEX MATCHALL "to [A-Za-z0-9_]+, [0-9]+ of [0-9]+, [0-9]+ m from it at [0-9]+ kt, [0-9]+ s in"
           _lines "${_after}")
    list(LENGTH _lines _n)
    if(_n LESS 2)
        message(FATAL_ERROR "the server said where the glide had got to ${_n} times, not twice:\n"
                            "${_served}")
    endif()
    list(GET _lines 0 _first)
    list(GET _lines -1 _last)
    string(REGEX MATCH "to [A-Za-z0-9_]+, ([0-9]+) of [0-9]+, ([0-9]+) m from it at ([0-9]+) kt" _m "${_first}")
    set(_first_leg "${CMAKE_MATCH_1}")
    set(_first_m "${CMAKE_MATCH_2}")
    string(REGEX MATCH "to [A-Za-z0-9_]+, ([0-9]+) of [0-9]+, ([0-9]+) m from it at ([0-9]+) kt" _m "${_last}")
    set(_last_leg "${CMAKE_MATCH_1}")
    set(_last_m "${CMAKE_MATCH_2}")
    set(_kts "${CMAKE_MATCH_3}")
    if(_last_leg EQUAL _first_leg AND NOT _last_m LESS _first_m)
        message(FATAL_ERROR "the glide came no nearer its waypoint: ${_first}, then ${_last}")
    endif()
    # **Every line from 45 s after the glide was taken at the glide's
    # airspeed**, not the last alone - the model gave every waypoint that
    # airspeed too, and an aircraft flying them without the glide slows to it
    # in time - and at least two of them. The 45 s are the glide's to slow
    # from cruise to it (a_glide_with_the_engine_stopped_holds_its_airspeed_...).
    math(EXPR _low "${_glide} - 5")
    math(EXPR _high "${_glide} + 5")
    set(_held 0)
    foreach(_line IN LISTS _lines)
        string(REGEX MATCH "at ([0-9]+) kt, ([0-9]+) s in" _m "${_line}")
        if(CMAKE_MATCH_2 LESS _settled)
            continue()
        endif()
        if(CMAKE_MATCH_1 LESS _low OR CMAKE_MATCH_1 GREATER _high)
            message(FATAL_ERROR "gliding at ${CMAKE_MATCH_1} kt, not within 5 kt of the "
                                "${_glide} kt asked for: ${_line}\n${_served}")
        endif()
        math(EXPR _held "${_held} + 1")
    endforeach()
    if(_held LESS 2)
        message(FATAL_ERROR "only ${_held} lines from 45 s into the glide, not 2: the glide was "
                            "not watched long enough to say it was held:\n${_served}")
    endif()
    message(STATUS "the engine stopped, and the player's copilot glided it at ${_glide} kt: "
                   "${_first}; then ${_last}")
endif()

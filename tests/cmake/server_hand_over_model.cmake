# server_hand_over_model.cmake - a player hands its aircraft to the AI in
# the air, choosing the model that plans it: Claude, ChatGPT, or none.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DCHOSEN=anthropic|openai|none|refused|taken_back [-DMODEL=<model>]
#         [-DPLAYBACK=<recording> | -DRECORD=<recording>]
#         -P server_hand_over_model.cmake
#
# **Built, not hoped for.** A server with one AI aircraft, and the headless
# client flying its own, predicting it, which hands it to the AI five seconds
# after joining (`--hand-over-at 5`), in the air, with the model it chose
# (`--hand-over-model`). The server must hand it to the AI once - the
# hand-over itself, announced as any is - and then:
#
# **With a model** (CHOSEN anthropic or openai): the client's copilot is
# asked, "the pilot has handed you the aircraft", with the data's hand-over
# words (tasks/hand-over.words) and where the aircraft is - from the
# recording (PLAYBACK) with no key, or of the model now with the player's
# key (RECORD). It answers with a route, which goes to the server as a
# `COPILOT_ROUTE`; the server must fly it - the copilot's route taken, and of
# the lines it prints each half minute the last nearer the waypoint it is
# flying to than the first, or past it. The client says who planned it, and
# that must be the provider and model chosen.
#
# **With none** (CHOSEN none), or **a model with no key** (CHOSEN refused:
# both models, one after the other, with no key anywhere the client could
# look - both key variables unset, and HOME, XDG_CONFIG_HOME and APPDATA an
# empty directory): no copilot is asked and no route sent; the AI holds the
# aircraft's course - its heading within 5 degrees and its height within
# 300 ft, over the 60 s of the session's clock the client stays after the
# update that first shows the AI with it. Refused, the client must say so,
# naming the missing key, before it hands over - refused, never faked.
#
# **Taken back at once** (CHOSEN taken_back): Claude chosen, played back, and
# the aircraft taken back in the same breath as it is handed over
# (`--take-back-at 5` with `--hand-over-at 5`), before any update can show
# the AI with it. Its copilot must stand by, saying so, and send no route:
# the server hands it to the AI once and flies no copilot's route - a late
# answer must not undo the take-back. The client leaves 20 s of the session's
# clock after an update first shows its pilot with it again.
#
# Everything waits on events: the client leaves on the session's clock, 60 s
# after the route is sent or the hold is seen, and the server runs until it
# has gone (`--until-empty`); 300 s and 400 s are only the most either waits.
#
# Asked of a model now costs money, so only with GLIDESLOPE_LIVE_MODEL=1;
# else skipped (exit 77), before anything else. Without the DEM's tiles,
# skipped.

cmake_minimum_required(VERSION 3.28)

if(DEFINED RECORD AND NOT "$ENV{GLIDESLOPE_LIVE_MODEL}" STREQUAL "1")
    message(STATUS "${CHOSEN} is not asked: a live model call costs money, and is made only "
                   "with GLIDESLOPE_LIVE_MODEL=1 set")
    cmake_language(EXIT 77)
endif()

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/hand_over.sqlite")
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

# One hand-over, with the model WHAT[:MODEL]; what each program said, in
# _said (the client's heard lines) and _served (the server's).
function(hand_over what)
    set(_heard "${WORK}/heard-${what}.txt")
    file(REMOVE "${_heard}")
    set(_chosen "${what}")
    if(DEFINED MODEL AND NOT what STREQUAL "none")
        set(_chosen "${what}:${MODEL}")
    endif()
    set(_asking --hand-over-at 5 --hand-over-model "${_chosen}" --copilot-stay 60 ${_also})
    if(DEFINED PLAYBACK)
        list(APPEND _asking --copilot-playback "${PLAYBACK}")
    elseif(DEFINED RECORD)
        list(APPEND _asking --copilot-record "${RECORD}")
    endif()
    # The client first and the server last, so that what the server prints
    # is what comes out of the pipeline.
    execute_process(
        COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 300 --after 1
                --predict ${_asking} --heard "${_heard}"
        COMMAND "${SERVER}" --port ${PORT} --seconds 400 --until-empty --ai 1 --headless
                --data "${DATA}" --timeout 5 --store "${_store}"
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _served ERROR_VARIABLE _err)
    if(NOT EXISTS "${_heard}")
        message(FATAL_ERROR "the client heard nothing (exit codes ${_rcs}):\n${_served}\n${_err}")
    endif()
    file(READ "${_heard}" _said)
    if(NOT _rcs STREQUAL "0;0")
        if(DEFINED RECORD AND _err MATCHES "no (OpenAI|Anthropic) key|no credits|credit balance|insufficient_quota")
            message(STATUS "${what} cannot be asked here: ${_err}")
            cmake_language(EXIT 77)
        endif()
        message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_said}\n${_served}\n${_err}")
    endif()
    # **Handed over, once**: the hand-over itself, and never again.
    string(REGEX MATCHALL "aircraft [0-9]+ handed to the AI" _handed "${_served}")
    list(LENGTH _handed _times)
    if(NOT _times EQUAL 1)
        message(FATAL_ERROR "the server handed the aircraft to the AI ${_times} times, not "
                            "once:\n${_served}\n${_said}")
    endif()
    if(NOT _said MATCHES "aircraft [0-9]+ handed to the AI")
        message(FATAL_ERROR "the client never heard its aircraft handed to the AI:\n${_said}")
    endif()
    set(_said "${_said}" PARENT_SCOPE)
    set(_served "${_served}" PARENT_SCOPE)
endfunction()

# The AI held its course: no copilot asked, no route sent or flown, and the
# heading and height kept.
function(held what)
    foreach(_not IN ITEMS "asked its copilot" "sent its copilot's route")
        if(_said MATCHES "${_not}")
            message(FATAL_ERROR "${what}: the client said \"${_not}\", with no model to plan "
                                "the hand-over:\n${_said}")
        endif()
    endforeach()
    if(_served MATCHES "flies its copilot's route")
        message(FATAL_ERROR "${what}: the server flew a copilot's route, with no model to plan "
                            "the hand-over:\n${_served}")
    endif()
    if(NOT _said MATCHES "held by the AI for ([0-9]+) s: heading (-?[0-9.]+) to (-?[0-9.]+), height (-?[0-9]+) to (-?[0-9]+) ft")
        message(FATAL_ERROR "${what}: the client never said how the AI held it:\n${_said}")
    endif()
    set(_line "${CMAKE_MATCH_0}")
    set(_s "${CMAKE_MATCH_1}")
    # Whole tenths, for math().
    string(REPLACE "." "" _from "${CMAKE_MATCH_2}")
    string(REPLACE "." "" _to "${CMAKE_MATCH_3}")
    math(EXPR _turned "${_to} - ${_from}")
    if(_turned LESS 0)
        math(EXPR _turned "0 - ${_turned}")
    endif()
    if(_turned GREATER 1800)
        math(EXPR _turned "3600 - ${_turned}")
    endif()
    math(EXPR _climbed "${CMAKE_MATCH_5} - ${CMAKE_MATCH_4}")
    if(_climbed LESS 0)
        math(EXPR _climbed "0 - ${_climbed}")
    endif()
    if(_s LESS 60)
        message(FATAL_ERROR "${what}: held for ${_s} s, not the 60 asked for: ${_line}")
    endif()
    if(_turned GREATER 50 OR _climbed GREATER 300)
        message(FATAL_ERROR "${what}: the AI did not hold its course - ${_line} - not within "
                            "5 degrees and 300 ft")
    endif()
    message(STATUS "${what}: ${_line}")
endfunction()

if(CHOSEN STREQUAL "taken_back")
    set(_also --take-back-at 5 --copilot-stay 20)
    hand_over(anthropic)
    # The rule first: no route after the take-back.
    if(_said MATCHES "sent its copilot's route" OR _served MATCHES "flies its copilot's route")
        message(FATAL_ERROR "taken back, the aircraft was planned all the same:\n${_said}\n"
                            "${_served}")
    endif()
    foreach(_what IN ITEMS "handed over, planned by anthropic"
                           "its pilot has taken it back: the copilot stands by"
                           "flown by its pilot again for")
        if(NOT _said MATCHES "${_what}")
            message(FATAL_ERROR "the client never said \"${_what}\":\n${_said}")
        endif()
    endforeach()
    message(STATUS "handed over and taken back at once: its copilot stood by, and no route "
                   "was sent")
    return()
endif()

if(CHOSEN STREQUAL "none")
    hand_over(none)
    if(NOT _said MATCHES "handed over, planned by no model: the AI holds its course")
        message(FATAL_ERROR "the client never said no model planned it:\n${_said}")
    endif()
    held("none")
    return()
endif()

if(CHOSEN STREQUAL "refused")
    # No key anywhere the client could look.
    unset(ENV{GLIDESLOPE_OPENAI_KEY})
    unset(ENV{GLIDESLOPE_ANTHROPIC_KEY})
    file(REMOVE_RECURSE "${WORK}/no-config")
    file(MAKE_DIRECTORY "${WORK}/no-config")
    set(ENV{XDG_CONFIG_HOME} "${WORK}/no-config")
    set(ENV{HOME} "${WORK}/no-config")
    set(ENV{APPDATA} "${WORK}/no-config")
    # **Every model there is to choose**, each refused: the walk says how
    # many it covered, and fails unless it is all of them.
    set(_models anthropic openai)
    set(_walked 0)
    foreach(_model IN LISTS _models)
        hand_over(${_model})
        if(_model STREQUAL "anthropic")
            set(_missing "no Anthropic key")
        else()
            set(_missing "no OpenAI key")
        endif()
        if(NOT _said MATCHES "the hand-over's model, ${_model}, is refused: ${_missing}")
            message(FATAL_ERROR "${_model} with no key was not refused, saying so:\n${_said}")
        endif()
        if(NOT _said MATCHES "handed over, planned by no model \\(${_model} was refused\\): the AI holds its course")
            message(FATAL_ERROR "${_model} refused, the client never said no model planned it:\n"
                                "${_said}")
        endif()
        held("${_model} refused")
        math(EXPR _walked "${_walked} + 1")
    endforeach()
    list(LENGTH _models _all)
    if(NOT _walked EQUAL _all)
        message(FATAL_ERROR "walked ${_walked} of the ${_all} models")
    endif()
    message(STATUS "each of the ${_all} models, with no key, was refused and the aircraft held")
    return()
endif()

# ---- a model plans it ----
hand_over(${CHOSEN})
if(NOT _said MATCHES "handed over, planned by ([a-z]+), ([^\n]+)")
    message(FATAL_ERROR "the client never said a model planned the hand-over:\n${_said}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL CHOSEN OR (DEFINED MODEL AND NOT CMAKE_MATCH_2 STREQUAL MODEL))
    message(FATAL_ERROR "planned by ${CMAKE_MATCH_1}, ${CMAKE_MATCH_2}, not the ${CHOSEN} "
                        "${MODEL} chosen")
endif()
foreach(_what IN ITEMS "asked its copilot, the pilot has handed you the aircraft"
                       "its copilot answered with a route" "sent its copilot's route of")
    if(NOT _said MATCHES "${_what}")
        message(FATAL_ERROR "the client never said \"${_what}\":\n${_said}")
    endif()
endforeach()
if(_said MATCHES "held by the AI")
    message(FATAL_ERROR "the client took the aircraft as held, with a model planning it:\n${_said}")
endif()
if(NOT _served MATCHES "aircraft ([0-9]+) flies its copilot's route of [0-9]+:([^\n]*)")
    message(FATAL_ERROR "the server never flew the route the hand-over's model planned:\n"
                        "${_served}")
endif()
set(_names "${CMAKE_MATCH_2}")
if(_served MATCHES "copilot's route refused")
    message(FATAL_ERROR "the server refused the hand-over's route:\n${_served}")
endif()
# Its progress, each half minute: the first line and the last.
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
message(STATUS "handed over in the air, ${CHOSEN} planned it and the server flew it,${_names}: "
               "${_first}; then ${_last}")

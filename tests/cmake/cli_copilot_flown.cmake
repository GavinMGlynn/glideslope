# cli_copilot_flown.cmake - a Cessna flown by the AI over the DEM with a
# language model as its copilot, asked as the flight goes
# (glideslope_cli fly-copilot).
#
#   cmake -DCLI=<glideslope_cli> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DSCENARIO=coast|engine -DPROVIDER=openai|anthropic
#         [-DMODEL=<model>] (-DPLAYBACK=<recording> | -DRECORD=<recording>)
#         -P cli_copilot_flown.cmake
#
# **coast**: off Bondi's south end at 1,500 ft, told to follow the coast south to
# Cronulla and orbit over its beach. It must come within 3 km of the beach,
# with the coast - land and sea both - within 1 km of it in at least 85% of
# the looks on the way, one each ten seconds from its first route, and never
# more than 1,750 m from it. Flying straight there does neither: it crosses
# Maroubra inland and the middle of Botany Bay - Claude Haiku 4.5, which did,
# was 2,000 m from the coast at most and within 1 km in 77% of its looks.
#
# **engine**: off Bondi at 4,500 ft, told to follow the coast south to
# Cronulla; the engine stops 90 s in. The copilot must glide - an answer
# with `glide` in it - and the aircraft come within 1.5 km of a runway's
# threshold that was within 40 km when it stopped, more than 300 ft above the
# ground there, holding its glide's airspeed within a 10 kt band from 45 s
# after the glide began, and come down to 300 ft above the ground within 3 km of
# one - over the airport, not short of it.
#
# In each, nothing wrecked, and the copilot asked more than once.
#
# **Asked of PROVIDER now costs money**, so it is asked only when
# GLIDESLOPE_LIVE_MODEL=1 is set in the environment; otherwise it reports
# itself skipped (exit 77), before anything else - as cli_plan_flown.cmake.
# Asked now with RECORD, what was asked and answered is kept there, with no
# header and so no key, to be played back as PLAYBACK. Played back, the
# requests are matched but their numbers (copilot::Match::but_numbers), so a
# flight flown again a little differently on another machine still plays.

cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED PLAYBACK AND NOT "$ENV{GLIDESLOPE_LIVE_MODEL}" STREQUAL "1")
    message(STATUS "${PROVIDER} is not asked: a live model call costs money, and is made only "
                   "with GLIDESLOPE_LIVE_MODEL=1 set")
    cmake_language(EXIT 77)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

if(SCENARIO STREQUAL "coast")
    set(_flight -33.895 151.278 1500 190 100
        "follow the coast south to Cronulla at 1,500 ft, then orbit over Cronulla beach"
        --to -34.055 151.155 --minutes 12)
elseif(SCENARIO STREQUAL "engine")
    set(_flight -33.891 151.285 4500 180 100
        "follow the coast south to Cronulla at 4,500 ft"
        --engine-fails-at 90 --minutes 12)
else()
    message(FATAL_ERROR "SCENARIO is coast or engine, not \"${SCENARIO}\"")
endif()

set(_how)
if(DEFINED PLAYBACK)
    set(_how --playback "${PLAYBACK}")
elseif(DEFINED RECORD)
    set(_how --record "${RECORD}")
endif()
if(DEFINED MODEL)
    list(APPEND _how --model "${MODEL}")
endif()
execute_process(
    COMMAND "${CLI}" --data "${DATA}" fly-copilot c172p ${_flight} --provider ${PROVIDER} ${_how}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    if(NOT DEFINED PLAYBACK AND _err MATCHES "no (OpenAI|Anthropic) key|no credits|credit balance|insufficient_quota")
        message(STATUS "${PROVIDER} cannot be asked here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the flight was not flown (exit ${_rc}):\n${_out}\n${_err}")
endif()
if(_out MATCHES "wrecked")
    message(FATAL_ERROR "the aircraft was wrecked:\n${_out}")
endif()
if(NOT _out MATCHES "was asked ([0-9]+) times: ([0-9]+) new routes")
    message(FATAL_ERROR "the copilot was not asked:\n${_out}")
endif()
if(CMAKE_MATCH_1 LESS 2 OR CMAKE_MATCH_2 LESS 1)
    message(FATAL_ERROR "the copilot was asked ${CMAKE_MATCH_1} times and gave "
                        "${CMAKE_MATCH_2} routes: not flying with it as the flight went:\n${_out}")
endif()

if(SCENARIO STREQUAL "coast")
    if(NOT _out MATCHES "came within 3 km of the destination, ([0-9]+) s in; the coast at most ([0-9]+) m from it on the way, and within 1 km in ([0-9]+) of ([0-9]+) looks")
        message(FATAL_ERROR "it never came to Cronulla:\n${_out}")
    endif()
    set(_farthest "${CMAKE_MATCH_2}")
    set(_seen "${CMAKE_MATCH_3}")
    set(_looks "${CMAKE_MATCH_4}")
    if(_farthest GREATER 1750)
        message(FATAL_ERROR "the coast was ${_farthest} m from it at most, more than 1750:\n${_out}")
    endif()
    math(EXPR _percent "${_seen} * 100 / ${_looks}")
    if(_percent LESS 85)
        message(FATAL_ERROR "the coast was within 1 km in only ${_seen} of ${_looks} looks "
                            "(${_percent}%, at least 85):\n${_out}")
    endif()
    message(STATUS "came to Cronulla with the coast within 1 km in ${_seen} of ${_looks} looks, "
                   "and never more than ${_farthest} m from it")
else()
    if(NOT _out MATCHES "gliding at [0-9]+ kt")
        message(FATAL_ERROR "the copilot never glided:\n${_out}")
    endif()
    if(NOT _out MATCHES "nearest ([A-Z0-9]+ [0-9A-Z]+)'s threshold, ([0-9]+) m from it at ([-0-9]+) ft above the ground")
        message(FATAL_ERROR "the engine stopped near no runway:\n${_out}")
    endif()
    set(_field "${CMAKE_MATCH_1}")
    set(_off "${CMAKE_MATCH_2}")
    set(_above "${CMAKE_MATCH_3}")
    if(_off GREATER 1500 OR _above LESS 300)
        message(FATAL_ERROR "the nearest it came to a runway was ${_field}'s threshold, ${_off} m "
                            "off at ${_above} ft above the ground (within 1500 m, above 300 "
                            "ft):\n${_out}")
    endif()
    if(NOT _out MATCHES "down to 300 ft above the ground, [0-9]+ s after the engine stopped, ([0-9]+) m from")
        message(FATAL_ERROR "the glide never came down to 300 ft above the ground:\n${_out}")
    endif()
    set(_down "${CMAKE_MATCH_1}")
    if(_down GREATER 3000)
        message(FATAL_ERROR "down to 300 ft above the ground ${_down} m from the nearest "
                            "threshold, more than 3 km:\n${_out}")
    endif()
    if(NOT _out MATCHES "gliding at ([0-9]+) to ([0-9]+) kt from 45 s after the glide began")
        message(FATAL_ERROR "no glide was held:\n${_out}")
    endif()
    math(EXPR _band "${CMAKE_MATCH_2} - ${CMAKE_MATCH_1}")
    if(_band GREATER 10)
        message(FATAL_ERROR "the glide from ${CMAKE_MATCH_1} to ${CMAKE_MATCH_2} kt, not "
                            "within 10 kt:\n${_out}")
    endif()
    message(STATUS "glided to ${_field}, ${_off} m from its threshold at ${_above} ft above "
                   "the ground, at ${CMAKE_MATCH_1} to ${CMAKE_MATCH_2} kt, and down to 300 ft "
                   "above the ground ${_down} m from a threshold")
endif()
message(STATUS "${_out}")

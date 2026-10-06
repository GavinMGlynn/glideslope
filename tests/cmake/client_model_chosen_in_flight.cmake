# client_model_chosen_in_flight.cmake - the client with the window chooses
# the model that plans a hand-over in flight, with M, and with no key for it
# is refused, says so, and its aircraft is held.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -DPRESSES=1|2|3 -P client_model_chosen_in_flight.cmake
#
# **The situation is built.** The client starts with no hand-over model -
# none, the default - and no key for any: the keys' variables unset and its
# configuration directories empty, as on a machine that has never had one.
# A second in, M is pressed PRESSES times (`--next-model-after`, what M
# does), a second apart, and five seconds in it hands its aircraft to
# the AI (`--hand-over-after`). The shot waits for the server to say the AI
# has it.
#
# **What must hold**: each press says the model it chose, in turn - Claude
# (anthropic), ChatGPT (openai), and none again - so the cycle is walked
# whole by the three cases. With a model chosen, the hand-over is refused
# for want of its key, said for what it is, and planned by no model: the AI
# holds its course. With none chosen again, no model is asked at all. Either
# way the server says the AI has the aircraft.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/model.sqlite")
set(_shot "${WORK}/model.bmp")
set(_ready "${WORK}/flying")
file(REMOVE "${_store}" "${_shot}" "${_ready}")

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

# **No key, anywhere it is looked for**.
unset(ENV{GLIDESLOPE_OPENAI_KEY})
unset(ENV{GLIDESLOPE_ANTHROPIC_KEY})
unset(ENV{OPENAI_API_KEY})
unset(ENV{ANTHROPIC_API_KEY})
file(MAKE_DIRECTORY "${WORK}/no-config")
set(ENV{XDG_CONFIG_HOME} "${WORK}/no-config")
set(ENV{HOME} "${WORK}/no-config")
set(ENV{APPDATA} "${WORK}/no-config")

set(_choices anthropic openai none)
set(_presses)
set(_at 1)
foreach(_i RANGE 1 ${PRESSES})
    list(APPEND _presses --next-model-after ${_at})
    math(EXPR _at "${_at} + 1")
endforeach()
math(EXPR _last "${PRESSES} - 1")
list(GET _choices ${_last} _chosen)

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 600 --view cockpit ${_presses} --hand-over-after 5
            --after-ready "${_ready}" --server 127.0.0.1 ${PORT} --server-key ${_key}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT EXISTS "${_shot}")
    if(_err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()

# Each press, in turn.
string(REGEX MATCHALL "glideslope: a hand-over is planned by [a-z]+" _said "${_out}")
set(_wanted)
foreach(_i RANGE 0 ${_last})
    list(GET _choices ${_i} _c)
    if(_c STREQUAL "none")
        set(_c "no")
    endif()
    list(APPEND _wanted "glideslope: a hand-over is planned by ${_c}")
endforeach()
if(NOT _said STREQUAL _wanted)
    message(FATAL_ERROR "M chose '${_said}', not '${_wanted}':\n${_out}")
endif()

if(_chosen STREQUAL "none")
    # Each model chosen before it was refused as it was chosen - said at
    # once, not at the hand-over - and nothing after none.
    string(FIND "${_out}" "a hand-over is planned by no model" _none_at)
    string(SUBSTRING "${_out}" ${_none_at} -1 _after)
    if(_out MATCHES "handed over, planned by" OR _after MATCHES "no copilot:")
        message(FATAL_ERROR "with no model chosen again, a model was still asked:\n${_out}")
    endif()
else()
    if(NOT _out MATCHES "glideslope: handed over, planned by no model \\(${_chosen} was refused: [^\n]*\\): the AI holds its course\n")
        message(FATAL_ERROR "${_chosen} with no key was not refused and the aircraft held, "
                            "saying so:\n${_out}")
    endif()
endif()
if(NOT _out MATCHES "the server says the AI has aircraft [0-9]+\n")
    message(FATAL_ERROR "the server never said the AI had the client's aircraft:\n${_out}")
endif()
message(STATUS "M pressed ${PRESSES} times chose ${_chosen}; handed over, the AI holds the "
               "aircraft")

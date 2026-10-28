# client_stovl_at_once.cmake - the client with the window, its first asking
# for her levers ignored, selects STOVL mode with its first input, and her
# prediction's law flies her on the pitch trim the server's does.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DIMPAIR=<glideslope_impair> -DCACHE=<downloads dir> -DWORK=<scratch>
#         -DPORT=<a port, and the next for a relay> -DDRIVER=<gpu driver> -P client_stovl_at_once.cmake
#
# **Built, every run.** The server and plan of server_stovl_at_once.cmake;
# the client with the window joins asking for the F-35B, and spends the
# server's whole second of requests on nothing just before it asks for the
# levers the server holds her on (`--test-spend-requests-first`), so that
# that asking is acknowledged and ignored (net/budget.hpp). STOVL mode is
# selected with its very first input (`--test-stovl-at-once`). The server
# makes its law on that input from what it held her on - her trimmed
# levers, their pitch trim the trim her trim found - and the prediction's is
# made from the levers it was told (sim::Prediction::held_by_server). Asked
# once and given up on, as this client was until 2026-10-29, it was told
# nothing, and its law flew her on a pitch trim of nought.
#
# **What must hold**: the server said it ignored a request of this client's,
# and its levers were said all the same; the server says the pitch trim its
# law flew her on, not nought, and the client's law flies her on the same,
# to the printed 17 digits, as it quits at 1,200 steps (ten seconds).
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")
if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/stovl.sqlite")
set(_plan "${WORK}/stovl.plan")
file(REMOVE "${_store}")
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
set(ENV{LSAN_OPTIONS} "exitcode=0")
set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
# The server's standard output, where it says the law it made, reaches
# standard error through a relay after it (glideslope_impair, delivering at
# once); the client last, its own on standard output.
math(EXPR _relay "${PORT} + 1")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 30 --store "${_store}" --plan "${_plan}"
            --ready-file "${_ready}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
            --seed 1 --until-input-ends --seconds 600
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --aircraft f35b --test-stovl-at-once --test-spend-requests-first
            --shot "${WORK}/stovl.bmp" --shot-at 2400 --quit-at 1200
            --after-ready "${_ready}"
            --server 127.0.0.1 ${_relay} --server-key ${_key}
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
list(GET _rcs -1 _rc)
list(GET _rcs 0 _server_rc)
if(NOT _out MATCHES "quitting at tick")
    if(_err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the client never flew to its last tick:\n${_out}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()
if(NOT _server_rc EQUAL 0)
    message(FATAL_ERROR "the server exited ${_server_rc}:\n${_err}")
endif()
if(NOT _out MATCHES "the server gave this client aircraft [0-9]+, the f35b")
    message(FATAL_ERROR "the client was not given the F-35B it asked for:\n${_out}")
endif()
# The situation, built: its first asking ignored.
if(NOT _err MATCHES "dropped past [0-9]+ a second, [1-9][0-9]* of [0-9]+ requests ignored")
    message(FATAL_ERROR "the server ignored none of the client's requests, so its first asking "
                        "for her levers was answered:\n${_err}")
endif()
if(NOT _err MATCHES "aircraft [0-9]+'s STOVL law made at step [0-9]+: throttle integral [-0-9.e+]+; pitch trim ([-0-9.e+]+)\n")
    message(FATAL_ERROR "the server never said it made her STOVL law:\n${_err}")
endif()
set(_server_trim "${CMAKE_MATCH_1}")
# Not nought: her trim found one, and two noughts would pass whatever the
# laws were made from.
if(_server_trim MATCHES "^-?0(\\.0*)?(e[-+]?[0-9]+)?$")
    message(FATAL_ERROR "the server's law flew her on a pitch trim of ${_server_trim}, nought: "
                        "her trim found none, and the trims compared say nothing:\n${_err}")
endif()
if(NOT _out MATCHES "her STOVL law flies her on a pitch trim of ([-0-9.e+]+)\n")
    message(FATAL_ERROR "the client did not say the pitch trim her law flies her on:\n${_out}")
endif()
set(_client_trim "${CMAKE_MATCH_1}")
if(NOT _client_trim STREQUAL _server_trim)
    message(FATAL_ERROR "the prediction's law flew her on a pitch trim of ${_client_trim}, the "
                        "server's on ${_server_trim}:\n${_out}")
endif()
if(NOT _out MATCHES "the server's levers kept")
    message(FATAL_ERROR "the client was never told her levers:\n${_out}")
endif()
message(STATUS "her first asking for her levers ignored, asked again and told; both laws "
               "flying her on a pitch trim of ${_server_trim}")

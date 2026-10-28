# client_stovl_join.cmake - the client with the window, given an F-35B whose
# STOVL law is already flying her, joins and begins on the law the server
# told it (`LAW_STATE`).
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DAIRCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DDRIVER=<gpu driver>
#         -P client_stovl_join.cmake
#
# **Built, not hoped for**, as server_stovl_join.cmake builds it: A, the
# command-line client, flies an F-35B at her vertical-landing loading to 20
# degrees of bank held jet-borne, and leaves once her law holds it and the
# server has the input (`--leave-once-bank-held`); B, **the window's
# program, with no display** (`--headless`, drawing offscreen, so DISPLAY and
# WAYLAND_DISPLAY are not read), joins on A's key (`--player-key`) through a
# relay that holds every datagram 100 ms each way, and is given her. The
# client's joining (Online::join) waits for the law (it is not given her
# until the server has said the whole of it), and says what it began on.
#
# **What must hold**: the server said her law to its client once, and B
# joined on exactly that - the same stage and bank held, to the digit - and
# drew its frames. Without a GPU driver it reports itself skipped (exit 77),
# never passed; so it does without the network.
cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")
if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
unset(ENV{DISPLAY})
unset(ENV{WAYLAND_DISPLAY})
set(_store "${WORK}/stovl.sqlite")
set(_heard_a "${WORK}/a.txt")
set(_gone "${WORK}/a-gone.txt")
set(_plan "${WORK}/stovl.plan")
set(_shot "${WORK}/b.bmp")
set(_said_b "${WORK}/b.txt")
file(REMOVE "${_store}" "${_heard_a}" "${_gone}" "${_shot}" "${_said_b}")
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
set(_player "e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26")
# The addresses are written in the commands, where test_ports.cmake reads them.
execute_process(
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 320x240 --screen flight
            --aircraft f35b --loading vertical_landing --after-ready "${_gone}"
            --server 127.0.0.1 ${_relay} --server-key ${_key} --player-key ${_player}
            --shot "${_shot}" --shot-at 120 --stdout "${_said_b}"
    COMMAND "${AIRCLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 600 --after 1 --key "${_player}"
            --aircraft f35b --loading vertical_landing --predict --stovl-land --stovl-level
            --stovl-bank-at 2100 --leave-once-bank-held --no-goodbye --done "${_gone}"
            --heard "${_heard_a}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 900 --until-empty --ai 1 --headless
            --players 2 --data "${DATA}" --timeout 30 --store "${_store}" --plan "${_plan}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 900
    RESULT_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT EXISTS "${_shot}")
    if(_err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the client drew nothing (exit codes ${_rcs}):\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")
list(REMOVE_ITEM _rcs 0)
if(_rcs)
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
string(REGEX MATCHALL "aircraft [0-9]+'s STOVL law told to its client" _told "${_err}")
list(LENGTH _told _times)
if(NOT _times EQUAL 1)
    message(FATAL_ERROR "the server told her law ${_times} times, not once:\n${_err}")
endif()
if(NOT _err MATCHES "STOVL law told to its client at [0-9.]+ s: stage ([0-9]+), bank held ([-0-9.]+) degrees")
    message(FATAL_ERROR "the server did not say what it told:\n${_err}")
endif()
set(_stage "${CMAKE_MATCH_1}")
set(_bank "${CMAKE_MATCH_2}")
file(READ "${_said_b}" _said)
if(NOT _said MATCHES "glideslope: joined aircraft [0-9]+ with her STOVL law told: stage ([0-9]+), bank held ([-0-9.]+) degrees")
    message(FATAL_ERROR "the client with the window never joined on her law:\n${_said}\n${_err}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL _stage OR NOT CMAKE_MATCH_2 STREQUAL _bank)
    message(FATAL_ERROR "the client joined on stage ${CMAKE_MATCH_1}, ${CMAKE_MATCH_2} degrees; the "
                        "server told stage ${_stage}, ${_bank}:\n${_err}")
endif()
if(NOT _stage EQUAL 2 OR NOT _bank STREQUAL "20.000")
    message(FATAL_ERROR "she was not given jet-borne with 20 degrees held (stage ${_stage}, "
                        "${_bank}):\n${_err}")
endif()
message(STATUS "the client with the window joined on her law: stage ${_stage}, ${_bank} degrees of bank held")

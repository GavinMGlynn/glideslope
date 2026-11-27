# client_sees_others_surfaces.cmake - the client with the window draws
# another player's aircraft with its ailerons over as it rolls.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope>
#         -DCLI=<glideslope_cli> -DCHECK=<glideslope_ridden_surfaces_check>
#         -DDATA=<data> -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -P client_sees_others_surfaces.cmake
#
# **Built, not hoped for.** A server for two players and no AI aircraft but
# the one it needs to fly at all; a second player, glideslope_cli connect
# --fly, flying its Cessna with the stick held full left - a thing no AI pilot
# does - so that she rolls, her ailerons over, for as long as the test; and
# the client with the window riding along in that player's aircraft
# (`--ride-along-person`), seen from behind. At its shot, which waits for
# the ride along to have begun, the client says where it drew her surfaces,
# and draws the frame three times more with no HUD (`--shot-ridden`): her as
# the updates have her, with her surfaces still, and without her.
#
# **What must hold**: that it rode along in the other player's Cessna; that
# her ailerons were drawn over the way the stick holds them - full left, the
# left aileron's trailing edge up and the right's down, each by five degrees
# at least, where her flight model has them at about fifteen; and that the
# frame as the updates have her differs from the one with her surfaces still
# (glideslope_ridden_surfaces_check). With the surfaces a state update
# carries sent as nought, all of it fails: her ailerons drawn at nought, and
# the two frames the same.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/skip_or_fail.cmake")
# **Its port held machine-wide** while it runs (port_lock.cmake).
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
file(MAKE_DIRECTORY "${WORK}")
set(_store "${WORK}/surfaces.sqlite")
set(_shot "${WORK}/surfaces.bmp")
set(_ridden "${WORK}/surfaces-ridden")
file(REMOVE "${_store}" "${_shot}" "${_ridden}-moving.bmp" "${_ridden}-still.bmp"
     "${_ridden}-without.bmp")

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
glideslope_warmed_up("${_rc}" "${_err}")

set(ENV{LSAN_OPTIONS} "exitcode=0")
# **Each client connects once the server is flying** (its --ready-file): a
# handshake gives up five seconds after it begins. The player who rolls
# stays until the client with the window has gone (--until-exists, on the
# file it writes when it is done), so that she is rolling at its shot however
# long a slow machine takes to get there; the server runs until both have
# gone.
set(_ready "${WORK}/flying")
set(_done "${WORK}/done")
file(REMOVE "${_ready}" "${_done}")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --ai 1 --players 2
            --headless --data "${DATA}" --timeout 30 --store "${_store}"
            --ready-file "${_ready}"
    COMMAND "${CLI}" connect "127.0.0.1:${PORT}" "${_key}" 600 --fly
            --after-ready "${_ready}" --until-exists "${_done}"
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 600 --view behind --ride-along-person
            --shot-ridden "${_ridden}" --done "${_done}" --after-ready "${_ready}"
            --server 127.0.0.1 ${PORT} --server-key ${_key}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")

if(NOT EXISTS "${_shot}")
    if(_err MATCHES "no GPU|could not|device")
        glideslope_cannot_draw("${_err}")
    endif()
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "a program exited ${_rc}:\n${_out}\n${_err}")
endif()

if(NOT _out MATCHES "riding along in aircraft ([0-9]+), the c172p")
    message(FATAL_ERROR "the client did not ride along in the other player's Cessna:\n${_out}")
endif()
set(_number "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "aircraft ${_number} drawn with its surfaces at ([^\n]+)\n")
    message(FATAL_ERROR "the client did not say where it drew her surfaces:\n${_out}")
endif()
set(_surfaces "${CMAKE_MATCH_1}")
message(STATUS "aircraft ${_number} drawn with its surfaces at ${_surfaces}")
# **Lit by the shader**, as the client's own aeroplane is: every one of
# her parts, body and surfaces, given the sun (tests/cmake/client_light.cmake
# holds what that light does).
if(NOT _out MATCHES "aircraft ${_number} drawn in ([0-9]+) parts, ([0-9]+) of them lit by the shader")
    message(FATAL_ERROR "the client did not say how her parts were lit:\n${_out}")
endif()
if(CMAKE_MATCH_1 LESS 2 OR NOT CMAKE_MATCH_2 EQUAL CMAKE_MATCH_1)
    message(FATAL_ERROR "her parts are not all lit by the shader: ${CMAKE_MATCH_2} of "
                        "${CMAKE_MATCH_1}")
endif()
message(STATUS "her ${CMAKE_MATCH_1} parts all lit by the shader")
if(NOT _surfaces MATCHES "left-aileron ([-+][0-9.]+) deg")
    message(FATAL_ERROR "no left aileron drawn: ${_surfaces}")
endif()
set(_left "${CMAKE_MATCH_1}")
if(NOT _surfaces MATCHES "right-aileron ([-+][0-9.]+) deg")
    message(FATAL_ERROR "no right aileron drawn: ${_surfaces}")
endif()
set(_right "${CMAKE_MATCH_1}")
# The Cessna's ailerons move their trailing edges down for a positive
# deflection (c172p.surfaces): full left stick puts the left one's up -
# below minus five degrees - and the right one's down.
if(NOT _left LESS -5 OR NOT _right GREATER 5)
    message(FATAL_ERROR "her ailerons were not drawn over the way the stick holds them, "
                        "left ${_left} and right ${_right} degrees: ${_surfaces}")
endif()

execute_process(COMMAND "${CHECK}" "${_ridden}-moving.bmp" "${_ridden}-still.bmp"
                        "${_ridden}-without.bmp"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _check_out ERROR_VARIABLE _check_err)
message(STATUS "${_check_out}${_check_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the other player's Cessna drawn: ${_check_err}")
endif()
message(STATUS "the other player's Cessna was drawn rolling with her ailerons over, "
               "left ${_left} and right ${_right} degrees")

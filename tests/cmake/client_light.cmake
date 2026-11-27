# client_light.cmake - the aeroplane's light follows it as it banks, with no
# mesh of it made again.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_light_check>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -P client_light.cmake
#
# Flies the Cessna headless over the Nullarbor, heading east, and shoots her
# from above in the body frame - the view that sees her tops - four times:
# level at tick 2, and at tick BANKED_AT with the stick held full right from
# the start, each with her drawn and not (--draw-aircraft off). Banking right
# while heading east turns her tops to the south, away from the sun in the
# north-west, so they are drawn darker; the client says where the sun was in
# her body frame at each shot, and glideslope_light_check holds the ratio of
# her tops' brightness, banked to level, to the ratio that sun gives.
#
# **No mesh made again**: the banked run, which rolls her through some tens of
# degrees, must say its meshes were made once. Lit on the way in, as before
# 2026-11-28, they were made again at every five degrees.
#
# The flight stands on the DEM and draws it, so it needs the tiles and the
# geoid, fetched into CACHE: without the network the test is skipped (exit 77)
# unless GLIDESLOPE_REQUIRE_NETWORK is set. No imagery, so that each pair of
# shots differs where she is drawn and nowhere else.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

set(_banked_at 90)

# Shoots one frame; sets <name>_SUN_Z to the body-frame z of the sun the
# client says it drew her under, and <name>_MADE to how many times her meshes
# were made.
function(shoot name at)
    set(_shot "${WORK}/light-${DRIVER}-${name}.bmp")
    file(REMOVE "${_shot}")
    execute_process(
        COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                --screen flight --aircraft c172p --view above --imagery off
                --at "-31.0,129.0,400" --attitude "0,0,90"
                ${ARGN} --shot-at ${at} --shot "${_shot}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    glideslope_skip_when_not_downloaded("${_rc}" "${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope ${name} exited ${_rc}\n${_out}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
    set(_said "at the shot, banked ([-0-9.]+) degrees, the sun in the body frame ")
    string(APPEND _said "[-0-9.]+ [-0-9.]+ ([-0-9.]+); times the aeroplane's meshes were made: ")
    string(APPEND _said "([0-9]+)")
    if(_out MATCHES "${_said}")
        message(STATUS "${name}: banked ${CMAKE_MATCH_1} degrees, the sun's z ${CMAKE_MATCH_2}, "
                       "meshes made ${CMAKE_MATCH_3} times")
        set(${name}_SUN_Z ${CMAKE_MATCH_2} PARENT_SCOPE)
        set(${name}_MADE ${CMAKE_MATCH_3} PARENT_SCOPE)
    else()
        set(${name}_SUN_Z "" PARENT_SCOPE)
        set(${name}_MADE "" PARENT_SCOPE)
    endif()
endfunction()

shoot(level 2)
shoot(level_without 2 --draw-aircraft off)
shoot(banked ${_banked_at} --hold-stick 1,0,0)
shoot(banked_without ${_banked_at} --hold-stick 1,0,0 --draw-aircraft off)

if(level_SUN_Z STREQUAL "" OR banked_SUN_Z STREQUAL "")
    message(FATAL_ERROR "the client did not say where the sun was at the shot")
endif()
if(NOT banked_MADE EQUAL 1)
    message(FATAL_ERROR "the aeroplane's meshes were made ${banked_MADE} times as she banked, "
                        "where once is all her light needs")
endif()

execute_process(COMMAND "${CHECK}" "${WORK}/light-${DRIVER}-level.bmp"
                        "${WORK}/light-${DRIVER}-level_without.bmp"
                        "${WORK}/light-${DRIVER}-banked.bmp"
                        "${WORK}/light-${DRIVER}-banked_without.bmp"
                        ${level_SUN_Z} ${banked_SUN_Z}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the Cessna's light: ${_err}")
endif()
message(STATUS "the Cessna's light follows her bank, her meshes made once")

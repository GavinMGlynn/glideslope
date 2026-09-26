# client_memory.cmake - a headless client drawing ten thousand frames keeps its
# memory level.
#
#   cmake -DPROGRAM=<glideslope> -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -P client_memory.cmake
#
# Flies the flight screen headless for ten thousand frames - every frame two
# ticks, 166 s of flight, counted in frames and not in seconds - printing the
# memory the process holds (the resident set on Linux, private bytes on
# Windows: platform/memory.hpp) at frame 1 and every 500th frame after, and
# shoots frame 10,001.
#
# **Why the frame after.** The frame shot waits for every terrain tile its
# view needs, which by itself takes the client from 160 MiB to 970 MiB on
# lavapipe, the first frame or the ten-thousandth alike. That is the shot, not
# the frames, so the frames sampled are the ten thousand before it.
#
# **The bound.** Terrain, Cesium Native's caches and the driver's pools grow
# while the flight settles in, and by frame 1,000 they have (157 MiB, from
# 125 MiB at the first frame, in WSL on lavapipe). From frame 1,000 to frame
# 10,000 the memory held may grow by at most MAX_GROWTH_MIB, 64 MiB; it grew
# 3 MiB. Without the renderer's wait on the frame two before (gfx/renderer.hpp),
# lavapipe's queue of frames not yet drawn grew 0.8 MiB a frame: 966 MiB at
# frame 1,000, 1.6 GB at 1,750, and a segmentation fault before 2,000. Every
# sample must have been read: a platform that will not say fails the test
# rather than passing it.
#
# The flight stands on the DEM and draws it, so it needs the tiles and the
# geoid, fetched into CACHE: without the network the test is skipped (exit 77)
# unless GLIDESLOPE_REQUIRE_NETWORK is set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_frames 10000)
set(_every 500)
set(_settled 1000)
if(NOT DEFINED MAX_GROWTH_MIB)
    set(MAX_GROWTH_MIB 64)
endif()

file(MAKE_DIRECTORY "${WORK}")
set(_shot "${WORK}/memory-${DRIVER}.bmp")
file(REMOVE "${_shot}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

math(EXPR _shot_frame "${_frames} + 1")
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                        --screen flight --shot-frame ${_shot_frame} --memory-every ${_every}
                        --shot "${_shot}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 AND _err MATCHES "could not download")
    if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "the DEM could not be had: ${_err}")
        cmake_language(EXIT 77)
    endif()
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope exited ${_rc}\n${_out}${_err}")
endif()
glideslope_judge_leaks("${_err}")

if(NOT _out MATCHES "glideslope: wrote tick ([0-9]+), frame ([0-9]+),")
    message(FATAL_ERROR "no shot was written:\n${_out}")
endif()
if(NOT CMAKE_MATCH_2 EQUAL _shot_frame)
    message(FATAL_ERROR "the shot was of frame ${CMAKE_MATCH_2}, not ${_shot_frame}")
endif()

# Every sample, frame 1 and each 500th to 10,000: 21 of them, each read.
math(EXPR _expected "${_frames} / ${_every} + 1")
string(REGEX MATCHALL "glideslope: frame [0-9]+, memory held -?[0-9]+" _samples "${_out}")
list(LENGTH _samples _count)
if(NOT _count EQUAL _expected)
    message(FATAL_ERROR "${_count} memory samples, not ${_expected}:\n${_out}")
endif()
set(_settled_mib "")
set(_last_frame 0)
set(_worst_mib 0)
set(_table "")
foreach(_sample IN LISTS _samples)
    string(REGEX MATCH "frame ([0-9]+), memory held (-?[0-9]+)" _ "${_sample}")
    set(_frame "${CMAKE_MATCH_1}")
    set(_bytes "${CMAKE_MATCH_2}")
    if(_bytes LESS 0)
        message(FATAL_ERROR "the memory held could not be read at frame ${_frame}")
    endif()
    # In MiB. CMake's arithmetic is 64-bit, so the bytes fit.
    math(EXPR _mib "${_bytes} / 1048576")
    string(APPEND _table "  frame ${_frame}: ${_mib} MiB\n")
    if(_frame EQUAL _settled)
        set(_settled_mib ${_mib})
    endif()
    if(_frame GREATER_EQUAL _settled AND _mib GREATER _worst_mib)
        set(_worst_mib ${_mib})
    endif()
    set(_last_frame ${_frame})
endforeach()
message(STATUS "the memory held, every ${_every} frames:\n${_table}")
if(_settled_mib STREQUAL "" OR NOT _last_frame EQUAL _frames)
    message(FATAL_ERROR "the samples do not run from frame ${_settled} to ${_frames}")
endif()
math(EXPR _growth "${_worst_mib} - ${_settled_mib}")
if(_growth GREATER MAX_GROWTH_MIB)
    message(FATAL_ERROR "the memory held grew ${_growth} MiB between frame ${_settled} "
                        "and frame ${_frames}, more than ${MAX_GROWTH_MIB} MiB:\n${_table}")
endif()
message(STATUS "the memory held grew ${_growth} MiB between frame ${_settled} and "
               "frame ${_frames}; at most ${MAX_GROWTH_MIB} MiB")

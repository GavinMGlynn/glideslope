# client_memory.cmake - a headless client drawing ten thousand frames keeps its
# memory level.
#
#   cmake -DPROGRAM=<glideslope> -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -P client_memory.cmake
#
# Draws the terrain screen headless, 160x120 - Sydney from 1,500 m over Botany
# Bay, looking up the harbour, with its imagery and its credits over it - for
# ten thousand frames, counted in frames and not in seconds, printing the
# memory the process holds (the resident set on Linux, private bytes on
# Windows, the physical footprint on macOS: platform/memory.hpp) at frame 1
# and every 500th frame after; then shoots frame 10,001.
#
# **Why the terrain screen, and why small.** What grew was the renderer's
# queue of frames submitted and not yet drawn, and every frame of every screen
# goes through it: a command buffer, its uniforms, and the overlay's upload.
# The flight screen put it through the same, but flying the aeroplane and
# building the HUD were most of each frame's time in the sanitized build -
# 283 s for the test there, against 59 s for this - and none of it is the
# GPU's. The frame's size is not what costs: 64x48 was no quicker than
# 320x240 by more than a tenth.
#
# **Why the frame after.** The frame shot waits for every terrain tile its
# view needs, which by itself takes the client up by hundreds of MiB, the
# first frame or the ten-thousandth alike. That is the shot, not the frames,
# so the frames sampled are the ten thousand before it.
#
# **The bound.** The terrain, Cesium Native's caches and the driver's pools
# grow while the view settles in; by frame 1,000 they have. From there on the
# highest sample may be at most MAX_GROWTH_MIB, 64 MiB, above the lowest - the
# lowest, not the one at frame 1,000, so a sample that happened to catch a
# peak cannot hide growth after it. Without the renderer's wait on the frame
# two before (gfx/renderer.hpp), lavapipe's queue grew about 0.8 MiB a frame
# and the client died before frame 2,000. Every sample must have been read: a
# platform that will not say fails the test rather than passing it.
#
# The terrain is the DEM's, so it needs the tiles and the geoid, fetched into
# CACHE: without the network the test is skipped (exit 77) unless
# GLIDESLOPE_REQUIRE_NETWORK is set.

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
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 160x120
                        --screen terrain --at "-33.95,151.18,1500" --toward "-33.87,151.21,0"
                        --shot-frame ${_shot_frame} --memory-every ${_every}
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
set(_lowest_mib "")
set(_highest_mib "")
set(_settled_count 0)
set(_last_frame 0)
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
    if(_frame GREATER_EQUAL _settled)
        math(EXPR _settled_count "${_settled_count} + 1")
        if(_lowest_mib STREQUAL "" OR _mib LESS _lowest_mib)
            set(_lowest_mib ${_mib})
        endif()
        if(_highest_mib STREQUAL "" OR _mib GREATER _highest_mib)
            set(_highest_mib ${_mib})
        endif()
    endif()
    set(_last_frame ${_frame})
endforeach()
message(STATUS "the memory held, every ${_every} frames:\n${_table}")
# From frame 1,000 to 10,000 in 500s: 19 samples, and the last at 10,000.
math(EXPR _settled_expected "(${_frames} - ${_settled}) / ${_every} + 1")
if(NOT _settled_count EQUAL _settled_expected OR NOT _last_frame EQUAL _frames)
    message(FATAL_ERROR "${_settled_count} samples from frame ${_settled} to "
                        "${_last_frame}, not ${_settled_expected} to ${_frames}")
endif()
math(EXPR _growth "${_highest_mib} - ${_lowest_mib}")
if(_growth GREATER MAX_GROWTH_MIB)
    message(FATAL_ERROR "from frame ${_settled} to frame ${_frames} the memory held "
                        "ranged ${_growth} MiB, from ${_lowest_mib} to ${_highest_mib}: "
                        "more than ${MAX_GROWTH_MIB} MiB:\n${_table}")
endif()
message(STATUS "from frame ${_settled} to frame ${_frames} the memory held ranged "
               "${_growth} MiB, from ${_lowest_mib} to ${_highest_mib}; at most "
               "${MAX_GROWTH_MIB} MiB")

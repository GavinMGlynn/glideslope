# imagery_stale_refused.cmake - the window client whose cached imagery has
# gone stale, and is refused when it asks for it again, draws its frame and
# exits 0.
#
#   cmake -DPROGRAM=<glideslope> -DTOOL=<glideslope_cache_revalidation>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir> -DPORT=<a port>
#         -P imagery_stale_refused.cmake
#
# **The crash, in the client itself.** The client with the window segfaulted
# as it closed, an imagery tile "given up, unfinished": the tile was in
# Cesium Native's cache, gone stale, and being asked for again, and the cache
# read the status of an answer that had no response (gfx/fetches.hpp). On the
# development machine, whose cache is weeks old, that was one run in many; on
# CI, whose cache is new, never. Here it is built every run:
#
#   1. a shot over Mount Taranaki with its imagery asked of 127.0.0.1 at PORT,
#      a port from the tests' block that nothing listens on, with a Cesium
#      cache of the test's own, made empty. Every imagery fetch is refused, and
#      the client names each as it fails;
#   2. each tile it named is stored in that cache, an hour stale, with an ETag
#      (glideslope_cache_revalidation stale);
#   3. the same shot again. Each of those tiles is in the cache and stale, so
#      it is asked for again, with If-None-Match, and refused.
#
# The second run must exit 0 and write its frame, and must have asked again
# for a tile stored - so the run tested what it says; both counts are said.
# Not every one: which tiles a shot asks for depends on the order its
# refusals land in, run to run (frame_terrain_imagery_refused.cmake), and
# locally 44 of 48 and 45 of 45 were asked again. Before the fix the first
# of them was enough to end it.
# Before the fix it ended at its first refused revalidation: a segfault in a
# release build, the undefined-behaviour sanitizer's report in a debug one.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(_stem "${WORK}/imagery-stale-refused-${DRIVER}")
set(_cesium "${_stem}.sqlite")
file(REMOVE "${_cesium}" "${_cesium}-wal" "${_cesium}-shm")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{GLIDESLOPE_CESIUM_CACHE} "${_cesium}")
set(ENV{LSAN_OPTIONS} "exitcode=0")
string(CONCAT _refused_at "http://127.0.0.1:" "${PORT}" "/")

# The limit is for a hang, not for a slow machine (frame_terrain.cmake).
set(_step_limit_s 600)
function(shoot run out_var)
    set(_shot "${_stem}-${run}.bmp")
    file(REMOVE "${_shot}")
    execute_process(
        COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                --screen terrain --at "-39.33,174.155,1500" --toward "-39.2967,174.0634,2200"
                --imagery on
                --test-imagery-url "${_refused_at}{TileMatrix}/{TileRow}/{TileCol}.jpg"
                --shot "${_shot}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err
        TIMEOUT ${_step_limit_s})
    if(_rc MATCHES "timeout")
        message(FATAL_ERROR "the ${run} shot was still running after ${_step_limit_s} s:\n"
                            "${_out}\n${_err}")
    endif()
    glideslope_skip_when_not_downloaded("${_rc}" "${_err}")
    if(NOT EXISTS "${_shot}" AND _err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the ${run} shot exited ${_rc}:\n${_out}\n${_err}")
    endif()
    if(NOT EXISTS "${_shot}")
        message(FATAL_ERROR "the ${run} shot drew nothing:\n${_out}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
    # Each refused tile, once: "glideslope: URL: URL: why".
    string(REGEX MATCHALL "glideslope: ${_refused_at}[0-9]+/[0-9]+/[0-9]+\\.jpg:"
           _named "${_err}")
    set(_tiles)
    foreach(_one IN LISTS _named)
        string(REGEX REPLACE "^glideslope: (.*):$" "\\1" _one "${_one}")
        list(APPEND _tiles "${_one}")
    endforeach()
    list(REMOVE_DUPLICATES _tiles)
    set(${out_var} "${_tiles}" PARENT_SCOPE)
endfunction()

shoot(first _first)
list(LENGTH _first _stored)
if(_stored EQUAL 0)
    message(FATAL_ERROR "the first shot asked for no imagery, so nothing can go stale")
endif()
execute_process(COMMAND "${TOOL}" stale "${_cesium}" ${_first}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the stale tiles were not stored:\n${_out}\n${_err}")
endif()

shoot(second _second)
set(_asked 0)
set(_not_asked)
foreach(_tile IN LISTS _first)
    if(_tile IN_LIST _second)
        math(EXPR _asked "${_asked} + 1")
    else()
        list(APPEND _not_asked "${_tile}")
    endif()
endforeach()
message(STATUS "${_stored} tiles stored stale, ${_asked} asked for again and refused; "
               "the client drew its frame and exited 0")
if(_asked EQUAL 0)
    message(FATAL_ERROR "of ${_stored} stale tiles, none was asked for again, so "
                        "nothing was tested - not asked: ${_not_asked}")
endif()

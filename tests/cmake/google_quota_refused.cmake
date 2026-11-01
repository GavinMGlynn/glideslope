# google_quota_refused.cmake - a Google terrain shot whose tileset is refused
# for quota says so, and draws no frame.
#
#   cmake -DPROGRAM=<glideslope> -DSTALL=<glideslope_ion_stall> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -P google_quota_refused.cmake
#
# **Built on purpose**: Google's Photorealistic 3D Tiles through Cesium ion are
# fetched with ion's own Google project, whose quota of 150 root requests a
# minute every ion user in the world shares. When it is spent, Google answers
# the root with a 429 and RESOURCE_EXHAUSTED, and Cesium Native then loads
# nothing at all. The shot waited out its 45 s for tiles that could never
# come, drew an empty sky and exited 0, and the terrain test failed "google
# via ion-token drew no terrain at all" in local full suites, never alone.
#
# glideslope_ion_stall's quota mode stands in for both on the loopback: it
# says where asset 2275207 is - itself - and refuses that with Google's 429.
# The run must end without a frame, exit non-zero, and say "could not
# download" and the status - what the provider test reads to report itself
# skipped, never passed - and the stand-in must have refused it, or nothing
# was tested.
#
# With STALL_PORT_FILE, this is the inner half, started beside the stand-in.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_stem "${WORK}/google-quota-${DRIVER}")

if(DEFINED STALL_PORT_FILE)
    while(NOT EXISTS "${STALL_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STALL_PORT_FILE}" _port LIMIT_COUNT 1)
    set(_cache "${_stem}.sqlite")
    set(_nowhere "${_stem}-config")
    file(REMOVE_RECURSE "${_nowhere}")
    file(MAKE_DIRECTORY "${_nowhere}")
    file(REMOVE "${_cache}" "${_cache}-wal" "${_cache}-shm" "${_stem}.bmp")
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    set(ENV{GLIDESLOPE_CESIUM_CACHE} "${_cache}")
    set(ENV{GLIDESLOPE_CESIUM_ION_API} "http://127.0.0.1:${_port}")
    set(ENV{GLIDESLOPE_CESIUM_ION_TOKEN} "not-a-token")
    # No Google key anywhere, so the way in is ion's.
    set(ENV{GLIDESLOPE_GOOGLE_MAPS_KEY} "")
    set(ENV{XDG_CONFIG_HOME} "${_nowhere}")
    set(ENV{APPDATA} "${_nowhere}")
    set(ENV{HOME} "${_nowhere}")
    execute_process(
        COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 160x120
                --screen terrain --terrain google --at -39.0,174.3,4000
                --toward -39.296,174.064,2518 --shot-at 2 --shot "${_stem}.bmp"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_refusal "could not download the google terrain's tileset: the provider answered 429")
    # Skipped only for the DEM's own download failing - copernicus-dem-*, or
    # no answer at all - and never for anything the tileset said: a refusal
    # worded otherwise, say by a Cesium Native that no longer says "Received
    # status code", fails below.
    if(_err MATCHES "could not download(: | https://copernicus-dem-)")
        set(_verdict "SKIP: the DEM could not be had: ${_err}")
    elseif(_rc EQUAL 0)
        string(CONCAT _verdict "FAIL: refused for quota, the run still exited 0 "
                     "and drew a frame:\n${_out}${_err}")
    elseif(NOT _rc MATCHES "^[0-9]+$")
        set(_verdict "FAIL: the run crashed (${_rc}):\n${_err}")
    elseif(NOT _err MATCHES "${_refusal}")
        string(CONCAT _verdict "FAIL: the run exited ${_rc} without saying the "
                     "provider refused it with a 429:\n${_err}")
    elseif(EXISTS "${_stem}.bmp")
        set(_verdict "FAIL: the run said it was refused, and wrote a frame all the same")
    else()
        string(REGEX MATCH "could not download[^\n]*" _said "${_err}")
        set(_verdict "REFUSED: exit ${_rc}, saying: ${_said}")
        glideslope_judge_leaks("${_err}")
    endif()
    file(WRITE "${RESULT_FILE}" "${_verdict}")
    file(DOWNLOAD "http://127.0.0.1:${_port}/stop" "${RESULT_FILE}.stop")
    return()
endif()

file(MAKE_DIRECTORY "${WORK}")
set(_port_file "${_stem}.port")
set(_result_file "${_stem}.result")
file(REMOVE "${_port_file}" "${_result_file}")
execute_process(COMMAND "${STALL}" quota "${_port_file}"
                COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=${PROGRAM}" "-DSTALL=${STALL}"
                        "-DDRIVER=${DRIVER}" "-DWORK=${WORK}" "-DCACHE=${CACHE}"
                        "-DSTALL_PORT_FILE=${_port_file}"
                        "-DRESULT_FILE=${_result_file}"
                        -P "${CMAKE_CURRENT_LIST_FILE}"
                RESULTS_VARIABLE _rcs ERROR_VARIABLE _stall_said)
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the run was never made against the stand-in (${_rcs}):\n"
                        "${_stall_said}")
endif()
file(READ "${_result_file}" _verdict)
if(_verdict MATCHES "^SKIP: (.*)")
    message(STATUS "${CMAKE_MATCH_1}")
    cmake_language(EXIT 77)
endif()
if(NOT _verdict MATCHES "^REFUSED")
    message(FATAL_ERROR "${_verdict}\n${_stall_said}")
endif()
# The run was refused by the stand-in, not stopped by something else.
if(NOT _stall_said MATCHES "answered 1 endpoints and held 0 transfers, and refused ([1-9][0-9]*) requests")
    message(FATAL_ERROR "the stand-in refused nothing, so nothing was tested:\n"
                        "${_stall_said}\n${_verdict}")
endif()
message(STATUS "${_verdict}; the stand-in refused ${CMAKE_MATCH_1} requests")

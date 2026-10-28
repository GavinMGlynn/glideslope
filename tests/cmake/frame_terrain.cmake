# frame_terrain.cmake - the terrain drawn is the terrain the DEM says is there.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_terrain_check> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -DDATA=<data dir>
#         [-DIMAGERY=ON] -P frame_terrain.cmake
#
# or, its imagery refused, through frame_terrain_imagery_refused.cmake.
#
# Shoots the terrain screen headless at 320x240 from 9 km east of Mount
# Taranaki's summit, 1,500 m up its slope, looking just below the summit - a
# cone 2,518 m high on a plain that runs to the sea, all in one DEM tile - and
# has glideslope_terrain_check ray-cast the same view through the DEM and hold
# the frame to it. With IMAGERY, at 640x480 with the open imagery draped on the
# terrain, the reference's ground is the imagery at each place, and the check
# also finds where the imagery matches best.
#
# **With REFUSE_IMAGERY, every imagery fetch fails**, and the frame is still
# drawn, on the DEM's colours, and says why. The imagery is asked for
# (`--imagery on`) from REFUSED_AT (`--test-imagery-url`): 127.0.0.1 at a
# port from the tests' block that no test listens on, so every fetch is
# refused - the situation built, not hoped for from a network having a bad
# minute (frame_terrain_imagery_refused.cmake, which is given the port). The
# client must exit 0, must say on its standard error that the tiles were
# drawn on the DEM's colours because their imagery could not be fetched -
# every tile it drew, counted against the tiles it says the frame holds -
# and the frame is held to the DEM's colours as the shot without imagery is.
# **The imagery's credit is not on it**: none of EOX's imagery is drawn, so
# the client leaves its credit off, and glideslope_terrain_check, reading the
# credits back off the frame, holds them to the DEM's notice alone - a credit
# line more fails it ("the credits, line 1, read ..."). A window client on CI whose
# EOX tiles were reset refused its shot and wrote nothing (run 40618052854).
#
# The terrain
# comes from the DEM's tiles and the geoid, fetched into CACHE: without the
# network the test is skipped (exit 77) unless GLIDESLOPE_REQUIRE_NETWORK is
# set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_eye "-39.33,174.155,1500")
set(_summit "-39.2967,174.0634,2200")

file(MAKE_DIRECTORY "${WORK}")
set(_refuse)
set(_also "without their imagery")
if(REFUSE_IMAGERY)
    set(_name "terrain-imagery-refused-${DRIVER}")
    set(_size 320x240)
    set(_imagery on)
    set(_check_imagery "")
    set(_refuse --test-imagery-url "${REFUSED_AT}{TileMatrix}/{TileRow}/{TileCol}.jpg")
    # A tile drawn without its imagery for any reason but its fetch failing
    # is still refused, and is not a download to skip for.
    set(_also)
elseif(IMAGERY)
    set(_name "terrain-imagery-${DRIVER}")
    set(_size 640x480)
    set(_imagery on)
    set(_check_imagery imagery)
else()
    set(_name "terrain-${DRIVER}")
    set(_size 320x240)
    set(_imagery off)
    set(_check_imagery "")
endif()
set(_shot "${WORK}/${_name}.bmp")
file(REMOVE "${_shot}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size ${_size}
                        --screen terrain --at "${_eye}" --toward "${_summit}"
                        --imagery ${_imagery} ${_refuse}
                        --shot "${_shot}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(_also)
    glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "${_also}")
else()
    glideslope_skip_when_not_downloaded("${_rc}" "${_err}")
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope exited ${_rc}\n${_out}\n${_err}")
endif()
if(NOT EXISTS "${_shot}")
    message(FATAL_ERROR "glideslope exited 0 and drew nothing\n${_out}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")
if(REFUSE_IMAGERY)
    if(NOT _out MATCHES "terrain of ([0-9]+) tiles")
        message(FATAL_ERROR "the client did not say how many tiles it drew:\n${_out}")
    endif()
    set(_drawn "${CMAKE_MATCH_1}")
    set(_why "([0-9]+) terrain tiles were drawn on the DEM's colours: ")
    string(APPEND _why "their imagery could not be fetched")
    if(NOT _err MATCHES "${_why}")
        message(FATAL_ERROR "the client did not say why its tiles have no imagery:\n${_err}")
    endif()
    set(_on_dem "${CMAKE_MATCH_1}")
    if(_drawn EQUAL 0 OR NOT _on_dem EQUAL _drawn)
        message(FATAL_ERROR "of ${_drawn} tiles drawn, ${_on_dem} were said to be "
                            "on the DEM's colours, where every one's imagery was refused")
    endif()
    string(FIND "${_err}" "${REFUSED_AT}" _refused)
    if(_refused EQUAL -1)
        message(FATAL_ERROR "no fetch from ${REFUSED_AT} was seen to fail:\n${_err}")
    endif()
    message(STATUS "${_on_dem} tiles of ${_drawn} drawn on the DEM's colours, their imagery refused")
else()
    glideslope_skip_when_imagery_not_fetched("${_err}")
endif()

execute_process(COMMAND "${CHECK}" "${_shot}" "${WORK}/${_name}-reference.bmp"
                        "${WORK}/${_name}-difference.bmp" "${_eye}" "${_summit}"
                        "${CACHE}" "${DATA}" ${_check_imagery}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

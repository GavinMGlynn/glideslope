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
# **And the terrain is all there**: as many tiles, to the same level, as the
# same shot without imagery draws (`--imagery off`, shot here too). Every
# fetch refused, Cesium Native wedges tiles beneath an ancestor whose imagery
# failed first - never renderable, their parents drawn instead - and on
# Windows and macOS its wait for every tile never ended (CI run
# 40627612271). The client gives the imagery up when it finds that
# (gfx::TerrainTiles), and must say so.
#
# **With WEDGE the wedge is built, not hoped for**: `--test-imagery-wedge`
# holds the imagery's finer fetches until the root tile's has failed, so
# every finer failure lands beneath it. Left to the order the refusals land
# in, CI's Linux once did not wedge, and the give-up this pins was not taken
# (run 40649190581). The client must say the held fetches went, and that it
# gave the imagery up. Without the give-up it drew 19 tiles to level 6 here,
# where without imagery it draws 26 to level 7. Without WEDGE nothing is
# held, and the frame is held to the same as either way.
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
    if(WEDGE)
        list(APPEND _refuse --test-imagery-wedge)
    endif()
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
# **Each program it runs is given a limit of its own**, and one that reaches
# it fails the test naming it, with what it had said: the whole test killed
# by ctest's TIMEOUT says nothing of which of its three programs hung or
# where (CI's Windows debug, run 40686462655, the wedged-imagery test at
# 300 s and no word of it).
set(_step_limit_s 240)
function(glideslope_frame_step_ended what rc out err)
    if(rc MATCHES "timeout")
        message(FATAL_ERROR "${what} was still running after ${_step_limit_s} s, and was "
                            "stopped:\n${out}\n${err}")
    endif()
endfunction()
set(_shot "${WORK}/${_name}.bmp")
file(REMOVE "${_shot}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size ${_size}
                        --screen terrain --at "${_eye}" --toward "${_summit}"
                        --imagery ${_imagery} ${_refuse}
                        --shot "${_shot}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err
                TIMEOUT ${_step_limit_s})
glideslope_frame_step_ended("the shot" "${_rc}" "${_out}" "${_err}")
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
    if(WEDGE)
        if(NOT _err MATCHES "the imagery's finer fetches, held for a test, go now")
            message(FATAL_ERROR "the client did not hold its finer imagery fetches "
                                "until the root's had failed - the wedge was not "
                                "built:\n${_err}")
        endif()
        if(NOT _err MATCHES "the imagery is given up")
            message(FATAL_ERROR "the client did not give its wedged imagery up - the "
                                "path this pins was not taken:\n${_err}")
        endif()
    endif()
    if(NOT _out MATCHES "terrain of [0-9]+ tiles, [0-9]+ loaded, the deepest at level ([0-9]+)")
        message(FATAL_ERROR "the client did not say how deep its terrain went:\n${_out}")
    endif()
    set(_deepest "${CMAKE_MATCH_1}")
    set(_bare "${WORK}/terrain-imagery-refused-bare-${DRIVER}.bmp")
    execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size ${_size}
                            --screen terrain --at "${_eye}" --toward "${_summit}"
                            --imagery off --shot "${_bare}"
                    RESULT_VARIABLE _bare_rc OUTPUT_VARIABLE _bare_out ERROR_VARIABLE _bare_err
                    TIMEOUT ${_step_limit_s})
    glideslope_frame_step_ended("the shot without imagery" "${_bare_rc}" "${_bare_out}"
                                "${_bare_err}")
    if(NOT _bare_rc EQUAL 0 OR NOT _bare_out MATCHES
       "terrain of ([0-9]+) tiles, [0-9]+ loaded, the deepest at level ([0-9]+)")
        message(FATAL_ERROR "the shot without imagery failed (${_bare_rc}):\n${_bare_out}\n${_bare_err}")
    endif()
    if(NOT _drawn EQUAL CMAKE_MATCH_1 OR NOT _deepest EQUAL CMAKE_MATCH_2)
        message(FATAL_ERROR "its imagery refused, the client drew ${_drawn} tiles to level "
                            "${_deepest}; without imagery, ${CMAKE_MATCH_1} to level "
                            "${CMAKE_MATCH_2}")
    endif()
    glideslope_judge_leaks("${_bare_err}")
    if(_err MATCHES "the imagery is given up")
        set(_given_up "and given up")
    else()
        set(_given_up "and not wedged")
    endif()
    message(STATUS "${_on_dem} tiles of ${_drawn} drawn on the DEM's colours to level "
                   "${_deepest}, as without imagery, their imagery refused ${_given_up}")
else()
    glideslope_skip_when_imagery_not_fetched("${_err}")
    # **Imagery that came is never given up**: a fetch failing here and
    # there, under an ancestor whose imagery loaded, is not Cesium Native's
    # wedge, and must not take the imagery off for the run.
    if(_err MATCHES "the imagery is given up")
        message(FATAL_ERROR "the imagery came and was given up:\n${_err}")
    endif()
endif()

execute_process(COMMAND "${CHECK}" "${_shot}" "${WORK}/${_name}-reference.bmp"
                        "${WORK}/${_name}-difference.bmp" "${_eye}" "${_summit}"
                        "${CACHE}" "${DATA}" ${_check_imagery}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err
                TIMEOUT ${_step_limit_s})
glideslope_frame_step_ended("the terrain check" "${_rc}" "${_out}" "${_err}")
message(STATUS "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

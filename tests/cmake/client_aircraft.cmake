# client_aircraft.cmake - the client flies the aircraft it is given, in the air
# or standing on the ground.
#
#   cmake -DPROGRAM=<glideslope> -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -P client_aircraft.cmake
#
# Flies the flight screen headless twice, for 240 ticks - two seconds - with
# --trace: the F-22 given by --aircraft, which the client must say it flies
# and which must be flying at its catalogue start's 300 KCAS; and the Cessna
# --on-ground at Sydney airport, which must stand there, its wheels on the DEM
# and its airspeed nothing, the brakes on. The flights stand on the DEM, so
# they need the tiles and the geoid, fetched into CACHE: without the network
# the test is skipped (exit 77) unless GLIDESLOPE_REQUIRE_NETWORK is set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

# Runs the client with the arguments; its standard output in <out>, the trace's
# line at tick 240 in <last>.
function(fly name out last)
    set(_shot "${WORK}/aircraft-${name}-${DRIVER}.bmp")
    file(REMOVE "${_shot}")
    execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                            --screen flight ${ARGN} --shot-at 240 --shot "${_shot}" --trace
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0 AND _err MATCHES "could not download")
        if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
            message(STATUS "the DEM could not be had: ${_err}")
            cmake_language(EXIT 77)
        endif()
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope ${ARGN} exited ${_rc}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
    string(REGEX MATCH "trace tick 240 [^\n]*" _line "${_out}")
    if(_line STREQUAL "")
        message(FATAL_ERROR "no trace at tick 240 from glideslope ${ARGN}:\n${_out}")
    endif()
    set(${out} "${_out}" PARENT_SCOPE)
    set(${last} "${_line}" PARENT_SCOPE)
endfunction()

fly(f22 _out _last --aircraft f22)
if(NOT _out MATCHES "glideslope: flying the Lockheed Martin F-22A Raptor \\(f22\\)\n")
    message(FATAL_ERROR "the client did not say it flies the F-22:\n${_out}")
endif()
if(NOT _last MATCHES " kcas ([0-9]+)\\.[0-9]+ ")
    message(FATAL_ERROR "no airspeed in the trace: ${_last}")
endif()
if(CMAKE_MATCH_1 LESS 290 OR CMAKE_MATCH_1 GREATER 310)
    message(FATAL_ERROR "the F-22 flies at ${CMAKE_MATCH_1} KCAS, not its start's 300")
endif()
message(STATUS "the F-22 flown at ${CMAKE_MATCH_1} KCAS")

# Sydney airport, beside runway 16R.
fly(ground _out _last --aircraft c172p --on-ground --at -33.9461,151.1772,0)
if(NOT _out MATCHES "glideslope: flying the Cessna 172P Skyhawk \\(c172p\\), standing on the ground\n")
    message(FATAL_ERROR "the client did not say the Cessna stands on the ground:\n${_out}")
endif()
if(NOT _last MATCHES " agl_ft (-?[0-9]+)\\.[0-9]+ kcas ([0-9]+)\\.[0-9]+ ")
    message(FATAL_ERROR "no height or airspeed in the trace: ${_last}")
endif()
if(CMAKE_MATCH_1 GREATER 10 OR CMAKE_MATCH_1 LESS 0 OR CMAKE_MATCH_2 GREATER 2)
    message(FATAL_ERROR "the Cessna is not standing on the ground: ${CMAKE_MATCH_1} ft above "
                        "it at ${CMAKE_MATCH_2} KCAS")
endif()
message(STATUS "the Cessna stands ${CMAKE_MATCH_1} ft above the ground at ${CMAKE_MATCH_2} KCAS")

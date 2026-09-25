# frame_hud_no_weather.cmake - a HUD test with no weather to be had reports
# itself skipped, never passed and never failed.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_hud_check> -DCLI=<glideslope_cli>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir> [-DFLY=...]
#         [-DFLYING=...] -P frame_hud_no_weather.cmake
#
# Runs frame_hud.cmake as its test runs it, with the weather services'
# requests sent to a port nothing listens on - no weather, built on purpose -
# and with GLIDESLOPE_REQUIRE_NETWORK set, as CI sets it. It must exit 77,
# saying there is no weather to fly in. Without the DEM (which is fetched
# before the weather is asked for) the test is itself skipped, unless the
# network is required here.

cmake_minimum_required(VERSION 3.28)

# Port 1 on the loopback: tcpmux, which nothing serves, so each request is
# refused at once and the retries (fetch_with_retries) are the only wait.
set(_nowhere "http://127.0.0.1:1")
set(_required "$ENV{GLIDESLOPE_REQUIRE_NETWORK}")
set(ENV{GLIDESLOPE_REQUIRE_NETWORK} 1)
set(_case "")
if(DEFINED FLYING)
    set(_case "-DFLYING=${FLYING}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}"
                        "-DPROGRAM=${PROGRAM}" "-DCHECK=${CHECK}" "-DCLI=${CLI}"
                        "-DDRIVER=${DRIVER}" "-DWORK=${WORK}/no-weather" "-DCACHE=${CACHE}"
                        "-DFLY=${FLY}" ${_case} "-DWEATHER_SERVICE=${_nowhere}"
                        -P "${CMAKE_CURRENT_LIST_DIR}/frame_hud.cmake"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
set(_said "${_out}${_err}")
if(_rc EQUAL 77 AND _said MATCHES "there is no weather to fly in")
    if(NOT _said MATCHES "127\\.0\\.0\\.1:1")
        message(FATAL_ERROR "skipped for want of weather, but not the weather asked "
                            "of ${_nowhere}:\n${_said}")
    endif()
    message(STATUS "skipped, as it should be, with no weather to be had")
    return()
endif()
# The DEM's failure, and not the weather's, which is what is being built here.
if(NOT _rc EQUAL 0 AND _said MATCHES "could not download" AND
   NOT _said MATCHES "the weather could not be had" AND _required STREQUAL "")
    message(STATUS "the DEM could not be had here, so nothing is shown: ${_said}")
    cmake_language(EXIT 77)
endif()
message(FATAL_ERROR "with no weather to be had the HUD test exited ${_rc}, "
                    "not 77 for skipped:\n${_said}")

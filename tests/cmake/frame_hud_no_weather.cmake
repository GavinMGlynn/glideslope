# frame_hud_no_weather.cmake - a HUD test with no weather to be had reports
# itself skipped; one whose weather service refuses it fails.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_hud_check> -DCLI=<glideslope_cli>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir> [-DFLY=...]
#         [-DFLYING=...] [-DSTUB=<glideslope_http_stub> -DANSWER=<status>]
#         -P frame_hud_no_weather.cmake
#
# Runs frame_hud.cmake as its test runs it, with GLIDESLOPE_REQUIRE_NETWORK
# set, as CI sets it, and the weather services' requests sent elsewhere:
#
#   - Without ANSWER, to a port on the loopback nothing listens on: no
#     weather, built on purpose. It must exit 77, saying there is no weather
#     to fly in.
#   - With ANSWER, to glideslope_http_stub answering every request with that
#     status - 400, as a misspelt Open-Meteo variable is answered. That is a
#     fault of ours, not a service's bad minute, and the HUD test must fail:
#     not pass, and not skip.
#
# Without the DEM (which is fetched before the weather is asked for) the test
# is itself skipped, unless the network is required here.
#
# With STUB_PORT_FILE, this is the inner half of an ANSWER run, started beside
# the stub: it waits for the stub's port, runs the HUD test against it, keeps
# what that did in RESULT_FILE, and stops the stub.

cmake_minimum_required(VERSION 3.28)

set(_case "")
if(DEFINED FLYING)
    set(_case "-DFLYING=${FLYING}")
endif()
function(run_frame_hud service rc_var said_var)
    execute_process(COMMAND "${CMAKE_COMMAND}"
                            "-DPROGRAM=${PROGRAM}" "-DCHECK=${CHECK}" "-DCLI=${CLI}"
                            "-DDRIVER=${DRIVER}" "-DWORK=${WORK}/no-weather" "-DCACHE=${CACHE}"
                            "-DFLY=${FLY}" ${_case} "-DWEATHER_SERVICE=${service}"
                            "-DWEATHER_ANSWER=${ANSWER}"
                            -P "${CMAKE_CURRENT_LIST_DIR}/frame_hud.cmake"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(${rc_var} "${_rc}" PARENT_SCOPE)
    set(${said_var} "${_out}${_err}" PARENT_SCOPE)
endfunction()

# The inner half: the stub is listening, or about to be.
if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _port LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_REQUIRE_NETWORK} 1)
    run_frame_hud("http://127.0.0.1:${_port}" _rc _said)
    file(WRITE "${RESULT_FILE}" "${_rc}\n${_said}")
    file(DOWNLOAD "http://127.0.0.1:${_port}/stop" "${RESULT_FILE}.stop")
    return()
endif()

set(_required "$ENV{GLIDESLOPE_REQUIRE_NETWORK}")
string(MAKE_C_IDENTIFIER "${FLYING}${ANSWER}" _tag)
if(DEFINED ANSWER)
    set(_port_file "${WORK}/no-weather/stub-${DRIVER}-${_tag}.port")
    set(_result_file "${WORK}/no-weather/stub-${DRIVER}-${_tag}.result")
    file(MAKE_DIRECTORY "${WORK}/no-weather")
    file(REMOVE "${_port_file}" "${_result_file}")
    # Both at once: the stub, and this script's inner half beside it.
    execute_process(COMMAND "${STUB}" "${ANSWER}" "${_port_file}"
                    COMMAND "${CMAKE_COMMAND}"
                            "-DPROGRAM=${PROGRAM}" "-DCHECK=${CHECK}" "-DCLI=${CLI}"
                            "-DDRIVER=${DRIVER}" "-DWORK=${WORK}" "-DCACHE=${CACHE}"
                            "-DFLY=${FLY}" ${_case} "-DANSWER=${ANSWER}"
                            "-DSTUB_PORT_FILE=${_port_file}"
                            "-DRESULT_FILE=${_result_file}"
                            -P "${CMAKE_CURRENT_LIST_FILE}"
                    RESULTS_VARIABLE _rcs ERROR_VARIABLE _stub_said)
    if(NOT EXISTS "${_result_file}")
        message(FATAL_ERROR "the HUD test was not run against the stub (${_rcs}):\n"
                            "${_stub_said}")
    endif()
    file(READ "${_result_file}" _result)
    # The exit code, a line, and what it said. (Not a REGEX REPLACE of "^...":
    # CMake matches "^" again after each replacement.)
    string(FIND "${_result}" "\n" _eol)
    string(SUBSTRING "${_result}" 0 ${_eol} _rc)
    math(EXPR _eol "${_eol} + 1")
    string(SUBSTRING "${_result}" ${_eol} -1 _said)
    if(NOT _stub_said MATCHES "answered ([1-9][0-9]*) requests ${ANSWER}")
        message(FATAL_ERROR "the stub was never asked for the weather:\n${_stub_said}\n${_said}")
    endif()
    if(NOT _rc EQUAL 0 AND NOT _rc EQUAL 77 AND _said MATCHES "status ${ANSWER}" AND
       NOT _said MATCHES "no weather to fly in")
        message(STATUS "failed, as it should, when the weather was answered ${ANSWER}")
        return()
    endif()
    if(NOT _rc EQUAL 0 AND _said MATCHES "could not download" AND
       NOT _said MATCHES "127\\.0\\.0\\.1" AND _required STREQUAL "")
        message(STATUS "the DEM could not be had here, so nothing is shown: ${_said}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "with the weather answered ${ANSWER} the HUD test exited ${_rc}, "
                        "when it should fail on that answer:\n${_said}")
endif()

# Port 1 on the loopback: tcpmux, which nothing serves, so each request is
# refused at once and the retries (fetch_with_retries) are the only wait.
set(_nowhere "http://127.0.0.1:1")
set(ENV{GLIDESLOPE_REQUIRE_NETWORK} 1)
run_frame_hud("${_nowhere}" _rc _said)
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

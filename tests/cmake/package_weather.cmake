# package_weather.cmake - the package smoke's weather checks skip, saying why,
# when a weather service does not answer, and fail on every other fault.
#
#   cmake -DBASH=<bash> -DSCRIPT=<tools/package_weather.sh> -DCLI=<glideslope_cli>
#         -DSTUB=<glideslope_http_stub> -DWORK=<scratch dir>
#         -P package_weather.cmake
#
# **What is built**, with no real service asked (GLIDESLOPE_WEATHER_SERVICE
# sends every request to the loopback):
#
#   - the real glideslope_cli, with the weather asked of port 1, which nothing
#     serves, so every request is refused: the check exits 0 and prints a
#     ::warning:: saying the weather check was SKIPPED;
#   - the real glideslope_cli, with the weather asked of glideslope_http_stub
#     answering 400, as a wrong URL is: the check must exit 1 - a fault, not
#     an outage;
#   - a stand-in for each program that says what the real one says when a
#     service does not answer, and one that fails for any other reason: the
#     outage is a skip and anything else fails, for the CLI's check and for
#     the client's frame, which is drawn again without `--weather YSSY` after
#     an outage, and not at all after any other failure.
#
# Skipped (exit 77) where there is no bash, as on a Windows machine without
# Git's; CI's Windows job runs the script with Git's bash.

cmake_minimum_required(VERSION 3.28)

if(NOT BASH)
    message(STATUS "no bash here, so this is skipped")
    cmake_language(EXIT 77)
endif()

file(MAKE_DIRECTORY "${WORK}")

function(run_check rc_var said_var)
    execute_process(COMMAND "${BASH}" "${SCRIPT}" ${ARGN}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(${rc_var} "${_rc}" PARENT_SCOPE)
    set(${said_var} "${_out}${_err}" PARENT_SCOPE)
endfunction()

function(expect_skip what rc said)
    if(NOT rc EQUAL 0 OR NOT said MATCHES "::warning[^\n]*SKIPPED")
        message(FATAL_ERROR "${what}: expected exit 0 and a ::warning:: saying it was "
                            "SKIPPED, got ${rc}:\n${said}")
    endif()
    message(STATUS "${what}: skipped, with its warning")
endfunction()

function(expect_failure what rc said)
    if(rc EQUAL 0 OR said MATCHES "::warning[^\n]*SKIPPED")
        message(FATAL_ERROR "${what}: expected a failure, got ${rc}:\n${said}")
    endif()
    message(STATUS "${what}: failed, as it should (${rc})")
endfunction()

# The inner half, beside the stub: the check against it, kept for the outer.
if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _port LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:${_port}")
    run_check(_rc _said cli "${CLI}")
    file(WRITE "${RESULT_FILE}" "${_rc}\n${_said}")
    file(DOWNLOAD "http://127.0.0.1:${_port}/stop" "${RESULT_FILE}.stop")
    return()
endif()
# The stand-ins.
set(_outage "${WORK}/outage.sh")
file(WRITE "${_outage}" "#!/usr/bin/env bash
for a in \"$@\"; do
    if [ \"$a\" = --weather ] || [ \"$a\" = weather ]; then
        echo \"glideslope: the weather could not be had: could not download https://api.open-meteo.com/v1/forecast: SSL connect timeout\" >&2
        exit 1
    fi
done
echo drew the frame without weather
")
set(_other "${WORK}/other.sh")
file(WRITE "${_other}" "#!/usr/bin/env bash
echo \"glideslope: could not download https://aviationweather.gov/x: status 400\" >&2
exit 3
")
file(CHMOD "${_outage}" "${_other}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

run_check(_rc _said cli "${_outage}")
expect_skip("the cli's weather, a service not answering" "${_rc}" "${_said}")
run_check(_rc _said cli "${_other}")
expect_failure("the cli's weather, failing for another reason" "${_rc}" "${_said}")
run_check(_rc _said frame "${_outage}" --headless --weather YSSY --shot-at 600)
expect_skip("the frame, a service not answering" "${_rc}" "${_said}")
if(NOT _said MATCHES "drew the frame without weather")
    message(FATAL_ERROR "the frame was not drawn again without the weather:\n${_said}")
endif()
run_check(_rc _said frame "${_other}" --headless --weather YSSY --shot-at 600)
expect_failure("the frame, failing for another reason" "${_rc}" "${_said}")
if(NOT _rc EQUAL 3)
    message(FATAL_ERROR "the frame's own exit code, 3, was lost: ${_rc}")
endif()

# The real program, its weather asked of a port nothing listens on.
set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:1")
run_check(_rc _said cli "${CLI}")
expect_skip("glideslope_cli, every request refused" "${_rc}" "${_said}")
if(NOT _said MATCHES "the weather could not be had")
    message(FATAL_ERROR "the warning does not say why:\n${_said}")
endif()

# The real program, its weather service answering 400: a fault, not an outage.
set(_port_file "${WORK}/stub.port")
set(_result_file "${WORK}/stub.result")
file(REMOVE "${_port_file}" "${_result_file}")
execute_process(COMMAND "${STUB}" 400 "${_port_file}"
                COMMAND "${CMAKE_COMMAND}" -DBASH=${BASH} -DSCRIPT=${SCRIPT} -DCLI=${CLI}
                        -DSTUB=${STUB} -DWORK=${WORK}/inner -DSTUB_PORT_FILE=${_port_file}
                        -DRESULT_FILE=${_result_file} -P "${CMAKE_CURRENT_LIST_FILE}"
                RESULTS_VARIABLE _rcs ERROR_VARIABLE _stub_said)
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the check was not run against the stub (${_rcs}):\n${_stub_said}")
endif()
file(READ "${_result_file}" _result)
string(FIND "${_result}" "\n" _eol)
string(SUBSTRING "${_result}" 0 ${_eol} _rc)
math(EXPR _eol "${_eol} + 1")
string(SUBSTRING "${_result}" ${_eol} -1 _said)
if(NOT _stub_said MATCHES "answered ([1-9][0-9]*) requests 400")
    message(FATAL_ERROR "the stub was never asked for the weather:\n${_stub_said}\n${_said}")
endif()
expect_failure("glideslope_cli, the weather service answering 400" "${_rc}" "${_said}")

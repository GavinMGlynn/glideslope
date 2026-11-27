# skip_or_fail_check.cmake - every case of skip_or_fail.cmake's two
# functions, each run in a cmake of its own and judged by how it ended:
# passed (0), skipped (77) or failed (anything else, with its message).
#
#   cmake -P skip_or_fail_check.cmake
#
# The warm-up that times out is a real one: a process given a one-second
# limit and asked to sleep for ten, so the result is the text execute_process
# itself gives, not one written here to match the function.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CASE)
    include("${CMAKE_CURRENT_LIST_DIR}/skip_or_fail.cmake")
    if(CASE STREQUAL "warm_ran")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E true
                        RESULT_VARIABLE _rc ERROR_VARIABLE _err TIMEOUT 60)
        glideslope_warmed_up("${_rc}" "${_err}")
    elseif(CASE MATCHES "^warm_timed_out")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 10
                        RESULT_VARIABLE _rc ERROR_VARIABLE _err TIMEOUT 1)
        glideslope_warmed_up("${_rc}" "${_err}")
    elseif(CASE MATCHES "^warm_not_downloaded")
        glideslope_warmed_up("1" "glideslope_server: could not download https://example/tile.tif")
    elseif(CASE STREQUAL "warm_no_weather")
        glideslope_warmed_up("1" "glideslope_server: the weather could not be had: 503")
    elseif(CASE STREQUAL "warm_stopped")
        glideslope_warmed_up("3" "glideslope_server: no such aircraft")
    elseif(CASE MATCHES "^cannot_draw")
        glideslope_cannot_draw("glideslope: no GPU device")
    else()
        message(FATAL_ERROR "no such case: ${CASE}")
    endif()
    return()
endif()

# case | GLIDESLOPE_REQUIRE_NETWORK | GLIDESLOPE_REQUIRE_WINDOW | ends | said
set(_cases
    "warm_ran|||passed|"
    "warm_timed_out|||failed|did not get its terrain inside its limit"
    "warm_timed_out_network_required|1||failed|did not get its terrain inside its limit"
    "warm_not_downloaded|||skipped|could not be downloaded, so it is skipped"
    "warm_not_downloaded_network_required|1||failed|the warm-up server stopped .exit 1."
    "warm_no_weather|1||skipped|there is no weather to fly in"
    "warm_stopped|||failed|the warm-up server stopped .exit 3."
    "cannot_draw|||skipped|cannot draw here, so this is skipped"
    "cannot_draw_window_required||1|failed|GLIDESLOPE_REQUIRE_WINDOW is set")
# **Coverage asserted**: the warm-up ran, timed out (network required or
# not), could not download (required or not), had no weather, or stopped;
# and the two ends of not drawing.
set(_expected 9)

set(_walked 0)
foreach(_row IN LISTS _cases)
    string(REPLACE "|" ";" _f "${_row}")
    list(GET _f 0 _case)
    list(GET _f 1 _network)
    list(GET _f 2 _window)
    list(GET _f 3 _ends)
    list(LENGTH _f _n)
    set(_said "")
    if(_n GREATER 4)
        list(GET _f 4 _said)
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env --unset=GLIDESLOPE_REQUIRE_NETWORK
                --unset=GLIDESLOPE_REQUIRE_WINDOW
                "GLIDESLOPE_REQUIRE_NETWORK=${_network}" "GLIDESLOPE_REQUIRE_WINDOW=${_window}"
                "${CMAKE_COMMAND}" -DCASE=${_case} -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(_rc STREQUAL "0")
        set(_was passed)
    elseif(_rc STREQUAL "77")
        set(_was skipped)
    else()
        set(_was failed)
    endif()
    if(NOT _was STREQUAL _ends)
        message(FATAL_ERROR "${_case} ${_was} (exit ${_rc}), where it should have ${_ends}:\n${_out}${_err}")
    endif()
    # CMake wraps an error's lines, so the words are looked for unwrapped.
    string(REGEX REPLACE "[ \t\r\n]+" " " _flat "${_out}${_err}")
    if(NOT _said STREQUAL "" AND NOT _flat MATCHES "${_said}")
        message(FATAL_ERROR "${_case} ${_was} without saying '${_said}':\n${_out}${_err}")
    endif()
    message(STATUS "${_case}: ${_was}")
    math(EXPR _walked "${_walked} + 1")
endforeach()
if(NOT _walked EQUAL _expected)
    message(FATAL_ERROR "walked ${_walked} cases of ${_expected}")
endif()
message(STATUS "all ${_walked} of ${_expected} cases ended as they should")

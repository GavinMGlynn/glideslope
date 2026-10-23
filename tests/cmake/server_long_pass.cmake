# server_long_pass.cmake - a server held up a quarter of a second or more says
# so, and how much of it was its own CPU time.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -P server_long_pass.cmake
#
# **Why.** Inputs that arrive while a server is held up longer than
# net::most_steps_held (250 ms) are flown early by what is over, and a client
# predicting through it is put right by metres: stopped 600 ms at 6 s on its
# clock, the 200 ms network check's worst prediction error went from 0.4 m to
# 13.7 m (docs/status, 2026-10-23). Nothing said whether a CI failure of that
# shape was the server's, so the server says each such pass.
#
# **Built, not waited for**: the server runs nothing for 600 ms at 1 s on its
# clock (`--test-stopped-once 1 600`), a machine's stall, and must say a pass
# of 600 ms or more ending within a tenth of a second of that moment, with
# less than half of it its own CPU - a sleep is not work. It needs the DEM's
# tiles to fly its one aircraft, so without the network it reports itself
# skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 3 --ai 1 --headless --data "${DATA}"
            --test-stopped-once 1 600
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

string(REGEX MATCHALL "a pass of [0-9]+ ms, [0-9]+ ms of it this server's CPU, ending at [0-9.]+ s on its clock"
       _passes "${_err}")
set(_found FALSE)
foreach(_pass IN LISTS _passes)
    string(REGEX MATCH "a pass of ([0-9]+) ms, ([0-9]+) ms of it this server's CPU, ending at ([0-9.]+) s"
           _ "${_pass}")
    set(_wall "${CMAKE_MATCH_1}")
    set(_cpu "${CMAKE_MATCH_2}")
    set(_at "${CMAKE_MATCH_3}")
    math(EXPR _half "${_wall} / 2")
    if(_wall GREATER_EQUAL 600 AND _cpu LESS _half AND _at GREATER_EQUAL 1.0
       AND _at LESS 1.1)
        set(_found TRUE)
    endif()
endforeach()
if(NOT _found)
    message(FATAL_ERROR "the server stopped 600 ms at 1 s on its clock did not say a pass "
                        "of 600 ms or more there, mostly not its own CPU:\n${_err}")
endif()
message(STATUS "said: ${_passes}")

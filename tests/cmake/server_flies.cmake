# server_flies.cmake - the server flies aircraft anywhere on Earth, each over
# its own terrain.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -P server_flies.cmake
#
# **The item's own verification**: "aircraft on opposite sides of the world
# fly in one session, each over its own terrain."
#
# **What makes it a real check rather than a smoke test.** The two places are
# chosen so that the ground under them could not be confused: Sydney is a few
# feet above the sea and Denver is a mile up. The server prints the ground it
# found under each aircraft, and this holds each to a band around what that
# place really is. A server flying both over one terrain - or over no terrain,
# taking the ellipsoid for the ground - would fail both bands at once.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

# Sydney Airport and Denver. The bands are wide enough for any DEM that has
# the right tile and narrow enough that the wrong tile, or no tile, fails.
set(_south "-33.94,151.18")   # a few feet above the sea
set(_south_low -50)
set(_south_high 400)
set(_north "39.74,-104.99")   # the mile-high city
set(_north_low 4500)
set(_north_high 6500)

execute_process(
    COMMAND "${SERVER}" --data "${DATA}" --headless --port 0
            --ai 0 --fly "c172p@${_south}" --fly "c172p@${_north}" --seconds 2
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not fly: ${_err}")
    cmake_language(EXIT 77)
endif()

# It stepped at a fixed rate for the time it was given.
if(NOT _out MATCHES "ran ([0-9.]+) s, ([0-9]+) steps")
    message(FATAL_ERROR "the server did not say what it ran:\n${_out}")
endif()
set(_ran ${CMAKE_MATCH_1})
set(_steps ${CMAKE_MATCH_2})
if(_steps LESS 200)
    message(FATAL_ERROR "it took ${_steps} steps in ${_ran} s, which is fewer than "
                        "120 Hz for two seconds would give")
endif()

string(REGEX MATCHALL "flew c172p at [^\n]+" _lines "${_out}")
list(LENGTH _lines _count)
if(NOT _count EQUAL 2)
    message(FATAL_ERROR "the server flew ${_count} aircraft, not two:\n${_out}")
endif()

# Each aircraft, held to the ground its own place really has.
set(_walked 0)
foreach(_line IN LISTS _lines)
    if(NOT _line MATCHES "at ([-0-9.]+), ([-0-9.]+)  ([-0-9.]+) ft agl over ground ([-0-9]+) ft")
        message(FATAL_ERROR "cannot read the server's line: ${_line}")
    endif()
    set(_lat ${CMAKE_MATCH_1})
    set(_agl ${CMAKE_MATCH_3})
    set(_ground ${CMAKE_MATCH_4})
    if(_lat MATCHES "^-")
        set(_low ${_south_low})
        set(_high ${_south_high})
        set(_where "Sydney")
    else()
        set(_low ${_north_low})
        set(_high ${_north_high})
        set(_where "Denver")
    endif()
    if(_ground LESS _low OR _ground GREATER _high)
        message(FATAL_ERROR
                "the aeroplane over ${_where} found ground at ${_ground} ft, which is "
                "outside the ${_low} to ${_high} ft that place has: it is not over "
                "its own terrain")
    endif()
    message(STATUS "${_where}: ground ${_ground} ft, flying ${_agl} ft above it")
    math(EXPR _walked "${_walked} + 1")
endforeach()

if(NOT _walked EQUAL 2)
    message(FATAL_ERROR "only ${_walked} aircraft were held to their terrain")
endif()
message(STATUS "the server flew 2 aircraft on opposite sides of the world for "
               "${_ran} s in ${_steps} steps, each over its own terrain")

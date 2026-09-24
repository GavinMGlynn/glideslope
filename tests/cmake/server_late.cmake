# server_late.cmake - AI aircraft keep flying with nobody connected, and a
# client joining later finds them mid-flight.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_late.cmake
#
# **Both halves are checked, and the second is the hard one.** That the AI fly
# with nobody watching is easy to believe and easy to get wrong in the other
# direction - a server that only stepped its aircraft when a client was
# connected would pass any test that connected at the start. So the client
# waits five seconds from when the server says it is flying (its
# `--ready-file`) before it connects - not five seconds from launch, which a
# debug server on a slow runner spent building its terrain, so that the
# client arrived as it started - and what it finds must be a session
# already running: a clock past the moment it joined, and aircraft that have
# moved away from where the flight plan starts.
#
# **Its own aircraft is the control.** It was made when the client joined, so
# it is still at the plan's start while the AI are not. If everything in the
# packet had moved, the check would be measuring the plan's start point and
# not the passage of time.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

set(_store "${WORK}/late.sqlite")
file(REMOVE "${_store}")
set(_wait 5)

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 2 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 2 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 2 --after-ready "${_ready}"
            --after ${_wait}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client did not finish well:\n${_err}\n${_out}")
endif()

# **The session was already running when it arrived.** The clock in the first
# update it heard is past the moment it connected.
if(NOT _out MATCHES "state: ([0-9]+) aircraft at ([0-9.]+) s, mine is ([-0-9]+)")
    message(FATAL_ERROR "the client heard no state update:\n${_out}")
endif()
set(_count ${CMAKE_MATCH_1})
set(_clock ${CMAKE_MATCH_2})
set(_mine ${CMAKE_MATCH_3})
# **One second, not three: this is about whether it stepped, not how fast.**
# A server that stepped only while somebody was connected would say a few
# hundredths of a second in the first update it sent - the time between the
# client arriving and the packet going out. One that steps on its own says
# however far it got. On a quiet machine that is nearly the whole wait: here,
# the server starts in 0.75 s and runs at real time after. On CI it shares a
# four-core runner with three other tests, one of them rendering on the
# software Vulkan driver across every core it can find, and it fell to a third
# of real time - 1.7 s in five - and a threshold of three failed a server that
# plainly had been flying. One second is still many times what a server that
# waited for company would say.
if(_clock LESS 1)
    message(FATAL_ERROR
            "the client joined after ${_wait} s and the simulation clock said "
            "${_clock} s. A server that steps only while somebody is connected "
            "would say nearly nothing")
endif()
if(NOT _count EQUAL 3)
    message(FATAL_ERROR "the packet held ${_count} aircraft, and two AI plus "
                        "this client's own is three:\n${_out}")
endif()
if(_mine LESS 0)
    message(FATAL_ERROR "the client was given no aircraft")
endif()

# **And the AI had gone somewhere.** The plan starts at -33.905; an aircraft
# that has been flying for five seconds is not still there, and the client's
# own - made when it joined - is.
set(_start_lat "-33.905")
set(_moved 0)
set(_still 0)
string(REGEX MATCHALL "\n  [0-9]+ at [^\n]+" _lines "${_out}")
list(LENGTH _lines _printed)
if(NOT _printed EQUAL 3)
    message(FATAL_ERROR "${_printed} aircraft lines were printed, not three")
endif()
foreach(_line IN LISTS _lines)
    if(NOT _line MATCHES "  ([0-9]+) at ([-0-9.]+), ([-0-9.]+)")
        message(FATAL_ERROR "cannot read the client's line: ${_line}")
    endif()
    set(_index ${CMAKE_MATCH_1})
    set(_lat ${CMAKE_MATCH_2})
    # More than a ten-thousandth of a degree is about eleven metres, which is
    # far more than a position rounds by and far less than five seconds of
    # flight.
    math(EXPR _away "0")
    if(_lat GREATER -33.9049 OR _lat LESS -33.9051)
        set(_away 1)
    endif()
    if(_index EQUAL _mine)
        if(_away)
            message(FATAL_ERROR
                    "the client's own aircraft is at ${_lat}, away from the "
                    "plan's start, though it was made the moment it joined")
        endif()
        math(EXPR _still "${_still} + 1")
    else()
        if(NOT _away)
            message(FATAL_ERROR
                    "an AI aircraft is still at ${_lat}, the place the plan "
                    "starts, after ${_clock} s of flying: the server is not "
                    "stepping them with nobody connected")
        endif()
        math(EXPR _moved "${_moved} + 1")
    endif()
endforeach()
if(NOT _moved EQUAL 2 OR NOT _still EQUAL 1)
    message(FATAL_ERROR "${_moved} aircraft had moved and ${_still} had not, "
                        "and it should be two and one")
endif()

message(STATUS "the client joined after ${_wait} s and found a session at "
               "${_clock} s: two AI aircraft away from where the plan starts, "
               "and its own, numbered ${_mine}, still on it")

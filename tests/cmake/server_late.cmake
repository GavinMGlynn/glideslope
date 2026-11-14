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
# it has flown only since then while the AI have flown since the start. If
# everything in the packet had moved, the check would be measuring the plan's
# start point and not the passage of time. **Judged by how long it has
# flown**, not by a fixed distance: the server says when on the session's
# clock it made her, and she starts in the air at the plan's speed, so by the
# first update she has flown that update's clock less it. Until 2026-11-14
# she had to be within eleven metres of the start - a fifth of a second at
# 100 kt - and on a slow macOS runner (CI run 43513776356) the first update
# came later than that after she was made: 12 m away, and red. **Built, not
# hoped for**: the client joins through a relay holding every datagram a
# tenth of a second each way (glideslope_impair at PORT + 12), so that she
# has always flown more than a fifth of a second before it hears her.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake): another
# build's run of this test on the same port waits for it.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

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
# The relay passes the server's standard output on to standard error: the
# server's lines are read from the one and the client's from the other.
math(EXPR _relay "${PORT} + 12")
port_lock_hold(${_relay})
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 2 --headless
            --data "${DATA}" --store "${_store}" --ready-file "${_ready}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0 --loss 0
            --seed 1 --until-input-ends --seconds 300
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 2 --after-ready "${_ready}"
            --after ${_wait}
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")
foreach(_rc IN LISTS _rcs)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_err}\n${_out}")
    endif()
endforeach()

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

# **And the AI had gone somewhere, and its own only as far as it has
# flown.** Where the plan starts and how fast, from the plan the server
# flies; when the server made her, from what it said.
file(READ "${DATA}/plans/sydney-harbour.plan" _plan)
if(NOT _plan MATCHES "\nstart ([-0-9.]+) ([-0-9.]+) [0-9]+ ([0-9]+) ([0-9]+)\n")
    message(FATAL_ERROR "cannot read where the plan starts and how fast")
endif()
set(_start_lat ${CMAKE_MATCH_1})
set(_start_lon ${CMAKE_MATCH_2})
set(_start_heading ${CMAKE_MATCH_3})
set(_start_kts ${CMAKE_MATCH_4})
# **Heading north**, which is what lets her be judged by how far north she
# is: she starts flying away from the start along it.
if(NOT _start_heading EQUAL 0)
    message(FATAL_ERROR "the plan starts heading ${_start_heading}, and this judges her "
                        "by how far north she has gone")
endif()
# In hundred-thousandths of a degree, as the client prints them.
function(_e5 _deg _var)
    if(NOT _deg MATCHES "^(-?)([0-9]+)\\.([0-9]*)$")
        message(FATAL_ERROR "not a latitude or longitude: ${_deg}")
    endif()
    set(_sign "${CMAKE_MATCH_1}")
    set(_whole ${CMAKE_MATCH_2})
    string(SUBSTRING "${CMAKE_MATCH_3}00000" 0 5 _frac)
    string(REGEX REPLACE "^0+([0-9])" "\\1" _frac "${_frac}")
    math(EXPR _v "${_whole} * 100000 + ${_frac}")
    if(_sign STREQUAL "-")
        math(EXPR _v "0 - ${_v}")
    endif()
    set(${_var} ${_v} PARENT_SCOPE)
endfunction()
# Thousandths of a second.
function(_ms _s _var)
    if(NOT _s MATCHES "^([0-9]+)\\.([0-9]*)$")
        message(FATAL_ERROR "not a time: ${_s}")
    endif()
    set(_whole ${CMAKE_MATCH_1})
    string(SUBSTRING "${CMAKE_MATCH_2}000" 0 3 _frac)
    string(REGEX REPLACE "^0+([0-9])" "\\1" _frac "${_frac}")
    math(EXPR _v "${_whole} * 1000 + ${_frac}")
    set(${_var} ${_v} PARENT_SCOPE)
endfunction()
_e5("${_start_lat}" _start_lat_e5)
_e5("${_start_lon}" _start_lon_e5)
# In still air, which is what makes her ground speed her airspeed's.
if(NOT _err MATCHES "weather at [0-9.]+ s: still air\n")
    message(FATAL_ERROR "the server flew in other than still air, and her speed over the "
                        "ground is not her airspeed's:\n${_err}")
endif()
if(NOT _err MATCHES "a player's aircraft starts on layer [0-9]+, [0-9]+ ft over the plan's start, at ([0-9.]+) s on the session's clock\n")
    message(FATAL_ERROR "the server did not say when it made the client's aircraft:\n${_err}")
endif()
set(_made_s ${CMAKE_MATCH_1})
_ms("${_made_s}" _made_ms)
_ms("${_clock}" _clock_ms)
# **Made the moment it joined**, not at the start with the AI: the client
# waited five seconds of the server flying, and one is the least a slow
# runner has flown in that (above).
if(_made_ms LESS 1000)
    message(FATAL_ERROR "the client's aircraft was made at ${_made_s} s on the session's clock, "
                        "and the client joined after ${_wait} s of it")
endif()
math(EXPR _flown_ms "${_clock_ms} - ${_made_ms}")
if(_flown_ms LESS 0)
    message(FATAL_ERROR "the first update, at ${_clock} s, was before the client's aircraft "
                        "was made, at ${_made_s} s")
endif()
# **The farthest she can have gone north**: the plan's calibrated airspeed,
# made a fifth more for her true airspeed up to the top layer (6,000 ft: 12 %
# more, at 2 % a thousand feet), for as long as she has flown; and eleven
# metres more - a ten-thousandth of a degree - for where a position rounds
# and the step she was made on. **And the least**: four fifths of it, less
# the same eleven metres - so that one made behind the start, flying back
# towards it, is not taken for one made on it (review of #374: made 40 m
# south, she was 23 m south after 290 ms, inside a bound on the distance
# alone). The relay makes her fly long enough for the floor to be above
# the start. A hundred-thousandth of a degree of latitude is 1.1132 m.
math(EXPR _farthest_e5 "${_start_kts} * 12 * 5144 * ${_flown_ms} / 10000 / 11132 + 10")
math(EXPR _least_e5 "${_start_kts} * 8 * 5144 * ${_flown_ms} / 10000 / 11132 - 10")
# **East or west of it, as far as her track to the first waypoint goes**
# for as far north as she can have gone, twice over for her turn onto it,
# and the eleven metres: THE_HEADS is 0.002 degrees east of the start, and
# a fixed eleven metres failed a correct run after about seven seconds of
# flight (re-review of #374) - a slow machine failed sooner than it need be.
if(NOT _plan MATCHES "\nwaypoint [A-Z_]+ +([-0-9.]+) ([-0-9.]+) ")
    message(FATAL_ERROR "cannot read the plan's first waypoint")
endif()
_e5("${CMAKE_MATCH_1}" _first_lat_e5)
_e5("${CMAKE_MATCH_2}" _first_lon_e5)
math(EXPR _track_north "${_first_lat_e5} - ${_start_lat_e5}")
math(EXPR _track_east "${_first_lon_e5} - ${_start_lon_e5}")
if(NOT _track_north GREATER 0)
    message(FATAL_ERROR "the plan's first waypoint is not north of its start")
endif()
if(_track_east LESS 0)
    math(EXPR _track_east "0 - ${_track_east}")
endif()
math(EXPR _farthest_east_e5 "(${_farthest_e5} - 10) * ${_track_east} * 2 / ${_track_north} + 10")
set(_moved 0)
set(_still 0)
set(_nearest_ai_e5 -1)
set(_mine_e5 -1)
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
    set(_lon ${CMAKE_MATCH_3})
    _e5("${_lat}" _lat_e5)
    _e5("${_lon}" _lon_e5)
    math(EXPR _north "${_lat_e5} - ${_start_lat_e5}")
    math(EXPR _east "${_lon_e5} - ${_start_lon_e5}")
    set(_signed_north ${_north})
    set(_signed_east ${_east})
    if(_north LESS 0)
        math(EXPR _north "0 - ${_north}")
    endif()
    if(_east LESS 0)
        math(EXPR _east "0 - ${_east}")
    endif()
    math(EXPR _away_e5 "${_north} + ${_east}")
    if(_index EQUAL _mine)
        if(_signed_north GREATER _farthest_e5 OR _signed_north LESS _least_e5)
            message(FATAL_ERROR
                    "the client's own aircraft is at ${_lat}, ${_signed_north} "
                    "hundred-thousandths of a degree north of the plan's start, and in the "
                    "${_flown_ms} ms since it was made, at ${_made_s} s, it would have gone "
                    "${_least_e5} to ${_farthest_e5}")
        endif()
        if(_east GREATER _farthest_east_e5)
            message(FATAL_ERROR
                    "the client's own aircraft is ${_signed_east} hundred-thousandths of a "
                    "degree east of the plan's start, and in the ${_flown_ms} ms since it "
                    "was made it would have gone no more than ${_farthest_east_e5} either way")
        endif()
        set(_mine_e5 ${_away_e5})
        math(EXPR _still "${_still} + 1")
    else()
        # More than a ten-thousandth of a degree is about eleven metres,
        # which is far more than a position rounds by and far less than a
        # second of flight.
        if(NOT _away_e5 GREATER 10)
            message(FATAL_ERROR
                    "an AI aircraft is still at ${_lat}, the place the plan "
                    "starts, after ${_clock} s of flying: the server is not "
                    "stepping them with nobody connected")
        endif()
        if(_nearest_ai_e5 LESS 0 OR _away_e5 LESS _nearest_ai_e5)
            set(_nearest_ai_e5 ${_away_e5})
        endif()
        math(EXPR _moved "${_moved} + 1")
    endif()
endforeach()
if(NOT _moved EQUAL 2 OR NOT _still EQUAL 1)
    message(FATAL_ERROR "${_moved} aircraft had moved and ${_still} had not, "
                        "and it should be two and one")
endif()
# **Nearer the start than either AI**: she has flown for less of the clock.
if(NOT _mine_e5 LESS _nearest_ai_e5)
    message(FATAL_ERROR "the client's own aircraft is ${_mine_e5} hundred-thousandths of a "
                        "degree from the plan's start, and an AI only ${_nearest_ai_e5}")
endif()

message(STATUS "the client joined after ${_wait} s and found a session at "
               "${_clock} s: two AI aircraft away from where the plan starts, "
               "and its own, numbered ${_mine}, made at ${_made_s} s, "
               "${_mine_e5} hundred-thousandths of a degree from it of the "
               "${_least_e5} to ${_farthest_e5} it would have flown")

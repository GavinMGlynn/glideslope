# server_state.cmake - a client hears where every aircraft is.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_state.cmake
#
# **What this pins** is the whole state stream, end to end and over a real
# socket: the server flies its aircraft, says where they are 25 times a
# simulated second inside a sealed datagram, and a client opens it and reads
# positions back out.
#
# **Why the positions are checked and not just the count.** A state packet
# carries Earth-centred, Earth-fixed metres, and the server builds them from
# the flight model's latitude, longitude and height. A wrong frame, a wrong
# sign or a latitude and longitude the wrong way round all give a packet of
# the right size full of the wrong place - so the client turns them back into
# latitude and longitude and this holds them to the harbour the flight plan
# starts over. The two AI aircraft are stacked 500 ft apart, and that gap is
# checked too, because a height dropped on the way out would not move either
# of them sideways.
#
# **The client goes last in the pipeline** - see server_timeout.cmake for the
# trick - because here it is the client's report that is being read.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

set(_store "${WORK}/state.sqlite")
file(REMOVE "${_store}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# The server needs its terrain before it can fly anything, and on a cold cache
# that is a download. One run first, alone, so the tiles are there before the
# client is waiting on them.
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 2 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err
    TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

set(_ai 2)
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 5 --ai ${_ai} --headless
            --data "${DATA}" --timeout 30 --store "${_store}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 3
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client did not finish well:\n${_err}\n${_out}")
endif()

# It got a session.
if(NOT _out MATCHES "session with ([0-9a-f]+)\n")
    message(FATAL_ERROR "the client never completed a session:\n${_out}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL _key)
    message(FATAL_ERROR "the client got a session with the wrong key")
endif()

# **One update for every twenty-fifth of a simulated second, and no more.**
# This was counted against the wall clock - three seconds should be
# seventy-five - with a floor lowered from fifty to ten as debug servers on
# busy runners fell behind real time, until one sent nine (CI run
# 35857600253): the count measured the runner. The server counts its updates
# in simulated time now (states_per_second in src/frontend/server/main.cpp),
# so the rule has an exact answer however fast the machine is: the client
# says the step of the first update and of the last it heard, and between them
# there must be one update per twenty-fifth of a second crossed. A dropped update, a doubled
# one, an update per pass of the loop or once a second all fail it.
if(NOT _out MATCHES "heard ([0-9]+) state update")
    message(FATAL_ERROR "the client said nothing about state updates:\n${_out}")
endif()
set(_heard ${CMAKE_MATCH_1})
if(NOT _out MATCHES "state updates from step ([0-9]+) to step ([0-9]+) of the simulation")
    message(FATAL_ERROR "the client did not say which steps it heard:\n${_out}")
endif()
set(_first_step ${CMAKE_MATCH_1})
set(_last_step ${CMAKE_MATCH_2})
math(EXPR _span "${_last_step} - ${_first_step}")
# A fifth of a second of simulation at the least - a tenth of real time over
# the three seconds - or the count below would say little.
if(_span LESS 24)
    message(FATAL_ERROR "the client heard updates over only ${_span} steps of the "
                        "simulation in three seconds")
endif()
math(EXPR _updates "${_last_step} * 25 / 120 - ${_first_step} * 25 / 120 + 1")
if(NOT _heard EQUAL _updates)
    message(FATAL_ERROR "the client heard ${_heard} state updates from step "
                        "${_first_step} to step ${_last_step}, and one for every "
                        "twenty-fifth of a second between them is ${_updates}")
endif()

# **Every aircraft the server is flying is in the packet**, and no more. That
# is the AI aircraft plus the one the client was given on joining: a slot
# comes with an aeroplane when the server has terrain to put one over.
math(EXPR _expected "${_ai} + 1")
if(NOT _out MATCHES "state: ([0-9]+) aircraft at ([0-9.]+) s, mine is ([-0-9]+)")
    message(FATAL_ERROR "the client did not report the aircraft:\n${_out}")
endif()
if(NOT CMAKE_MATCH_1 EQUAL _expected)
    message(FATAL_ERROR "the packet held ${CMAKE_MATCH_1} aircraft and the "
                        "server is flying ${_ai} plus this client's own")
endif()
# **And the client knows which one is its own.** Without that it could not
# reconcile a prediction against anything; -1 is what it prints when the
# server gave it none.
set(_mine ${CMAKE_MATCH_3})
if(_mine LESS 0)
    message(FATAL_ERROR "the server gave the client no aircraft, though it has "
                        "terrain loaded and a slot to spare")
endif()
if(_mine GREATER 3)
    message(FATAL_ERROR "the client's own aircraft is numbered ${_mine}, and a "
                        "player's aircraft is numbered by their slot, 0 to 3")
endif()

# **And they are where they really are.** The flight plan starts over Sydney
# Harbour; a position built in the wrong frame lands nowhere near it.
string(REGEX MATCHALL "\n  [0-9]+ at [-0-9.]+, [-0-9.]+  [-0-9.]+ m heading [-0-9.]+"
       _lines "${_out}")
if(NOT _lines)
    string(REGEX MATCHALL "\n  [0-9]+ at [^\n]+" _lines "${_out}")
endif()
list(LENGTH _lines _count)
if(NOT _count EQUAL _expected)
    message(FATAL_ERROR "${_count} aircraft lines were printed, not ${_expected}:\n${_out}")
endif()

set(_walked 0)
set(_heights "")
foreach(_line IN LISTS _lines)
    if(NOT _line MATCHES "at ([-0-9.]+), ([-0-9.]+) +([-0-9.]+) m")
        message(FATAL_ERROR "cannot read the client's line: ${_line}")
    endif()
    set(_lat ${CMAKE_MATCH_1})
    set(_lon ${CMAKE_MATCH_2})
    set(_height ${CMAKE_MATCH_3})
    # Sydney Harbour, wide enough for any plan that starts over it.
    if(_lat GREATER -33.5 OR _lat LESS -34.5)
        message(FATAL_ERROR "an aircraft is at latitude ${_lat}, and the plan "
                            "starts over Sydney Harbour")
    endif()
    if(_lon LESS 150.5 OR _lon GREATER 152.0)
        message(FATAL_ERROR "an aircraft is at longitude ${_lon}, and the plan "
                            "starts over Sydney Harbour")
    endif()
    if(_height LESS 100 OR _height GREATER 5000)
        message(FATAL_ERROR "an aircraft is at ${_height} m, which is not a "
                            "height this plan flies at")
    endif()
    list(APPEND _heights ${_height})
    math(EXPR _walked "${_walked} + 1")
endforeach()
if(NOT _walked EQUAL _expected)
    message(FATAL_ERROR "only ${_walked} of ${_expected} aircraft were held to a place")
endif()

# **Stacked 500 ft apart**, which is 152 m. A height lost on the way out would
# leave them in the same place without moving either sideways, so this is the
# check that catches it. The first two lines are the AI aircraft - the fleet
# is built before anybody joins, so a player's aircraft is always last.
list(GET _heights 0 _first)
list(GET _heights 1 _second)
math(EXPR _gap "${_second} - ${_first}")
if(_gap LESS 0)
    math(EXPR _gap "0 - ${_gap}")
endif()
if(_gap LESS 100 OR _gap GREATER 250)
    message(FATAL_ERROR "the two aircraft are ${_gap} m apart in height, and "
                        "the server stacks them 500 ft - about 152 m - apart")
endif()

message(STATUS "the client heard ${_heard} state updates in three seconds - one "
               "for each 1/25 s from step ${_first_step} to ${_last_step} - each "
               "with ${_expected} aircraft over Sydney Harbour - ${_ai} of the "
               "server's ${_gap} m apart in height, and its own, numbered ${_mine}")

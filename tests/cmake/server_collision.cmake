# server_collision.cmake - two aircraft on a collision course collide on the
# server, every client reading the state is told, and both fly again.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_collision.cmake
#
# **Built, not hoped for.** Two Boeing 737-300s are put on one meridian over
# the sea off Sydney, 3.3 km apart at the same height, facing each other, and
# their autopilots hold that course (`--fly ID@LAT,LON,HEADING`); they close at
# about 500 knots and meet in about thirteen seconds. A 737's reach is half its
# 94 ft span, so they collide unless something is wrong. (Two Cessnas set the
# same way passed 20 m apart - 11 m is a Cessna's reach - which is a miss, and
# why this is not a Cessna.)
#
# Two clients stay connected all the while, with fixed keys so that what each
# heard can be told apart; each says on standard error what it heard. The
# server must say both are wrecks, having collided with each other, and that
# five seconds later both fly again; and both clients must have heard all four.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/collision.sqlite")
file(REMOVE "${_store}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 0 --data "${DATA}"
            --fly 737-300@-33.90,151.40,0
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

# The clients' names are the first eight digits of their public keys.
set(_one e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26)
set(_two d1b109e3db55e52705b4664f92a64ab2c0a03c0e6d62fb9af829e908a06d48fc)
# Each client writes what it heard to a file of its own: on Windows the
# standard error of a pipeline came back empty.
set(_heard_one "${WORK}/heard-one.txt")
set(_heard_two "${WORK}/heard-two.txt")
file(REMOVE "${_heard_one}" "${_heard_two}")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 30 --after 1 --key ${_one}
            --heard "${_heard_one}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 30 --after 1 --key ${_two}
            --heard "${_heard_two}"
    # Until both clients have gone, which is long enough on any machine for the
    # collision and the flying again they are waiting to hear.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 0 --headless
            --data "${DATA}" --timeout 3 --store "${_store}"
            --fly 737-300@-33.90,151.40,0 --fly 737-300@-33.87,151.40,180
    RESULT_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

# **The server resolved it**: both wrecks, each having hit the other, then
# both flying again.
foreach(_said IN ITEMS
        "aircraft 4, 737-300, is a wreck: collided with aircraft 5"
        "aircraft 5, 737-300, is a wreck: collided with aircraft 4"
        "aircraft 4, 737-300, flies again"
        "aircraft 5, 737-300, flies again")
    string(FIND "${_out}" "${_said}" _at)
    if(_at LESS 0)
        message(FATAL_ERROR "the server never said '${_said}':\n${_out}\n${_err}")
    endif()
endforeach()

# **And every client heard it**, each in its own file.
set(_heard 0)
foreach(_file IN ITEMS "${_heard_one}" "${_heard_two}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "a client heard nothing at all - ${_file} was never written:\n${_err}")
    endif()
    file(READ "${_file}" _said)
    foreach(_what IN ITEMS "aircraft 4 is a wreck" "aircraft 5 is a wreck"
                           "aircraft 4 flies again" "aircraft 5 flies again")
        string(FIND "${_said}" "${_what}" _at)
        if(_at LESS 0)
            message(FATAL_ERROR "a client never heard '${_what}':\n${_said}")
        endif()
        math(EXPR _heard "${_heard} + 1")
    endforeach()
endforeach()
if(NOT _heard EQUAL 8)
    message(FATAL_ERROR "${_heard} of the 8 things the two clients should hear were heard")
endif()
message(STATUS "two 737s collided and flew again, and both clients heard all four")

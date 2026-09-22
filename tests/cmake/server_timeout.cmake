# server_timeout.cmake - a server lets go a client it stops hearing from, and
# the slot goes back to the session.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DWORK=<a scratch directory> -DPORT=<a port> -P server_timeout.cmake
#
# **The rule this pins** is `--timeout SECONDS` doing something: a client that
# has gone - crashed, unplugged, walked away - must not hold a slot for ever,
# because a session has at most four and `REQUIREMENTS.md` 6.6 gives no other
# way to get one back.
#
# **Two processes at once, from one `execute_process`.** CMake runs the
# commands of one `execute_process` as a pipeline, so they run together; the
# client's output goes into the server's standard input, which the server
# never reads, and `OUTPUT_VARIABLE` catches the last command's output, which
# is the server's. That is the whole trick, and it is why the client is first.
#
# The client cannot wait for the server to bind, because both start at once -
# so it resends its initiation every 250 ms for five seconds, which is what a
# client over UDP has to do anyway.
#
# It needs no network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

set(_store "${WORK}/timeout.sqlite")
file(REMOVE "${_store}")

# The key first: the client is given the server's public half out of band, and
# the store is how the same server comes back with the same key. One short run
# to mint it and write it down.
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server would not start to mint a key:\n${_err}")
endif()
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}")
endif()
set(_key "${CMAKE_MATCH_1}")

# The timeout is half a second and the server runs for three, so a client that
# connects at the start is let go with two seconds to spare - wide enough that
# a slow machine does not fail this, short enough that a server which never
# let anybody go cannot pass it.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 3 --ai 0 --headless
            --timeout 0.5 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server stopped badly:\n${_err}\n${_out}")
endif()

# It took the client in.
if(NOT _out MATCHES "admitted ([0-9a-f]+) to slot ([0-9]+)\n")
    message(FATAL_ERROR
            "the server never admitted the client, so this says nothing about "
            "the timeout:\n${_out}")
endif()
set(_who "${CMAKE_MATCH_1}")
set(_slot "${CMAKE_MATCH_2}")
if(NOT _slot EQUAL 0)
    message(FATAL_ERROR "the first client should have slot 0, not ${_slot}")
endif()

# And let it go once it went quiet - which it did the moment it exited.
if(NOT _out MATCHES "let go ([^ ]+) after ([0-9.]+) s of silence\n")
    message(FATAL_ERROR
            "the server held the slot for the whole run: --timeout does "
            "nothing:\n${_out}")
endif()
set(_after "${CMAKE_MATCH_2}")

# Let go for the timeout it was given, not for some other length of time. The
# sweep runs each time round the loop, so it is a little late and never early.
if(_after LESS 0.5)
    message(FATAL_ERROR "it let the client go after ${_after} s, sooner than "
                        "the 0.5 s it was told to wait")
endif()
if(_after GREATER 1.5)
    message(FATAL_ERROR "it let the client go after ${_after} s, far longer "
                        "than the 0.5 s it was told to wait")
endif()

# It was let go once, not once round every loop: the connection is forgotten,
# not merely reported.
string(REGEX MATCHALL "let go [^\n]+" _goings "${_out}")
list(LENGTH _goings _times)
if(NOT _times EQUAL 1)
    message(FATAL_ERROR "the server let a client go ${_times} times, so it is "
                        "not forgetting them:\n${_out}")
endif()

message(STATUS "the server admitted ${_who} to slot ${_slot} and let it go "
               "after ${_after} s of silence, with --timeout 0.5")

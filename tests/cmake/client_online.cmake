# client_online.cmake - a client started with --online reaches the server
# server.txt names.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DWORK=<scratch> -DPORT=<a port> -P client_online.cmake
#
# **The item's own verification.** `--online` is how somebody joins a server
# they were given a line about rather than a command line: `server.txt` names
# a host, a port and the server's public key, and the key in it is the half
# the server prints at startup, so the line can be sent to anybody.
#
# **The file is named by the environment, not written into the config
# directory.** A test that dropped a `server.txt` into the person's own
# `~/.config/glideslope` would be editing their machine to make itself pass.
# `GLIDESLOPE_SERVER_TXT` exists for this.
#
# Two processes at once, from one `execute_process` - see
# `server_timeout.cmake` for the trick. The client goes last because it is the
# client's report that says it got there.
#
# It needs no network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

set(_store "${WORK}/online.sqlite")
set(_txt "${WORK}/server.txt")
file(REMOVE "${_store}" "${_txt}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# A server.txt as somebody would be sent one, comment and all.
file(WRITE "${_txt}"
     "# the club's server\n\n127.0.0.1 ${PORT} ${_key}\n")
set(ENV{GLIDESLOPE_SERVER_TXT} "${_txt}")

execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 6 --ai 0 --headless
            --timeout 30 --store "${_store}"
    COMMAND "${CLIENT}" connect --online 2
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client did not finish well:\n${_err}\n${_out}")
endif()

# It read the file.
if(NOT _out MATCHES "server.txt: 127.0.0.1 port ${PORT}")
    message(FATAL_ERROR "the client did not say what it read from server.txt:\n${_out}")
endif()

# **And it reached the server that file names**, with a session on the key the
# file gave it.
if(NOT _out MATCHES "session with ([0-9a-f]+)\n")
    message(FATAL_ERROR "the client never completed a session:\n${_out}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL _key)
    message(FATAL_ERROR "it got a session with ${CMAKE_MATCH_1}, and server.txt "
                        "names ${_key}")
endif()
if(NOT _out MATCHES "answered ([0-9]+) ping")
    message(FATAL_ERROR "the client said nothing about staying:\n${_out}")
endif()
if(CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "it answered no pings, so the session did nothing")
endif()

# **And a client with no server.txt says so rather than connecting to
# nothing.** The environment names a file that is not there.
set(ENV{GLIDESLOPE_SERVER_TXT} "${WORK}/there-is-no-such-file.txt")
execute_process(COMMAND "${CLIENT}" connect --online 1
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(_rc EQUAL 0)
    message(FATAL_ERROR "--online succeeded with no server.txt at all")
endif()
if(NOT _err MATCHES "needs a server.txt")
    message(FATAL_ERROR "--online with no file did not say what it needs:\n${_err}")
endif()

message(STATUS "a client given only a server.txt reached ${_key} on port ${PORT}, "
               "and says what it needs when there is no such file")

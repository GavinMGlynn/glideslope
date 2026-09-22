# server_ping.cmake - the dashboard shows a real round trip and real traffic
# for a client that stays connected.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DWORK=<a scratch directory> -DPORT=<a port> -P server_ping.cmake
#
# **The rule this pins** is `REQUIREMENTS.md` 6.6: the dashboard shows
# "connected clients, ping, traffic". Until a client could connect those
# columns were dashes; this holds them to numbers that came off a socket.
#
# **Two processes at once, from one `execute_process`** - see
# `server_timeout.cmake`, which explains the trick and why the client is
# first. `--plain` is what makes the dashboard readable: the same rows without
# the escape codes that clear the screen.
#
# It needs no network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

set(_store "${WORK}/ping.sqlite")
file(REMOVE "${_store}")

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

# The client stays three seconds, the server runs four, and the timeout is
# long enough that nobody is let go: this is about the columns, not the sweep.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 3
    COMMAND "${SERVER}" --port ${PORT} --seconds 4 --ai 0 --plain
            --timeout 30 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server stopped badly:\n${_err}\n${_out}")
endif()

# --plain draws each pass one after another. The last is the one with the most
# traffic on it, so that is the one held.
string(REGEX MATCHALL "--- dashboard at [0-9]+ s ---" _passes "${_out}")
list(LENGTH _passes _drawn)
if(_drawn LESS 3)
    message(FATAL_ERROR "the dashboard was drawn ${_drawn} times in four "
                        "seconds, and it draws once a second:\n${_out}")
endif()
# The last pass, whatever second it landed on: it is the one with the most
# traffic behind it.
string(FIND "${_out}" "--- dashboard at " _at REVERSE)
if(_at EQUAL -1)
    message(FATAL_ERROR "there is no dashboard to read:\n${_out}")
endif()
string(SUBSTRING "${_out}" ${_at} -1 _last)

# Slot 0's row: who, the round trip in milliseconds, and the bytes each way.
if(NOT _last MATCHES "\n  0     ([0-9a-f]+) +([0-9]+) +([0-9.kM]+) +([0-9.kM]+)\n")
    message(FATAL_ERROR
            "slot 0 has no row with a ping and traffic on it. A client that "
            "stayed connected for three seconds should have all three:\n${_last}")
endif()
set(_who "${CMAKE_MATCH_1}")
set(_ping "${CMAKE_MATCH_2}")
set(_in "${CMAKE_MATCH_3}")
set(_out_bytes "${CMAKE_MATCH_4}")

# On loopback a round trip is under a millisecond, which the column rounds to
# 0. What is held is that it is a number and not a dash, and that it is not
# something absurd - a whole second on loopback would mean it is measuring the
# wrong two moments.
if(_ping GREATER 500)
    message(FATAL_ERROR "the round trip on loopback is given as ${_ping} ms, "
                        "which is not a round trip on loopback")
endif()

# The traffic: the server knocked three or four times and was answered, so
# both directions carry more than the handshake alone. A knock sealed is 6 +
# 8 + 9 + 16 = 39 bytes, so three each way is over a hundred.
foreach(_pair "in:${_in}" "out:${_out_bytes}")
    string(REGEX REPLACE "^([a-z]+):(.*)$" "\\1" _which "${_pair}")
    string(REGEX REPLACE "^([a-z]+):(.*)$" "\\2" _value "${_pair}")
    if(_value MATCHES "^[0-9]+$")
        if(_value LESS 100)
            message(FATAL_ERROR "only ${_value} bytes ${_which} after three "
                                "seconds of knocking, which is too few to be real")
        endif()
    endif()
endforeach()

message(STATUS "slot 0 is ${_who}, round trip ${_ping} ms, ${_in} in and "
               "${_out_bytes} out, drawn ${_drawn} times")

# server_stranger_full.cmake - a stranger on a full server is refused
# `SERVER_FULL`, its initiation read no further than its key, and the player
# keeps their session.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DWORK=<scratch> -DPORT=<a port> -P server_stranger_full.cmake
#
# **Why.** A full server now reads an initiation as far as its static key, so
# that a player started again from a new port is let back in at once
# (server_restart_full.cmake). That must not become work it does for a
# stranger: a key that is no player's is refused after the one X25519
# operation it took to unseal it, before the four that answering would cost,
# and the server says so with the count - `Responder::x25519_done`, which the
# unit test `a_full_server_reads_a_strangers_initiation_no_further_than_its_key`
# holds to the operations actually done.
#
# **The situation is built, not hoped for.** A server of one player
# (`--players 1`), the player in it and staying until the stranger has gone
# (`--until-exists` on the stranger's `--done`); the stranger, another key,
# starts once the player's session has begun (`--after-ready` on its
# `--heard`). The server runs until both have gone (`--until-empty`).
#
# **What must hold**: the stranger refused, reason 5 (`SERVER_FULL`); the
# server saying it read the stranger's key and no further, at one X25519
# operation; the stranger admitted nowhere; and the player admitted once and
# never let go for anything but its own goodbye.
#
# It needs no network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

set(_store "${WORK}/stranger.sqlite")
set(_ready "${WORK}/ready.txt")
set(_player "${WORK}/player.txt")
set(_stranger "${WORK}/stranger.txt")
set(_stranger_done "${WORK}/stranger_done.txt")
file(REMOVE "${_store}" "${_ready}" "${_player}" "${_stranger}" "${_stranger_done}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# The player's key's public half begins 11fc7622; the stranger's is minted.
set(_secret 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2)
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_ready}" --heard "${_player}" --until-exists "${_stranger_done}"
            --key ${_secret}
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4
            --after-ready "${_player}" --heard "${_stranger}" --done "${_stranger_done}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 0 --headless
            --players 1 --timeout 120 --store "${_store}" --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

file(READ "${_stranger}" _heard)
if(NOT _heard MATCHES "refused, reason 5")
    message(FATAL_ERROR "the stranger was not refused SERVER_FULL:\n${_heard}\n"
                        "${_out}\n${_err}")
endif()

string(CONCAT _said "refused 127\\.0\\.0\\.1:[0-9]+: the server is full, and its key "
                    "read no further than it took to know it is no player's here "
                    "\\(1 X25519\\)")
string(REGEX MATCHALL "${_said}" _cheap "${_out}")
list(LENGTH _cheap _cheaply)
if(_cheaply EQUAL 0)
    message(FATAL_ERROR "the server did not say it read the stranger's initiation no "
                        "further than its key, at one X25519 operation:\n${_out}\n${_err}")
endif()

string(REGEX MATCHALL "admitted [0-9a-f]+ to slot" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 1 OR NOT _out MATCHES "admitted 11fc7622 to slot 0")
    message(FATAL_ERROR "${_admissions} admitted, not the player alone:\n${_out}\n${_err}")
endif()
if(_out MATCHES "of silence" OR _out MATCHES "took over")
    message(FATAL_ERROR "the player's session was let go or taken over:\n${_out}\n${_err}")
endif()
message(STATUS "a stranger on a full server was refused SERVER_FULL ${_cheaply} times, "
               "each read no further than its key, at one X25519 operation")

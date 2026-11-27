# server_layout_refused.cmake - a client of another control layout is refused
# at the handshake, `WRONG_LAYOUT`, and says so by name.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DWORK=<scratch> -DPORT=<a port> -P server_layout_refused.cmake
#
# **Why.** Before a first release the version does not move with a message's
# layout, so a client sending another number of controls than its server
# reads was let in, and every input it sent was dropped as the wrong length,
# with nothing to say why. The initiation's payload now begins with the count
# (net::write_joining, docs/TRANSPORT.md "Starting a session"), and a server
# reading another refuses it by name.
#
# **Every way a count can be wrong, and the right one**, on one server, one
# after another (each `--after-ready` on the one before's `--heard`): a client
# saying twenty controls, one fewer than the server reads; one saying
# twenty-two, one more; one saying no count at all (`--controls none`, an
# empty payload); and then this build's own client, saying twenty-one, which
# is let in - so that the refusals are the layout's, not a server that admits
# nobody. The server runs until the last has gone (`--until-empty`).
#
# **What must hold**: each wrong client heard `refused, reason 8,
# WRONG_LAYOUT`; the server said, for each, what it was sent and what it
# reads; and only the right client was admitted.
#
# It needs no network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

set(_store "${WORK}/layout.sqlite")
set(_ready "${WORK}/ready.txt")
set(_fewer "${WORK}/fewer.txt")
set(_more "${WORK}/more.txt")
set(_none "${WORK}/none.txt")
set(_right "${WORK}/right.txt")
file(REMOVE "${_store}" "${_ready}" "${_fewer}" "${_more}" "${_none}" "${_right}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# The right client's key's public half begins 11fc7622.
set(_secret 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2)
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --controls 20
            --after-ready "${_ready}" --heard "${_fewer}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --controls 22
            --after-ready "${_fewer}" --heard "${_more}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --controls none
            --after-ready "${_more}" --heard "${_none}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 2
            --after-ready "${_none}" --heard "${_right}" --key ${_secret}
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 0 --headless
            --timeout 120 --store "${_store}" --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")

set(_refusals 0)
foreach(_case "fewer;20 controls" "more;22 controls" "none;no count of controls")
    list(GET _case 0 _which)
    list(GET _case 1 _says)
    set(_file "${WORK}/${_which}.txt")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "the client saying ${_says} wrote nothing:\n${_out}\n${_err}")
    endif()
    file(READ "${_file}" _heard)
    if(NOT _heard MATCHES "refused, reason 8, WRONG_LAYOUT\n")
        message(FATAL_ERROR "the client saying ${_says} was not refused WRONG_LAYOUT:\n"
                            "${_heard}\n${_out}\n${_err}")
    endif()
    if(NOT _out MATCHES "refused [0-9a-f]+ from 127\\.0\\.0\\.1:[0-9]+: it sends ${_says} in an input frame, and this server reads 21\n")
        message(FATAL_ERROR "the server did not say it refused the client saying ${_says}, "
                            "and why:\n${_out}\n${_err}")
    endif()
    math(EXPR _refusals "${_refusals} + 1")
endforeach()
if(NOT _refusals EQUAL 3)
    message(FATAL_ERROR "${_refusals} of the 3 wrong layouts were checked")
endif()

string(REGEX MATCHALL "admitted [0-9a-f]+ to slot" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 1 OR NOT _out MATCHES "admitted 11fc7622 to slot 0")
    message(FATAL_ERROR "${_admissions} admitted, not the right client alone:\n${_out}\n${_err}")
endif()
message(STATUS "the clients saying 20, 22 and no controls were refused WRONG_LAYOUT, "
               "by name; the one saying 21 was admitted")

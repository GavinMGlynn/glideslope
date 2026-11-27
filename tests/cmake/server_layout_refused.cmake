# server_layout_refused.cmake - a client of another control layout is refused
# at the handshake, `WRONG_LAYOUT`, and says so by name.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DWORK=<scratch> -DPORT=<a port> -P server_layout_refused.cmake
#
# **Why.** Before a first release the version does not move with a message's
# layout, so a client sending another layout of controls than its server
# reads was let in, and every input it sent was dropped as the wrong length -
# or, with the same count in another order, flown as another control - with
# nothing to say why. The initiation's payload now begins with the layout's
# fingerprint (net::write_joining, sim::control_layout, docs/TRANSPORT.md
# "Starting a session"), and a server whose own is another refuses it by name.
#
# **Every way a layout can be wrong, and the right one**, on one server, one
# after another (each `--after-ready` on the one before's `--heard`), each
# made by `glideslope_cli connect --layout`: this build's controls with the
# aileron and rudder swapped (`swap:1:2`), the same count in another order;
# with the yaw trim left out (`without:20`), one fewer; with one more
# (`with:spare`); and no layout at all (`none`, an empty payload). Then this
# build's own client, which is let in - so that the refusals are the
# layout's, not a server that admits nobody. The server runs until the last
# has gone (`--until-empty`).
#
# **What must hold**: each wrong client heard `refused, reason 8,
# WRONG_LAYOUT`; the server said, for each, the layout it was sent and its
# own; and only the right client was admitted.
#
# It needs no network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

set(_store "${WORK}/layout.sqlite")
set(_ready "${WORK}/ready.txt")
set(_cases swap:1:2 without:20 with:spare none)
set(_files)
foreach(_case IN LISTS _cases)
    string(REPLACE ":" "_" _name "${_case}")
    list(APPEND _files "${WORK}/${_name}.txt")
endforeach()
set(_right "${WORK}/right.txt")
file(REMOVE "${_store}" "${_ready}" ${_files} "${_right}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# **The flag refuses what it cannot read**, saying so, before anything is
# sent: not a guess, a clamp or a throw. Every form it takes, wrong.
set(_bad_flags 0)
foreach(_bad "bogus" "1234567" "123456789" "1234567g" "swap:1:1" "swap:1" "swap:1:21"
             "swap:a:2" "without:21" "without:" "without:-1" "with:")
    execute_process(
        COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 1 --layout "${_bad}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _bad_out ERROR_VARIABLE _bad_err)
    if(NOT _rc EQUAL 2 OR NOT _bad_err MATCHES "glideslope_cli: (--layout wants|swap:I:J wants|without:I wants)")
        message(FATAL_ERROR "--layout ${_bad} was not refused as unreadable (${_rc}):\n"
                            "${_bad_out}${_bad_err}")
    endif()
    math(EXPR _bad_flags "${_bad_flags} + 1")
endforeach()
if(NOT _bad_flags EQUAL 12)
    message(FATAL_ERROR "${_bad_flags} of 12 unreadable --layout values were tried")
endif()

# The right client's key's public half begins 11fc7622.
set(_secret 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2)
list(GET _files 0 _swapped)
list(GET _files 1 _fewer)
list(GET _files 2 _more)
list(GET _files 3 _none)
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --layout swap:1:2
            --after-ready "${_ready}" --heard "${_swapped}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --layout without:20
            --after-ready "${_swapped}" --heard "${_fewer}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --layout with:spare
            --after-ready "${_fewer}" --heard "${_more}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 4 --layout none
            --after-ready "${_more}" --heard "${_none}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 2
            --after-ready "${_none}" --heard "${_right}" --key ${_secret}
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 0 --headless
            --timeout 120 --store "${_store}" --ready-file "${_ready}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")

set(_said "refused [0-9a-f]+ from 127\\.0\\.0\\.1:[0-9]+: its controls' layout is ")
set(_ours ", and this server's is 03281B95\n")
string(REGEX MATCHALL "${_said}[0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]${_ours}"
       _other_layouts "${_out}")
string(REGEX MATCHALL "${_said}none${_ours}" _no_layouts "${_out}")
list(LENGTH _other_layouts _others)
list(LENGTH _no_layouts _nones)
set(_refusals 0)
foreach(_case _file IN ZIP_LISTS _cases _files)
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "the client of layout ${_case} wrote nothing:\n${_out}\n${_err}")
    endif()
    file(READ "${_file}" _heard)
    if(NOT _heard MATCHES "refused, reason 8, WRONG_LAYOUT\n")
        message(FATAL_ERROR "the client of layout ${_case} was not refused WRONG_LAYOUT:\n"
                            "${_heard}\n${_out}\n${_err}")
    endif()
    math(EXPR _refusals "${_refusals} + 1")
endforeach()
if(NOT _refusals EQUAL 4 OR NOT _others EQUAL 3 OR NOT _nones EQUAL 1)
    message(FATAL_ERROR "${_refusals} of 4 clients heard WRONG_LAYOUT; the server said "
                        "${_others} of 3 other layouts and ${_nones} of 1 with none:\n"
                        "${_out}\n${_err}")
endif()
if(_out MATCHES "layout is 03281B95")
    message(FATAL_ERROR "a wrong layout's fingerprint was this build's own:\n${_out}")
endif()

string(REGEX MATCHALL "admitted [0-9a-f]+ to slot" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 1 OR NOT _out MATCHES "admitted 11fc7622 to slot 0")
    message(FATAL_ERROR "${_admissions} admitted, not the right client alone:\n${_out}\n${_err}")
endif()
message(STATUS "the clients with the aileron and rudder swapped, one control fewer, one more "
               "and no layout were refused WRONG_LAYOUT, by name; this build's was admitted")

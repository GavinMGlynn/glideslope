# server_goodbye.cmake - a client that says it is leaving is let go at once,
# and a goodbye that is not the session's own lets nobody go.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DWORK=<a scratch directory> -DPORT=<a port> -DCASE=<said|forged>
#         -P server_goodbye.cmake
#
# **The timeout is two minutes**, so that nothing here can pass by it: a
# client let go in this test was let go by its goodbye, or the server says
# "of silence" and this fails. The server runs until everybody who joined has
# gone (`--until-empty`), so a goodbye it ignored costs this test those two
# minutes and then fails it; a goodbye it heard ends it as the client ends.
#
# **What is measured is the server's own clock**: it says, as it lets a
# client go for its goodbye, how long after its admission that was. The
# client stays `_stay` seconds after its handshake, so that is the least it
# can be; the most allowed is `_slack` more, which is a goodbye heard at once
# on any machine and nothing like the timeout.
#
# `said`: one client stays and says goodbye.
#
# `forged`: one client (`--forge-leaving`) makes a second session, and before
# it stays sends three goodbyes that are not the session's own - its own
# goodbye from the second session's address, the second session's goodbye
# from its own address, and its own goodbye from an address with no session,
# which the server refuses (and that refusal is waited for, so all three have
# arrived). Then it stays, and both sessions say goodbye for real. Had any
# forgery let a session go, that session's letting go would come at once, not
# after the stay; and its real goodbye would find nothing to let go.
#
# It needs no network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

set(_store "${WORK}/goodbye.sqlite")
set(_heard "${WORK}/heard.txt")
file(REMOVE "${_store}" "${_heard}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

set(_stay 3)
set(_slack 3)
if(CASE STREQUAL "said")
    set(_flags "")
    set(_sessions 1)
elseif(CASE STREQUAL "forged")
    set(_flags --forge-leaving)
    set(_sessions 2)
else()
    message(FATAL_ERROR "CASE is 'said' or 'forged', not '${CASE}'")
endif()

execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" ${_stay} ${_flags}
            --heard "${_heard}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --ai 0 --headless
            --timeout 120 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server stopped badly:\n${_err}\n${_out}")
endif()

string(REGEX MATCHALL "admitted [0-9a-f]+ to slot" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL _sessions)
    message(FATAL_ERROR "${_admissions} sessions were admitted, not ${_sessions}, so "
                        "the situation was not built:\n${_out}\n${_err}")
endif()

if(_out MATCHES "of silence")
    message(FATAL_ERROR "a client was let go for its silence, after the two minute "
                        "timeout, so its goodbye was not heard:\n${_out}")
endif()

if(CASE STREQUAL "forged")
    if(NOT EXISTS "${_heard}")
        message(FATAL_ERROR "the forging client wrote nothing:\n${_err}")
    endif()
    file(READ "${_heard}" _said)
    set(_forgeries
        "this session's goodbye from another session's address"
        "another session's goodbye from this session's address"
        "this session's goodbye from an address with no session, and it was refused, reason 6")
    set(_sent 0)
    foreach(_forgery IN LISTS _forgeries)
        string(FIND "${_said}" "sent ${_forgery}" _at)
        if(_at LESS 0)
            message(FATAL_ERROR "the client did not send ${_forgery}:\n${_said}")
        endif()
        math(EXPR _sent "${_sent} + 1")
    endforeach()
    list(LENGTH _forgeries _kinds)
    if(NOT _sent EQUAL _kinds OR NOT _kinds EQUAL 3)
        message(FATAL_ERROR "${_sent} of the ${_kinds} forgeries were sent, not all three")
    endif()
endif()

# **Every session let go by its own goodbye, and none sooner than its stay.**
string(REGEX MATCHALL
       "let go [0-9.:]+ after it said it was leaving, [0-9.]+ s after it was admitted"
       _goodbyes "${_out}")
list(LENGTH _goodbyes _count)
if(NOT _count EQUAL _sessions)
    message(FATAL_ERROR "${_count} sessions were let go for a goodbye, not "
                        "${_sessions}:\n${_out}")
endif()
string(REGEX MATCHALL "let go [^\n]+" _all "${_out}")
list(LENGTH _all _lets_go)
if(NOT _lets_go EQUAL _sessions)
    message(FATAL_ERROR "the server let ${_lets_go} sessions go, not ${_sessions}:\n${_out}")
endif()
math(EXPR _most "${_stay} + ${_slack}")
set(_times "")
foreach(_goodbye IN LISTS _goodbyes)
    string(REGEX MATCH "([0-9.]+) s after it was admitted" _ "${_goodbye}")
    set(_after "${CMAKE_MATCH_1}")
    if(_after LESS ${_stay})
        message(FATAL_ERROR "a session was let go ${_after} s after it was admitted, "
                            "before its client's ${_stay} s stay was over - by a "
                            "goodbye that was not its own:\n${_out}")
    endif()
    if(_after GREATER ${_most})
        message(FATAL_ERROR "a session was let go ${_after} s after it was admitted, "
                            "more than ${_slack} s after its ${_stay} s stay:\n${_out}")
    endif()
    list(APPEND _times "${_after}")
endforeach()

if(NOT _out MATCHES "everybody who joined has gone, ([0-9.]+) s in")
    message(FATAL_ERROR "the server did not stop when everybody had gone:\n${_out}")
endif()
message(STATUS "${CASE}: ${_sessions} session(s) let go by goodbye, ${_times} s after "
               "admission for a ${_stay} s stay; the server stopped ${CMAKE_MATCH_1} s "
               "in, with a timeout of 120")

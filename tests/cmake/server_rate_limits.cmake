# server_rate_limits.cmake - a client sending faster than the server's
# stated rates is held to them.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DDATA=<data dir> -DCACHE=<downloads dir> -DWORK=<scratch>
#         -DPORT=<a port> -DBUDGET=<net/budget.hpp> -P server_rate_limits.cmake
#
# **What is pinned** (REQUIREMENTS 6.2, docs/THREATS.md): a session may send
# at most 240 sealed datagrams a second and 8 requests a second, each with a
# second's worth at once (net/budget.hpp, read here so that the rates the
# test holds the server to are the ones it states). Past them, a datagram is
# dropped unread and a request acknowledged and ignored.
#
# **The situation is built**: `glideslope_cli connect --fly --flood`, once the
# server is flying its inputs, sends 1,000 sealed pings at 1,000 a second and
# then 50 requests at once (`WATCH`), knocks once more until the server has
# answered - it has read everything before - and leaves when its requests are
# all acknowledged. The events, not a time.
#
# **What must hold**: some of the client's pings went unanswered, and the
# server said it held the session to its rates, dropping some datagrams and
# taking a second's worth of the requests (one more for what refills while
# they are read). The rates themselves are pinned exactly by a unit test on a
# clock it sets: here the server's reading lags its sending by however long a
# loaded machine takes, and a bound on the client's own time flaked.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
file(READ "${BUDGET}" _budget)
if(NOT _budget MATCHES "session_datagrams_per_second = ([0-9]+)\\.0;")
    message(FATAL_ERROR "net/budget.hpp states no datagram rate")
endif()
set(_datagrams_per_s "${CMAKE_MATCH_1}")
if(NOT _budget MATCHES "session_requests_per_second = ([0-9]+)\\.0;")
    message(FATAL_ERROR "net/budget.hpp states no request rate")
endif()
set(_requests_per_s "${CMAKE_MATCH_1}")

set(_store "${WORK}/rates.sqlite")
set(_ready "${WORK}/ready.txt")
file(REMOVE "${_store}" "${_ready}")
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

# The server is last, so that its words after the client has gone - what it
# says as it lets the session go - are on standard output; the client says
# what came of its flood on standard error.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 280 --fly --flood
            --after-ready "${_ready}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --store "${_store}" --ready-file "${_ready}"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT _err MATCHES "flood: ([0-9]+) sealed pings over ([0-9]+)\\.([0-9][0-9][0-9]) s, ([0-9]+) answered; ([0-9]+) requests, all acknowledged")
    message(FATAL_ERROR "the client did not flood the server:\n${_out}\n${_err}")
endif()
set(_pings "${CMAKE_MATCH_1}")
math(EXPR _took_ms "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
set(_answered "${CMAKE_MATCH_4}")
set(_requests "${CMAKE_MATCH_5}")
# **Only what a slow machine cannot change is held here**: some pings went
# unanswered and the server dropped some. How many depends on how late the
# server read them, which the rates' own unit test
# (a_session_is_held_to_its_stated_rates_on_a_clock_the_test_sets) does not.
if(NOT _answered LESS _pings)
    message(FATAL_ERROR "all ${_answered} of ${_pings} pings sent over ${_took_ms} ms were "
                        "answered, at ${_datagrams_per_s} a second allowed:\n${_out}\n${_err}")
endif()
if(_answered LESS 1)
    message(FATAL_ERROR "none of the pings was answered:\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "was held to its rates: ([0-9]+) of ([0-9]+) sealed datagrams dropped past ([0-9]+) a second, ([0-9]+) of ([0-9]+) requests ignored past ([0-9]+) a second")
    message(FATAL_ERROR "the server did not say it held the client to its rates:\n${_out}")
endif()
set(_dropped "${CMAKE_MATCH_1}")
set(_ignored "${CMAKE_MATCH_4}")
set(_asked "${CMAKE_MATCH_5}")
if(NOT CMAKE_MATCH_3 EQUAL _datagrams_per_s OR NOT CMAKE_MATCH_6 EQUAL _requests_per_s)
    message(FATAL_ERROR "the server held the client to other rates than budget.hpp states "
                        "(${CMAKE_MATCH_3} and ${CMAKE_MATCH_6}):\n${_err}")
endif()
if(_dropped LESS 1)
    message(FATAL_ERROR "the server dropped no datagram:\n${_err}")
endif()
if(NOT _asked EQUAL _requests)
    message(FATAL_ERROR "the server read ${_asked} requests, not the ${_requests} sent:\n${_err}")
endif()
math(EXPR _taken "${_asked} - ${_ignored}")
math(EXPR _most_taken "${_requests_per_s} + 1")
if(_taken GREATER _most_taken OR _taken LESS _requests_per_s)
    message(FATAL_ERROR "the server took ${_taken} of ${_asked} requests sent at once, not "
                        "${_requests_per_s} or ${_most_taken}:\n${_out}")
endif()
list(GET _rcs 1 _server_rc)
list(GET _rcs 0 _client_rc)
if(NOT _server_rc EQUAL 0 OR NOT _client_rc EQUAL 0)
    message(FATAL_ERROR "the server or the client failed (${_rcs}):\n${_out}\n${_err}")
endif()
message(STATUS "held to its rates: ${_answered} of ${_pings} pings answered over ${_took_ms} "
               "ms, ${_taken} of ${_asked} requests taken")

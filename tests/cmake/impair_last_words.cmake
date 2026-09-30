# impair_last_words.cmake - the relay, told to stop when its input ends,
# first passes on everything the server sent before it went.
#
#   cmake -DIMPAIR=<glideslope_impair> -DCHECK=<glideslope_datagram_check>
#         -DWORK=<scratch> -DPORT=<a port> -P impair_last_words.cmake
#
# **What was wrong.** A server that drops its last client sends it a goodbye
# and, with nobody left, stops (--until-empty); its output ends, and the relay
# after it in the pipeline stopped on the first pass that saw that - with the
# goodbye still waiting in its socket, or held for the pass after. The client
# never heard it: the_client_with_the_window_dropped_by_the_operator_says_so_
# and_does_not_join_again failed on CI four times for it (2026-09-30), the
# server saying it had dropped the client and the client flying on to its shot.
#
# **Built, not hoped for.** The server is glideslope_datagram_check --answer
# (PORT), which answers the first datagram that comes and exits at once, its
# output ending with it; the asker is glideslope_datagram_check sending to the
# relay (PORT + 1). With the relay's --delay 1000 the answer is held a second
# before it may go, and the answering server has long gone by then: a relay
# that stops when its input ends, holding it, loses it every time - seen, with
# the relay's old loop put back. With --delay 0, the case the window client's
# test runs, the answer is taken and delivered in the passes after the input
# is seen ended. The asker's minute is a bound, not a wait: the answer comes in
# a second or two, and without it the test fails saying so.

cmake_minimum_required(VERSION 3.28)

math(EXPR _relay "${PORT} + 1")
file(MAKE_DIRECTORY "${WORK}")
set(_ask "${WORK}/ask.hex")
set(_last "${WORK}/last.hex")
# "ask", and "last words".
file(WRITE "${_ask}" "61736b\n")
file(WRITE "${_last}" "6c61737420776f726473\n")

foreach(_delay IN ITEMS 1000 0)
    execute_process(
        COMMAND "${CHECK}" --answer ${PORT} "${_last}" 600
        COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay ${_delay} --jitter 0
                --loss 0 --seed 1 --until-input-ends --seconds 600
        COMMAND "${CHECK}" "127.0.0.1:${_relay}" "${_ask}" 60
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _err MATCHES "answered\n")
        message(FATAL_ERROR "with --delay ${_delay}, the server was never asked "
                            "(${_rcs}):\n${_out}${_err}")
    endif()
    if(NOT _out MATCHES "^answer 6c61737420776f726473\n")
        message(FATAL_ERROR "with --delay ${_delay}, the relay stopped without passing on "
                            "the answer the server sent before it went (${_rcs}):\n"
                            "${_out}${_err}")
    endif()
    if(NOT _rcs STREQUAL "0;0;0")
        message(FATAL_ERROR "with --delay ${_delay}, the programs exited ${_rcs}, not "
                            "0;0;0:\n${_out}${_err}")
    endif()
endforeach()
message(STATUS "the relay passed on the server's last words, held a second and not held")

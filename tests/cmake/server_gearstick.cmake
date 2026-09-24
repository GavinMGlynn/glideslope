# server_gearstick.cmake - a gearstick client is refused by the server.
#
#   cmake -DSERVER=<glideslope_server> -DCHECK=<glideslope_datagram_check>
#         -DHELLO=<gearstick_hello.hex> -DPORT=<a port> -P server_gearstick.cmake
#
# **The first datagram a gearstick client sends**, made from gearstick's own
# docs/TRANSPORT.md by tools/make_gearstick_hello.py - its magic `GSSV`, its
# version, its `HANDSHAKE` type and a real Noise IK message one - put in front
# of a running glideslope server. The two transports are one design with
# different magics, so that each refuses the other at the first four bytes:
# the answer must be a glideslope `REFUSAL` saying `NOT_THIS_PROTOCOL` -
# `47 4c 44 53`, version `01`, type `04`, reason `01` - and nothing else, not a
# handshake answer and not silence.

cmake_minimum_required(VERSION 3.28)

execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 6 --ai 0 --headless
    COMMAND "${CHECK}" "127.0.0.1:${PORT}" "${HELLO}" 5
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "answer ([0-9a-f]+)")
    message(FATAL_ERROR "the server said nothing to a gearstick client:\n${_out}\n${_err}")
endif()
set(_answer "${CMAKE_MATCH_1}")
if(NOT _answer STREQUAL "474c4453010401")
    message(FATAL_ERROR "a gearstick client's first datagram was answered with "
                        "${_answer}, not a REFUSAL saying NOT_THIS_PROTOCOL "
                        "(474c4453010401)")
endif()
message(STATUS "a gearstick client was refused: ${_answer}")

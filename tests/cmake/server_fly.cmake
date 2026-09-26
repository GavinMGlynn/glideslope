# server_fly.cmake - a client's inputs fly its aircraft on the server.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_fly.cmake
#
# **What this pins** is the shape of the whole thing: the client sends inputs
# and never state, the server owns the aircraft and flies it by those inputs,
# and the client learns what happened only from the state updates that come
# back. Nothing the client sends here says where anything is.
#
# **Full left aileron, which no AI pilot on a flight plan would ever hold.**
# So an aircraft that rolls inverted is one being flown from the client and
# cannot be anything else: a server that ignored the inputs would leave it
# straight and level, and a server that applied them and then forgot them
# between steps would roll it a few degrees and stop. Both of those have
# happened here - the second was a real bug, and it gave 34 degrees in twelve
# seconds where holding the stick over gives 180 in six. The bar is 90: past
# it the aeroplane is inverted, which neither of those can reach.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

set(_store "${WORK}/fly.sqlite")
file(REMOVE "${_store}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# The terrain first, alone, so the client is not waiting on a download.
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

execute_process(
    # Until the client has gone, which is when the server has applied its
    # last input: a fixed eight seconds stopped a debug server on CI with a
    # third of them still unread.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --store "${_store}"
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 6 --fly
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client did not finish well:\n${_err}\n${_out}")
endif()

# **The server took the inputs.** Nearly all of them: the last few are still
# in flight when the client stops, and one frame is always outstanding.
if(NOT _out MATCHES "sent ([0-9]+) input frames, the server applied ([0-9]+)")
    message(FATAL_ERROR "the client said nothing about its inputs:\n${_out}")
endif()
set(_sent ${CMAKE_MATCH_1})
set(_applied ${CMAKE_MATCH_2})
# Thirty a second is 180, but the floor is what fails a client that sends
# once a second - six - not the runner's speed: a debug client on a Windows
# runner shared with three client renders sent 97.
if(_sent LESS 20)
    message(FATAL_ERROR "the client sent ${_sent} input frames in six seconds, "
                        "and it sends thirty a second")
endif()
if(_applied EQUAL 0)
    message(FATAL_ERROR "the server applied none of the ${_sent} input frames "
                        "the client sent: a client cannot fly")
endif()
# **Every one.** The client, its six seconds up, waits for the server to have
# applied the last input it sent (glideslope_cli's `stay`), so nothing is in
# flight when it says what was applied: a count short of what was sent is a
# server that did not get there. It used to allow thirty short, for the frames
# a slow server had not yet read, and a debug server on CI fell sixty behind.
if(NOT _applied EQUAL _sent)
    message(FATAL_ERROR "the server applied ${_applied} of ${_sent} input frames:\n${_out}")
endif()

# **And the aeroplane did what it was told.** Past 90 degrees it is inverted.
if(NOT _out MATCHES "my aircraft rolled to ([-0-9]+) degrees")
    message(FATAL_ERROR "the client never found its own aircraft in a state "
                        "update, so it does not know which one is its own:\n${_out}")
endif()
set(_roll ${CMAKE_MATCH_1})
if(_roll LESS 0)
    math(EXPR _roll "0 - ${_roll}")
endif()
if(_roll LESS 90)
    message(FATAL_ERROR
            "the client held full aileron for six seconds and its aircraft "
            "reached ${_roll} degrees of bank. Holding the stick over rolls it "
            "inverted; a server that dropped the inputs leaves it level, and "
            "one that forgot them between steps reaches a few tens of degrees")
endif()

message(STATUS "the client sent ${_sent} input frames, the server applied "
               "${_applied} and rolled the aeroplane to ${_roll} degrees of bank")

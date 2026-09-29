# server_drop_every_session.cmake - a player the operator drops is let go from
# every address their key has a session at, at once.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_drop_every_session.cmake
#
# **Why.** One key may have sessions at two addresses for a while: a client
# started again from a new port, or a replayed copy of its initiation. A drop
# that let go only the session on the dashboard's row would leave the key a
# session - its slot held, and its aircraft flying on - until that one timed
# out.
#
# **The situation is built, not hoped for.** The client
# (`--again-from-elsewhere`) has its copy's session answered before it flies,
# so the key has two sessions when the server drops the first player whose
# input it has flown (`--drop-once-flown`). The server's --timeout is a minute,
# so a session left behind would still be there long after the drop.
#
# **What must hold**: the drop said once; both sessions let go at once, and
# none for silence; the client told its session was ended; one player's
# aircraft.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/drop_every.sqlite")
set(_ready "${WORK}/ready.txt")
set(_heard "${WORK}/heard.txt")
set(_gone "${WORK}/copy_gone.txt")
file(REMOVE "${_store}" "${_ready}" "${_heard}" "${_gone}")

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

execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 120 --fly
            --again-from-elsewhere "${_gone}" --after-ready "${_ready}" --heard "${_heard}"
            --key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 60 --store "${_store}"
            --ready-file "${_ready}" --drop-once-flown
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

string(REGEX MATCHALL "admitted 11fc7622" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 2)
    message(FATAL_ERROR "the key was admitted ${_admissions} times, not twice - once "
                        "for the client and once for the copy:\n${_out}\n${_err}")
endif()
string(REGEX MATCHALL "dropped 127\\.0\\.0\\.1:[0-9]+ by the operator" _drops "${_out}")
list(LENGTH _drops _dropped)
if(NOT _dropped EQUAL 1)
    message(FATAL_ERROR "the server dropped ${_dropped} times, not once:\n${_out}")
endif()
string(REGEX MATCHALL
       "let go 127\\.0\\.0\\.1:[0-9]+(: its key was dropped|, unproven, with its key's last proven session)"
       _gone_lines "${_out}")
list(LENGTH _gone_lines _let_go)
if(NOT _let_go EQUAL 2)
    message(FATAL_ERROR "the drop let ${_let_go} of the key's sessions go, not both:\n"
                        "${_out}\n${_err}")
endif()
if(_out MATCHES "s of silence")
    message(FATAL_ERROR "a session of the dropped key outlived the drop and was let go "
                        "for silence:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _said MATCHES "the server ended this session")
    message(FATAL_ERROR "the client was not told its session was ended:\n${_said}\n${_out}")
endif()
string(REGEX MATCHALL "number [0-9]+, a player's, banked as far as [0-9]+ degrees"
       _players "${_out}")
list(LENGTH _players _count)
if(NOT _count EQUAL 1)
    message(FATAL_ERROR "the server flew ${_count} players' aircraft for one key, not "
                        "one:\n${_out}\n${_err}")
endif()
message(STATUS "a drop let go both of the key's sessions at once")

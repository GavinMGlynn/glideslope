# server_drop_keeps_out.cmake - a player the operator drops is told so, does
# not come back by itself, and is refused DROPPED when started again.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_drop_keeps_out.cmake
#
# **Why.** A client the server has let go joins again by itself
# (server_joins_again.cmake). Without more, the operator's drop button would
# be a kick that lasted three seconds: the dropped client would find its
# session gone and join again.
#
# **The situation is built.** The server drops the first player whose input
# it has flown (`--drop-once-flown` - the path the window's button takes).
# The dropped client flies, and must hear the server's sealed goodbye, say
# so, and leave without joining again. Then the same key is started again,
# once the first has gone (`--after-ready` on its `--done` file), and must be
# refused DROPPED (07). A third client, which does not fly, stays until the
# second has gone, which keeps the server (--until-empty) running - so it
# must be in before the first is dropped: the first joins only once the third
# has heard the server introduce an aircraft (`--after-ready` on the third's
# `--heard` file). Started together, the first was admitted and dropped
# before the third arrived on macOS CI (run 36289235846), the server found
# everybody gone and stopped, and the second waited a minute for no answer.
#
# **What must hold**: the dropped key admitted once and never again; the
# server saying it dropped it and refused it; the dropped client saying it was
# dropped and not joining again; the restarted one refused, reason 7.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/drop.sqlite")
set(_ready "${WORK}/ready.txt")
set(_first "${WORK}/first.txt")
set(_first_done "${WORK}/first_done.txt")
set(_again "${WORK}/again.txt")
set(_again_done "${WORK}/again_done.txt")
set(_staying "${WORK}/staying.txt")
file(REMOVE "${_store}" "${_ready}" "${_first}" "${_first_done}" "${_again}" "${_again_done}"
     "${_staying}")

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

# The dropped key's public half begins 11fc7622 (see server_slots.cmake); the
# staying client's, f661fe1f.
set(_dropped_key 9a47cf83f2e50ebb1bb176f4072fa4ad962b89d8cf09527d1ce6abd308f89ba2)
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300 --fly
            --after-ready "${_staying}" --heard "${_first}" --done "${_first_done}"
            --key ${_dropped_key}
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_first_done}" --heard "${_again}" --done "${_again_done}"
            --key ${_dropped_key}
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 300
            --after-ready "${_ready}" --until-exists "${_again_done}" --heard "${_staying}"
            --key e94098d673c95d5361083f2de65d653ab59f17b3141ebca6ee8e6fa488291f26
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --players 4 --data "${DATA}" --timeout 3 --store "${_store}"
            --ready-file "${_ready}" --drop-once-flown
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

foreach(_file IN ITEMS "${_first}" "${_again}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "a client wrote nothing to ${_file}:\n${_out}\n${_err}")
    endif()
endforeach()
file(READ "${_first}" _first_heard)
file(READ "${_again}" _again_heard)

if(NOT _out MATCHES "dropped 127\\.0\\.0\\.1:[0-9]+ by the operator")
    message(FATAL_ERROR "the server dropped nobody:\n${_out}")
endif()
if(NOT _first_heard MATCHES "dropped by the server's operator; not joining again")
    message(FATAL_ERROR "the dropped client did not hear it was dropped:\n"
                        "${_first_heard}\n${_out}\n${_err}")
endif()
if(NOT _again_heard MATCHES "refused, reason 7")
    message(FATAL_ERROR "the dropped key started again was not refused DROPPED:\n"
                        "${_again_heard}\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "refused 11fc7622 from 127\\.0\\.0\\.1:[0-9]+: dropped by the operator")
    message(FATAL_ERROR "the server did not say it refused the dropped key:\n${_out}")
endif()
string(REGEX MATCHALL "admitted 11fc7622" _admitted "${_out}")
list(LENGTH _admitted _admissions)
if(NOT _admissions EQUAL 1)
    message(FATAL_ERROR "the dropped key was admitted ${_admissions} times, not once:\n"
                        "${_out}")
endif()
message(STATUS "a dropped player was told, did not come back, and was refused DROPPED "
               "when started again")

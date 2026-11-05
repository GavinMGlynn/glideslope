# server_wreck_flown_again_layer.cmake - a wreck flown again while a player
# is at the start on its layer is kept 500 ft from her.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_wreck_flown_again_layer.cmake
#
# **Built, not hoped for.** A player's join skips a wreck when it looks for a
# free layer; a wreck flies again from its own start. A server with one AI
# aircraft (with none it never steps) and room for two players, whose
# aircraft is taken out of the sky when its player goes (`--on-leave
# remove`, the default). Three clients:
# - A joins at once, on layer 2 (one over the AI and A), and leaves 3 s
#   later;
# - C joins 1 s in, on layer 3, sends inputs (`--fly`), and dives into the
#   ground 4 s after that (`--dive-after`); it writes a file the moment it
#   hears the wreck (`--wreck-file`), and leaves once it hears its aircraft
#   fly again;
# - B joins when that file appears, so during C's five seconds as a wreck,
#   not on a clock: the AI and C (a wreck) make layer 3 the first B may
#   take, and a wreck holds no layer, so B starts on C's layer.
# C then flies again near the start, where B still is, five seconds in.
# **Checked**: the situation was built - C a wreck, then B started, then C
# flew again, in that order, with B on C's layer - and C flies again at least
# 500 ft from B's height (they are within 1.5 nm of each other at the start,
# so height is what keeps them apart). Before the fix it flew again on B's
# layer: 0 ft from her.
#
# Without the DEM's tiles it reports itself skipped (exit 77).

cmake_minimum_required(VERSION 3.28)

include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/wreck-layer.sqlite")
set(_wrecked "${WORK}/wrecked.txt")
file(REMOVE "${_store}" "${_wrecked}")

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
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 3
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 120 --after 1
            --fly --dive-after 4 --wreck-file "${_wrecked}" --until-flying-again 1
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${_key}" 15
            --after-ready "${_wrecked}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --players 2 --headless
            --data "${DATA}" --store "${_store}" --ai 1
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")

# **Three joins**, B's the third.
string(REGEX MATCHALL "a player's aircraft starts on layer [0-9]+" _starts "${_out}")
list(LENGTH _starts _count)
if(NOT _count EQUAL 3)
    message(FATAL_ERROR "${_count} players' starts were said, not the three joins:\n${_out}")
endif()
list(GET _starts 1 _c_start)
string(REGEX REPLACE ".* layer " "" _c_layer "${_c_start}")
list(GET _starts 2 _b_start)
string(REGEX REPLACE ".* layer " "" _b_layer "${_b_start}")

# **C a wreck, once.**
string(REGEX MATCHALL "aircraft [0-9]+, [^,\n]+, is a wreck" _wrecks "${_out}")
list(LENGTH _wrecks _wreck_count)
if(NOT _wreck_count EQUAL 1)
    message(FATAL_ERROR "${_wreck_count} wrecks, not C's one:\n${_out}")
endif()
if(NOT _wrecks MATCHES "^aircraft ([0-9]+), ")
    message(FATAL_ERROR "no wreck's number in '${_wrecks}'")
endif()
set(_c "${CMAKE_MATCH_1}")

# **C flown again**, and how high.
if(NOT _out MATCHES "aircraft ${_c}, [^,\n]+, flies again (-?[0-9]+) ft over the plan's start")
    message(FATAL_ERROR "aircraft ${_c} never said it flew again near the start:\n${_out}")
endif()
set(_c_ft "${CMAKE_MATCH_1}")
set(_c_again "${CMAKE_MATCH_0}")

# **The situation built**: wreck, then B's join, then C flown again - and B
# on the layer C started on.
string(FIND "${_out}" "${_wrecks}" _at_wreck)
string(FIND "${_out}" "${_b_start}" _at_b REVERSE)
string(FIND "${_out}" "${_c_again}" _at_again)
if(NOT (_at_wreck LESS _at_b AND _at_b LESS _at_again))
    message(FATAL_ERROR "B did not join while C was a wreck (wreck at ${_at_wreck}, B's start at "
                        "${_at_b}, C flown again at ${_at_again}) - the case is not built:\n${_out}")
endif()
if(NOT _b_layer EQUAL _c_layer)
    message(FATAL_ERROR "B started on layer ${_b_layer}, not C's layer ${_c_layer} - the case "
                        "is not built:\n${_out}")
endif()

# **Kept 500 ft from her**: both at the start, so by height.
math(EXPR _b_ft "${_b_layer} * 1000")
math(EXPR _apart "${_c_ft} - ${_b_ft}")
if(_apart LESS 0)
    math(EXPR _apart "0 - ${_apart}")
endif()
message(STATUS "B joined on layer ${_b_layer} (${_b_ft} ft) while C was a wreck; C flew again "
               "at ${_c_ft} ft, ${_apart} ft from her")
if(_apart LESS 500)
    message(FATAL_ERROR "C flew again ${_apart} ft from B, at the start: within 500 ft and "
                        "1.5 nm of her:\n${_out}")
endif()
if(NOT _rcs STREQUAL "0;0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
message(STATUS "a wreck flown again while a player was at the start on its layer was kept "
               "${_apart} ft from her")

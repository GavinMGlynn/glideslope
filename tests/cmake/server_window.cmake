# server_window.cmake - the server's window shows the dashboard the terminal
# would, and its drop button drops a player.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_window.cmake
#
# **What the item asks of the window**: the same facts as the terminal, and a
# drop button that drops that player. A client joins and flies; the server,
# with --window, is told to press `drop 0` the first time it draws it
# (--window-press - the same path a click takes), to print what its window
# drew next to what the terminal would have printed for the same moment
# (--window-dump), and to keep its last frame (--window-shot).
#
# Then: the player was dropped, by the operator; the window's last frame shows
# slot 0 open again and the drop in its log; every line it drew is the
# terminal's line, in order, and it drew all of them; and the frame is the
# window's size and background. It needs a display, so where there is none it
# reports itself skipped (exit 77) unless GLIDESLOPE_REQUIRE_WINDOW is set, as
# CI sets it; and it needs the DEM's tiles, so without the network the same.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/bmp.cmake")
# For glideslope_judge_leaks: a window loads the display's own libraries, and
# what they leak is theirs; a leak with a frame in glideslope still fails.
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND "$ENV{DISPLAY}" STREQUAL ""
   AND "$ENV{WAYLAND_DISPLAY}" STREQUAL "")
    if(NOT "$ENV{GLIDESLOPE_REQUIRE_WINDOW}" STREQUAL "")
        message(FATAL_ERROR "no DISPLAY or WAYLAND_DISPLAY, and GLIDESLOPE_REQUIRE_WINDOW is set")
    endif()
    message(STATUS "no DISPLAY or WAYLAND_DISPLAY; a window cannot open here")
    cmake_language(EXIT 77)
endif()

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/window.sqlite")
set(_shot "${WORK}/window.bmp")
file(REMOVE "${_store}" "${_shot}")

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

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 5 --fly --after 1
    # Until the client has gone - dropped, here - rather than for a fixed
    # time, which on a slow runner ended before the client had joined.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --data "${DATA}"
            --store "${_store}" --window --window-dump
            --window-press "drop 0" --window-shot "${_shot}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server with a window did not run well (exit ${_rc}):\n${_err}\n${_out}")
endif()

glideslope_judge_leaks("${_err}")

# **The drop button dropped the player.**
if(NOT _out MATCHES "dropped 127\\.0\\.0\\.1:[0-9]+ by the operator")
    message(FATAL_ERROR "the drop button dropped nobody:\n${_out}")
endif()

# **The window drew the terminal's dashboard, line for line.**
string(REPLACE "\r" "" _out "${_out}")
string(REPLACE ";" "," _flat "${_out}")
string(REPLACE "\n" ";" _lines "${_flat}")
set(_window "")
set(_terminal "")
foreach(_line IN LISTS _lines)
    if(_line MATCHES "^window: (.*)$")
        # Kept before the next MATCHES, which sets CMAKE_MATCH_1 afresh.
        set(_drawn "${CMAKE_MATCH_1}")
        if(NOT _drawn MATCHES "^\\[")
            list(APPEND _window "=${_drawn}")
        endif()
    elseif(_line MATCHES "^terminal: (.*)$")
        list(APPEND _terminal "=${CMAKE_MATCH_1}")
    endif()
endforeach()
list(LENGTH _terminal _n)
if(_n LESS 10)
    message(FATAL_ERROR "the terminal's dashboard came to ${_n} lines:\n${_out}")
endif()
if(NOT _window STREQUAL _terminal)
    message(FATAL_ERROR "the window drew something other than the terminal's dashboard.\n"
                        "window:\n${_window}\nterminal:\n${_terminal}")
endif()

# **And what it drew last is the drop, done.**
if(NOT _out MATCHES "window:   0     \\(open\\)")
    message(FATAL_ERROR "slot 0 is not open again after the drop:\n${_out}")
endif()
if(NOT _out MATCHES "window: [^\n]*admitted [0-9a-f]+ to slot 0" OR
   NOT _out MATCHES "window: [^\n]*dropped 127\\.0\\.0\\.1:[0-9]+ by the operator")
    message(FATAL_ERROR "the window's log does not show the player arriving and "
                        "being dropped:\n${_out}")
endif()

# **The frame**: the window's size, and its background in the corner.
# The size is the one the server says it drew: 1120 by 720 unless the screen
# is smaller, as macOS's on CI is, where the window is shrunk to fit.
if(NOT _out MATCHES "wrote [^\n]*window\\.bmp, ([0-9]+) by ([0-9]+)")
    message(FATAL_ERROR "the server did not say it wrote the window's frame:\n${_out}")
endif()
set(_w ${CMAKE_MATCH_1})
set(_h ${CMAKE_MATCH_2})
if(_w LESS 400 OR _h LESS 300 OR _w GREATER 1120 OR _h GREATER 720)
    message(FATAL_ERROR "the window's frame is ${_w} by ${_h}, which is not a window "
                        "of up to 1120 by 720 that can be read")
endif()
bmp_load("${_shot}")
if(NOT BMP_WIDTH EQUAL _w OR NOT BMP_HEIGHT EQUAL _h)
    message(FATAL_ERROR "the frame written is ${BMP_WIDTH} by ${BMP_HEIGHT}, and the "
                        "server said ${_w} by ${_h}")
endif()
math(EXPR _right "${_w} - 1")
math(EXPR _bottom "${_h} - 1")
bmp_pixel(_corner ${_right} ${_bottom})
if(NOT _corner MATCHES "^18;22;30;")
    message(FATAL_ERROR "the window's corner is ${_corner}, not its background 18;22;30")
endif()
message(STATUS "the window drew the terminal's ${_n} lines, dropped the player, "
               "and wrote a ${BMP_WIDTH} by ${BMP_HEIGHT} frame")

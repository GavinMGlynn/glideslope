# server_no_display.cmake - a server with no display runs, and refuses --window.
#
#   cmake -DSERVER=<glideslope_server> -P server_no_display.cmake
#
# **Where the server is meant to live** - a host in the cloud, EC2 or Fargate -
# there is no display. So with DISPLAY, WAYLAND_DISPLAY and XDG_RUNTIME_DIR taken
# away - without the last, a Wayland client finds a desktop's compositor at its
# default socket anyway, as WSLg's is - it must
# run as it always has, and asked for --window it must refuse, saying why,
# rather than run without the window it was asked for. (That no display
# library is linked at all is the_server_links_no_display_library.)

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY --unset=XDG_RUNTIME_DIR
            "${SERVER}" --port 0 --ai 0 --seconds 1 --headless
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR NOT _out MATCHES "server key [0-9a-f]+")
    message(FATAL_ERROR "with no display the server did not run (exit ${_rc}):\n${_out}\n${_err}")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY --unset=XDG_RUNTIME_DIR
            "${SERVER}" --port 0 --ai 0 --seconds 1
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "with no display the terminal's dashboard did not run (exit ${_rc}):\n${_err}")
endif()

set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY --unset=XDG_RUNTIME_DIR
            "${SERVER}" --port 0 --ai 0 --seconds 1 --window
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
glideslope_judge_leaks("${_err}")
if(_rc EQUAL 0)
    message(FATAL_ERROR "with no display, --window ran anyway:\n${_out}")
endif()
if(NOT _err MATCHES "--window needs a display")
    message(FATAL_ERROR "with no display, --window failed without saying why:\n${_err}")
endif()
message(STATUS "with no display the server runs, and refuses --window: ${_err}")

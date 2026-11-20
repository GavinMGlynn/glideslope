# this_machines_tool_test.cmake - the fallback the compiling tests lean on
# (this_machines_tool.cmake), in each of its four cases.
#
#   cmake -DTOOL=<a program on this machine, by full path> -P this_machines_tool_test.cmake
#
# TOOL stands for the build's compiler: CMake itself, which is here wherever
# the test runs, and on the PATH ctest runs it with.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/this_machines_tool.cmake")

set(_failures "")
set(_walked 0)
set(_expected 4)
get_filename_component(_name "${TOOL}" NAME)

# 1. The build's path is here: left as it is.
set(T "${TOOL}")
this_machines_tool(T)
math(EXPR _walked "${_walked} + 1")
if(NOT T STREQUAL "${TOOL}")
    string(APPEND _failures "\n  a path that is here became ${T}")
endif()

# 2. A bare name: left for the PATH to find.
set(T "${_name}")
this_machines_tool(T)
math(EXPR _walked "${_walked} + 1")
if(NOT T STREQUAL "${_name}")
    string(APPEND _failures "\n  a bare name became ${T}")
endif()

# 3. The build's path is not here, its name is on the PATH: that one.
set(T "/no/such/toolset/14.51.36231/bin/${_name}")
this_machines_tool(T)
math(EXPR _walked "${_walked} + 1")
if(NOT IS_ABSOLUTE "${T}" OR NOT EXISTS "${T}")
    string(APPEND _failures "\n  a path not here, its name on the PATH, became ${T}")
endif()

# 4. Neither: the script fails, naming both - run as its own process.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/this_machines_tool_missing.cmake"
     "include(\"${CMAKE_CURRENT_LIST_DIR}/this_machines_tool.cmake\")\n"
     "set(T \"/no/such/toolset/bin/glideslope-no-such-tool-anywhere\")\n"
     "this_machines_tool(T)\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -P
                        "${CMAKE_CURRENT_BINARY_DIR}/this_machines_tool_missing.cmake"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
math(EXPR _walked "${_walked} + 1")
string(REGEX REPLACE "[ \t\r\n]+" " " _said "${_out}${_err}")  # CMake wraps its errors
if(_rc EQUAL 0)
    string(APPEND _failures "\n  a tool in neither place was accepted")
elseif(NOT _said MATCHES "is not on this machine, nor glideslope-no-such-tool-anywhere on its PATH")
    string(APPEND _failures "\n  a tool in neither place failed for another reason:\n${_out}${_err}")
endif()

message(STATUS "walked ${_walked} of ${_expected} cases")
if(NOT _walked EQUAL _expected)
    string(APPEND _failures "\n  walked ${_walked} cases, expected ${_expected}")
endif()
if(_failures)
    message(FATAL_ERROR "the build's tool or this machine's:${_failures}")
endif()

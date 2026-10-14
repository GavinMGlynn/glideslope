# ci_python_pinned.cmake - the Python a CI configure found is the one the job
# pinned, so the test jobs that run its tree on another runner image find it.
#
#   cmake -DBUILD=<build dir> -DROOT=<the pinned Python's directory> -P tools/ci_python_pinned.cmake
#
# **Why.** The configure writes the Python it found into the tests' command
# lines, and a test job runs that tree on whatever runner image it gets.
# Left to itself, FindPython3 took the newest Python registered on the
# Windows image - C:/hostedtoolcache/windows/Python/3.14.7 - and the next
# image had 3.14.8 instead: every script check there failed "exited no such
# file or directory" (2026-10-14, run 38624857649). The Windows jobs pin
# Python with actions/setup-python, which sets Python3_ROOT_DIR; this fails
# the build unless the configure found that one, so a run that passes has
# shown it did, whichever images its jobs landed on.
cmake_minimum_required(VERSION 3.28)

if(NOT ROOT)
    message(FATAL_ERROR "no pinned Python: Python3_ROOT_DIR is not set")
endif()
if(NOT EXISTS "${BUILD}/CMakeCache.txt")
    message(FATAL_ERROR "no configure in ${BUILD}")
endif()
file(STRINGS "${BUILD}/CMakeCache.txt" _lines REGEX "^_?Python3_EXECUTABLE:")
if(NOT _lines)
    message(FATAL_ERROR "the configure in ${BUILD} found no Python")
endif()
list(GET _lines 0 _line)
string(REGEX REPLACE "^[^=]*=" "" _found "${_line}")
string(REPLACE "\\" "/" _found "${_found}")
string(REPLACE "\\" "/" _root "${ROOT}")
string(TOLOWER "${_found}" _found_l)
string(TOLOWER "${_root}/" _root_l)
string(FIND "${_found_l}" "${_root_l}" _at)
if(NOT _at EQUAL 0)
    message(FATAL_ERROR "the configure found ${_found}, not the pinned Python in ${_root}")
endif()
message(STATUS "the configure found the pinned Python: ${_found}")

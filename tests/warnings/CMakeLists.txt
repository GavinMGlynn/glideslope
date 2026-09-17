# A tiny project that includes the real cmake/CompilerWarnings.cmake, so that
# tests/cmake/warnings_are_errors.cmake can build deliberate mistakes with the
# project's own warning set and watch them fail.
cmake_minimum_required(VERSION 3.28)
project(glideslope_warnings_probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${GLIDESLOPE_ROOT}/cmake")
include(CompilerWarnings)

# Targets are prefixed because `clean` is a name CMake reserves.
foreach(_probe clean narrowing sign_conversion)
    add_executable(probe_${_probe} ${_probe}.cpp)
    glideslope_warnings(probe_${_probe})
endforeach()

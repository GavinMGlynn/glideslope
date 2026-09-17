cmake_minimum_required(VERSION 3.28)

project(glideslope
    VERSION     0.1.0
    DESCRIPTION "Glideslope - a multiplayer flight simulator in C++"
    LANGUAGES   CXX)

# ---------------------------------------------------------------------------
# **The C++ runtime goes inside the executable on Windows.**
#
# MSVC links the runtime dynamically by default, so the program needs
# VCRUNTIME140.dll and MSVCP140.dll from the Visual C++ Redistributable. Every
# machine with Visual Studio or a CI runner has them, so nothing notices - until
# a zip is unpacked on a machine without them and the program does not start. A
# zip cannot install a redistributable. gearstick learned this; the package
# workflow checks the packaged executable depends on neither DLL.
#
# Set before any target exists, so everything built here - and every dependency
# added under ext/ later - agrees about the runtime.
# ---------------------------------------------------------------------------
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 64-bit only, and a compiler at or above its floor - see cmake/Platform.cmake.
include(Platform)
glideslope_check_platform()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(GLIDESLOPE_TESTS "Build the tests"                                       ON)
option(GLIDESLOPE_ASAN  "Build with address and undefined-behaviour sanitizers" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# **C++20 because Cesium Native requires it**, and a project that links it
# cannot build its own code to an older standard than the headers it includes.
# C++23 is not taken: nothing here needs it yet, and AppleClang and MSVC cover
# it less evenly than they cover 20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CompilerWarnings)
include(Sanitizers)
include(Layering)

# src/sim/ includes no presentation - see cmake/Layering.cmake.
glideslope_check_layering("${CMAKE_CURRENT_SOURCE_DIR}")

# Everything a first-party target gets. Vendored code in ext/ never passes
# through here.
function(glideslope_configure target)
    glideslope_warnings(${target})
    glideslope_sanitize(${target})
endfunction()

# ---------------------------------------------------------------------------
# The simulation
#
# **Links no presentation.** No window, renderer, input or audio, so the
# headless CLI and the server can both link it. Today it knows only its own
# version; JSBSim arrives in Phase 1.
# ---------------------------------------------------------------------------
add_library(glideslope_sim STATIC
    src/sim/version.cpp)

# One include root, so every #include names the layer it crosses:
#   #include "sim/version.hpp"   not   #include "../sim/version.hpp"
target_include_directories(glideslope_sim PUBLIC src)
target_compile_definitions(glideslope_sim PRIVATE
    GLIDESLOPE_VERSION="${PROJECT_VERSION}")
glideslope_configure(glideslope_sim)

# ...and links none, checked once every target_link_libraries below has run.
cmake_language(DEFER CALL glideslope_check_sim_links glideslope_sim)

# ---------------------------------------------------------------------------
# glideslope_cli - the simulation with no window.
# ---------------------------------------------------------------------------
add_executable(glideslope_cli
    src/frontend/cli/main.cpp)
target_link_libraries(glideslope_cli PRIVATE glideslope_sim)
glideslope_configure(glideslope_cli)

# ---------------------------------------------------------------------------
# Packaging
#
# **A package is a folder somebody unpacks and runs in place**: a tarball on
# Linux and macOS, a zip on Windows, each holding one top-level folder named for
# the version and platform, with the programs, the licence and the README side
# by side in it. `cpack --preset <platform>-release` makes one, and
# .github/workflows/package.yml unpacks it somewhere else and runs it.
# ---------------------------------------------------------------------------
install(TARGETS glideslope_cli RUNTIME DESTINATION .)
install(FILES LICENSE README.md DESTINATION .)

set(CPACK_PACKAGE_NAME              "glideslope")
set(CPACK_PACKAGE_VENDOR            "Gavin Glynn")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME         "glideslope-${PROJECT_VERSION}-${GLIDESLOPE_PLATFORM}")
set(CPACK_PACKAGE_CHECKSUM          SHA256)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(GLIDESLOPE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

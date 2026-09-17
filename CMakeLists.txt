cmake_minimum_required(VERSION 3.28)

project(glideslope
    VERSION     0.1.0
    DESCRIPTION "Glideslope - a multiplayer flight simulator in C++"
    LANGUAGES   CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

include(Sanitizers)

# Everything a first-party target gets. Vendored code in ext/ never passes
# through here.
function(glideslope_configure target)
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

# ---------------------------------------------------------------------------
# glideslope_cli - the simulation with no window.
# ---------------------------------------------------------------------------
add_executable(glideslope_cli
    src/frontend/cli/main.cpp)
target_link_libraries(glideslope_cli PRIVATE glideslope_sim)
glideslope_configure(glideslope_cli)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(GLIDESLOPE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

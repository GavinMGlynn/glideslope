cmake_minimum_required(VERSION 3.28)

project(glideslope
    VERSION     0.1.0
    DESCRIPTION "Glideslope - a multiplayer flight simulator in C++"
    # C++ is the project. C is here for JSBSim's bundled copy of expat, the XML
    # parser its model files are read with; nothing of ours is written in C.
    LANGUAGES   C CXX)

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

# JSBSim, from ext/jsbsim - see cmake/Jsbsim.cmake.
include(Jsbsim)

# ---------------------------------------------------------------------------
# The data the programs read at run time: assets/, copied to data/ beside them -
# in the build tree, where the tests run them, and in a package - so a program
# finds its data the same way wherever it is. assets/jsbsim holds the flight
# models (made by tools/make_c172p.py; see assets/jsbsim/README.md) and
# assets/figures the published figures they are checked against.
#
# Copied at configure time. Any file added under assets/ reconfigures
# (CONFIGURE_DEPENDS), and so does any change to a file already there - which
# CONFIGURE_DEPENDS alone does not notice, and which is the change that matters
# when a model is being tuned.
# ---------------------------------------------------------------------------
set(GLIDESLOPE_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/data")
file(GLOB_RECURSE _assets CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_assets})
file(REMOVE_RECURSE "${GLIDESLOPE_DATA_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION "${GLIDESLOPE_DATA_DIR}")

# ---------------------------------------------------------------------------
# The simulation
#
# **Links no presentation.** No window, renderer, input or audio, so the
# headless CLI and the server can both link it. It links JSBSim, which is a
# flight model and nothing else.
# ---------------------------------------------------------------------------
add_library(glideslope_sim STATIC
    src/sim/aircraft.cpp
    src/sim/figures.cpp
    src/sim/fixed_step.cpp
    src/sim/version.cpp)

# One include root, so every #include names the layer it crosses:
#   #include "sim/version.hpp"   not   #include "../sim/version.hpp"
target_include_directories(glideslope_sim PUBLIC src)
target_compile_definitions(glideslope_sim PRIVATE
    GLIDESLOPE_VERSION="${PROJECT_VERSION}")
target_link_libraries(glideslope_sim PUBLIC libJSBSim)
glideslope_configure(glideslope_sim)

# ...and links none, checked once every target_link_libraries below has run.
cmake_language(DEFER CALL glideslope_check_sim_links glideslope_sim)

# ---------------------------------------------------------------------------
# The platform layer - where things are on this machine. Paths only, today;
# input and sockets arrive with the phases that need them.
# ---------------------------------------------------------------------------
add_library(glideslope_platform STATIC
    src/platform/paths.cpp)
target_include_directories(glideslope_platform PUBLIC src)
glideslope_configure(glideslope_platform)

# ---------------------------------------------------------------------------
# glideslope_cli - the simulation with no window.
# ---------------------------------------------------------------------------
add_executable(glideslope_cli
    src/frontend/cli/main.cpp)
target_link_libraries(glideslope_cli PRIVATE glideslope_sim glideslope_platform)
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
# Everything of ours is installed as the `glideslope` component, and CPack is
# told to package that component alone. JSBSim's src/ carries install rules for
# its own library, headers and program, which do not belong in a download.
install(TARGETS glideslope_cli RUNTIME DESTINATION . COMPONENT glideslope)
install(FILES LICENSE README.md DESTINATION . COMPONENT glideslope)
install(DIRECTORY "${GLIDESLOPE_DATA_DIR}/" DESTINATION data COMPONENT glideslope)
foreach(_licence IN LISTS GLIDESLOPE_JSBSIM_LICENCES)
    string(REPLACE "=" ";" _pair "${_licence}")
    list(GET _pair 0 _from)
    list(GET _pair 1 _as)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ext/jsbsim/${_from}"
            DESTINATION licenses RENAME "${_as}" COMPONENT glideslope)
endforeach()
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_CURRENT_BINARY_DIR};glideslope;glideslope;/")

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

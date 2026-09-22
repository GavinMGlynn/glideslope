cmake_minimum_required(VERSION 3.28)

# vcpkg, for Cesium Native's dependencies: its toolchain before project(), and
# the packages installed after the platform gate - see cmake/Vcpkg.cmake.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Vcpkg.cmake")

project(glideslope
    VERSION     0.1.0
    DESCRIPTION "Glideslope - a multiplayer flight simulator in C++"
    # C++ is the project. C is here for JSBSim's bundled copy of expat, the XML
    # parser its model files are read with; nothing of ours is written in C.
    LANGUAGES   C CXX)

# Objective-C++ on macOS, for NSURLSession (src/platform/http_nsurlsession.mm).
# Enabled here, before any target: CMake needs a language enabled at the top
# level before the first target that compiles or links it, or the build files
# are written without its rules.
if(APPLE)
    enable_language(OBJCXX)
endif()

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

# Cesium Native's dependencies, now that the compiler is one the project accepts.
glideslope_vcpkg_install()

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

# SDL3, from ext/sdl - see cmake/Sdl.cmake. Linked by the presentation only.
include(Sdl)

# The shader compiler, from glslang and SPIRV-Cross in ext/ - see
# cmake/Shaders.cmake.
include(Shaders)

# Cesium Native, from ext/cesium-native - see cmake/CesiumNative.cmake. Linked by
# the presentation only.
include(CesiumNative)

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
    src/sim/selftest.cpp
    src/sim/autopilot.cpp
    src/sim/catalogue.cpp
    src/sim/checklist.cpp
    src/sim/checklist_run.cpp
    src/sim/lander.cpp
    src/sim/departure.cpp
    src/sim/controller.cpp
    src/sim/navigator.cpp
    src/sim/prediction.cpp
    src/sim/test_pilot.cpp
    src/sim/version.cpp)

# One include root, so every #include names the layer it crosses:
#   #include "sim/version.hpp"   not   #include "../sim/version.hpp"
target_include_directories(glideslope_sim PUBLIC src)
target_compile_definitions(glideslope_sim PRIVATE
    GLIDESLOPE_VERSION="${PROJECT_VERSION}")
target_link_libraries(glideslope_sim PUBLIC libJSBSim)
glideslope_configure(glideslope_sim)

# The network: the wire format, and the encoding and decoding of it. It links
# nothing - not the simulation, not a socket - because everything it does is
# turning bytes into values and back, and that is what makes it fuzzable.
# **glideslope_net links libsodium**, and nothing else outside the standard
# library. The transport's handshake needs X25519, and a key is the one thing
# in here that cannot be done with arithmetic. It still links no socket, no
# terrain and no flight model, which is what lets the whole of it be walked
# against every pattern of loss without a network.
find_package(unofficial-sodium CONFIG REQUIRED)

add_library(glideslope_net STATIC
    src/net/inputs.cpp
    src/net/inside.cpp
    src/net/handshake.cpp
    src/net/keys.cpp
    src/net/sealing.cpp
    src/net/interpolation.cpp
    src/net/messages.cpp
    src/net/protocol.cpp
    src/net/slots.cpp
    src/net/state.cpp
    src/net/reliable.cpp)
target_include_directories(glideslope_net PUBLIC src)
target_link_libraries(glideslope_net PUBLIC unofficial-sodium::sodium)
glideslope_configure(glideslope_net)

# ...and links none, checked once every target_link_libraries below has run.
cmake_language(DEFER CALL glideslope_check_sim_links glideslope_sim)

# ---------------------------------------------------------------------------
# The world - where things are on the Earth: positions, the terrain's heights,
# and the weather from reports. Links no presentation.
# ---------------------------------------------------------------------------
add_library(glideslope_world STATIC
    src/world/air_motion.cpp
    src/world/lift.cpp
    src/world/sky.cpp
    src/world/byte_source.cpp
    src/world/dem.cpp
    src/world/digest.cpp
    src/world/download.cpp
    src/world/geodesy.cpp
    src/world/geoid.cpp
    src/world/geotiff.cpp
    src/world/inflate.cpp
    src/world/json.cpp
    src/world/metar.cpp
    src/world/terrain_mesh.cpp
    src/world/weather.cpp
    src/world/winds_aloft.cpp
    src/world/zip.cpp)
target_include_directories(glideslope_world PUBLIC src)
# Terrain and weather arrive over the network, through the platform's HTTP
# client; the weather is given to the simulation as its sim::Weather.
target_link_libraries(glideslope_world PUBLIC glideslope_platform glideslope_sim)
glideslope_configure(glideslope_world)

# ---------------------------------------------------------------------------
# The platform layer - where things are on this machine. Paths only, today;
# input and sockets arrive with the phases that need them.
# ---------------------------------------------------------------------------
add_library(glideslope_platform STATIC
    src/platform/store.cpp
    src/platform/paths.cpp
    src/platform/socket.cpp)
target_include_directories(glideslope_platform PUBLIC src)
# HTTPS through what each system trusts - see src/platform/http.hpp.
if(WIN32)
    target_sources(glideslope_platform PRIVATE src/platform/http_winhttp.cpp
                                               src/platform/socket_winsock.cpp)
    target_link_libraries(glideslope_platform PRIVATE winhttp ws2_32)
elseif(APPLE)
    target_sources(glideslope_platform PRIVATE src/platform/http_nsurlsession.mm)
    set_source_files_properties(src/platform/http_nsurlsession.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(glideslope_platform PRIVATE "-framework Foundation")
    target_sources(glideslope_platform PRIVATE src/platform/socket_posix.cpp)
else()
    target_sources(glideslope_platform PRIVATE src/platform/http_curl.cpp
                                               src/platform/socket_posix.cpp)
    target_link_libraries(glideslope_platform PRIVATE ${CMAKE_DL_LIBS})
endif()
find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(glideslope_platform PUBLIC unofficial::sqlite3::sqlite3)
glideslope_configure(glideslope_platform)

# ---------------------------------------------------------------------------
# Input - flight controllers through SDL, to the aircraft's controls. Apart
# from glideslope_platform, which the CLI links, because it links SDL.
# ---------------------------------------------------------------------------
add_library(glideslope_input STATIC
    src/platform/input.cpp)
target_include_directories(glideslope_input PUBLIC src)
target_link_libraries(glideslope_input PUBLIC glideslope_sim SDL3::SDL3-static)
glideslope_configure(glideslope_input)

# ---------------------------------------------------------------------------
# The drawing that draws nothing: the aircraft visual models tools/make_models.py
# makes, where each sits on its aeroplane and the views of it, the scene's
# geometry - placements, cameras and the projection - and the terrain's colours
# and light. Its own library because it is presentation that links nothing
# presentational, so the unit tests hold all of it without SDL or Cesium Native
# behind them, and glideslope_gfx is the part that needs a GPU.
# ---------------------------------------------------------------------------
add_library(glideslope_model STATIC
    src/gfx/aircraft.cpp
    src/gfx/model.cpp
    src/gfx/scene.cpp
    src/gfx/terrain_colour.cpp)
target_include_directories(glideslope_model PUBLIC src)
target_link_libraries(glideslope_model PUBLIC glideslope_world)
glideslope_configure(glideslope_model)

# ---------------------------------------------------------------------------
# The renderer - SDL_GPU over Vulkan, Direct3D 12 and Metal.
# ---------------------------------------------------------------------------
add_library(glideslope_gfx STATIC
    src/gfx/hud.cpp
    src/gfx/renderer.cpp
    src/gfx/sky.cpp
    src/gfx/terrain_tiles.cpp)
target_include_directories(glideslope_gfx PUBLIC src)
target_link_libraries(glideslope_gfx PUBLIC SDL3::SDL3-static glideslope_world
                                            glideslope_model)
# Cesium Native is behind gfx/terrain_tiles.hpp, and none of it is in a header
# here.
target_link_libraries(glideslope_gfx PRIVATE Cesium3DTilesSelection
                                          Cesium3DTilesContent CesiumGltfContent)
glideslope_shaders(glideslope_gfx
    src/gfx/shaders/mesh.vert
    src/gfx/shaders/mesh.frag)
glideslope_configure(glideslope_gfx)

# ---------------------------------------------------------------------------
# glideslope - the simulator.
# ---------------------------------------------------------------------------
add_executable(glideslope
    src/frontend/client/flight.cpp
    src/frontend/client/main.cpp
    src/frontend/client/scenes.cpp
    src/frontend/client/terrain.cpp)
target_link_libraries(glideslope PRIVATE glideslope_gfx glideslope_input glideslope_sim glideslope_world
                                         glideslope_platform)
glideslope_configure(glideslope)

# ---------------------------------------------------------------------------
# glideslope_cli - the simulation with no window.
# ---------------------------------------------------------------------------
add_executable(glideslope_cli
    src/frontend/cli/main.cpp)
target_link_libraries(glideslope_cli PRIVATE glideslope_sim glideslope_net glideslope_world glideslope_platform)
glideslope_configure(glideslope_cli)

# ---------------------------------------------------------------------------
# glideslope_server - the server, which owns every aircraft. It links nothing
# presentational: its dashboard is written to a terminal, not drawn.
# ---------------------------------------------------------------------------
add_executable(glideslope_server
    src/frontend/server/main.cpp)
target_link_libraries(glideslope_server PRIVATE glideslope_sim glideslope_net
                                                glideslope_world glideslope_platform)
glideslope_configure(glideslope_server)

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
install(TARGETS glideslope glideslope_cli glideslope_server RUNTIME DESTINATION . COMPONENT glideslope)
# The client links SDL, statically; its licence travels with it.
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/LICENSE.txt"
        DESTINATION licenses RENAME SDL.txt COMPONENT glideslope)
# And Cesium Native, and the libraries vcpkg built for it, statically too: every
# package vcpkg installed for the target, each with the copyright file vcpkg
# keeps for it, under licenses/vcpkg/.
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ext/cesium-native/LICENSE"
        DESTINATION licenses RENAME CesiumNative.txt COMPONENT glideslope)
file(GLOB _vcpkg_copyrights
     "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/*/copyright")
foreach(_copyright IN LISTS _vcpkg_copyrights)
    get_filename_component(_port_dir "${_copyright}" DIRECTORY)
    get_filename_component(_port "${_port_dir}" NAME)
    install(FILES "${_copyright}" DESTINATION licenses/vcpkg RENAME "${_port}.txt"
            COMPONENT glideslope)
endforeach()
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

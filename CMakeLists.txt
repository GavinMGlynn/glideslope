cmake_minimum_required(VERSION 3.28)

# **Under WSL, the Windows directories are taken off PATH for the configure.**
# WSL appends every directory of Windows' own PATH, under /mnt/c, and every
# find_path, find_file and find_program that does not find what it looks for
# searches them all, through WSL's slow share of the Windows drive - and a
# search that fails is not cached, so it is made again at every configure.
# Profiled on 2026-09-23 a configure took eight and a half minutes, 224
# seconds of it looking for ICU data files that are not there and about 150
# more in SDL's OpenGL and X11 checks looking for GL/xmesa.h. A Linux build
# never wants a Windows directory. Only WSL sets these variables; a native
# Windows or macOS build keeps its PATH as it is.
if(DEFINED ENV{WSL_DISTRO_NAME} OR DEFINED ENV{WSL_INTEROP})
    string(REPLACE ":" ";" _glideslope_path "$ENV{PATH}")
    list(FILTER _glideslope_path EXCLUDE REGEX "^/mnt/[A-Za-z]/")
    string(REPLACE ";" ":" _glideslope_path "${_glideslope_path}")
    set(ENV{PATH} "${_glideslope_path}")
endif()

# **A server does not need a GPU stack.** `-DGLIDESLOPE_SERVER_ONLY=ON` builds
# the simulation, the network and the server and nothing else: no SDL, no
# Cesium Native, no shaders, no client, and no vcpkg - the three libraries it
# does need come from the system. That is what `deploy/Dockerfile` builds, and
# it is the difference between an image that takes minutes and one that takes
# most of an hour building a renderer nothing in it will ever run.
#
# It is read before anything else because it decides whether vcpkg's toolchain
# is used at all, and a toolchain has to be chosen before project().
option(GLIDESLOPE_SERVER_ONLY
       "Build only the server: no SDL, no Cesium, no vcpkg" OFF)

# vcpkg, for Cesium Native's dependencies: its toolchain before project(), and
# the packages installed after the platform gate - see cmake/Vcpkg.cmake.
if(NOT GLIDESLOPE_SERVER_ONLY)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Vcpkg.cmake")
endif()

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
if(NOT GLIDESLOPE_SERVER_ONLY)
    glideslope_vcpkg_install()
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(GLIDESLOPE_TESTS "Build the tests"                                       ON)
if(GLIDESLOPE_SERVER_ONLY AND GLIDESLOPE_TESTS)
    # The tests link the renderer and SDL, which a server-only build has not
    # got. Saying so is better than failing to find a target halfway down.
    message(STATUS "glideslope: server-only, so the tests are not built")
    set(GLIDESLOPE_TESTS OFF)
endif()
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
if(NOT GLIDESLOPE_SERVER_ONLY)
    include(Sdl)  # SDL
endif()

# The shader compiler, from glslang and SPIRV-Cross in ext/ - see
# cmake/Shaders.cmake.
if(NOT GLIDESLOPE_SERVER_ONLY)
    include(Shaders)  # the shader compiler
endif()

# Cesium Native, from ext/cesium-native - see cmake/CesiumNative.cmake. Linked by
# the presentation only.
if(NOT GLIDESLOPE_SERVER_ONLY)
    include(CesiumNative)  # Cesium Native
endif()

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
    src/sim/lesson.cpp
    src/sim/lesson_run.cpp
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
if(GLIDESLOPE_SERVER_ONLY)
    # The system's, found by hand: vcpkg is not here to provide a target.
    find_library(GLIDESLOPE_SODIUM NAMES sodium REQUIRED)
    find_path(GLIDESLOPE_SODIUM_INCLUDE NAMES sodium.h REQUIRED)
    add_library(glideslope_sodium INTERFACE)
    target_link_libraries(glideslope_sodium INTERFACE "${GLIDESLOPE_SODIUM}")
    target_include_directories(glideslope_sodium
                               INTERFACE "${GLIDESLOPE_SODIUM_INCLUDE}")
    add_library(unofficial-sodium::sodium ALIAS glideslope_sodium)
else()
    find_package(unofficial-sodium CONFIG REQUIRED)
endif()

add_library(glideslope_net STATIC
    src/net/inputs.cpp
    src/net/inside.cpp
    src/net/handshake.cpp
    src/net/keys.cpp
    src/net/sealing.cpp
    src/net/interpolation.cpp
    src/net/messages.cpp
    src/net/protocol.cpp
    src/net/session.cpp
    src/net/slots.cpp
    src/net/state.cpp
    src/net/reliable.cpp)
target_include_directories(glideslope_net PUBLIC src)
# **The network links the platform**, because a client session owns its
# socket. It still links nothing presentational, which is what the layering
# check cares about.
target_link_libraries(glideslope_net PUBLIC unofficial-sodium::sodium
                                             glideslope_platform)
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
if(GLIDESLOPE_SERVER_ONLY)
    find_library(GLIDESLOPE_SQLITE3 NAMES sqlite3 REQUIRED)
    find_path(GLIDESLOPE_SQLITE3_INCLUDE NAMES sqlite3.h REQUIRED)
    add_library(glideslope_sqlite3 INTERFACE)
    target_link_libraries(glideslope_sqlite3 INTERFACE "${GLIDESLOPE_SQLITE3}")
    target_include_directories(glideslope_sqlite3
                               INTERFACE "${GLIDESLOPE_SQLITE3_INCLUDE}")
    add_library(unofficial::sqlite3::sqlite3 ALIAS glideslope_sqlite3)
else()
    find_package(unofficial-sqlite3 CONFIG REQUIRED)
endif()
target_link_libraries(glideslope_platform PUBLIC unofficial::sqlite3::sqlite3)
glideslope_configure(glideslope_platform)
# **The presentation, built only when there is something to present.**
# `-DGLIDESLOPE_SERVER_ONLY=ON` leaves all of this out - the input layer and
# the renderer link SDL, the model loader and the renderer link Cesium
# Native, and the client is all three. A server runs none of them.
if(NOT GLIDESLOPE_SERVER_ONLY)

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
    target_link_libraries(glideslope PRIVATE glideslope_gfx glideslope_input glideslope_net glideslope_sim glideslope_world
                                             glideslope_platform)
    glideslope_configure(glideslope)
endif()


# ---------------------------------------------------------------------------
# glideslope_cli - the simulation with no window.
# ---------------------------------------------------------------------------
add_executable(glideslope_cli
    src/frontend/cli/main.cpp)
target_link_libraries(glideslope_cli PRIVATE glideslope_sim glideslope_net glideslope_world glideslope_platform)
glideslope_configure(glideslope_cli)

# ---------------------------------------------------------------------------
# glideslope_server - the server, which owns every aircraft. Its dashboard is
# in the terminal, or in an SDL window with --window: SDL is linked statically
# and loads a display library only when that window is asked for, so a server
# without --window runs where none is installed, as a host in the cloud has.
# ---------------------------------------------------------------------------
add_executable(glideslope_server
    src/frontend/server/main.cpp
    src/frontend/server/dashboard.cpp)
target_link_libraries(glideslope_server PRIVATE glideslope_sim glideslope_net
                                                glideslope_world glideslope_platform)
# A server-only build has no SDL at all, and so no window: --window is refused
# there, saying why (window_none.cpp).
if(GLIDESLOPE_SERVER_ONLY)
    target_sources(glideslope_server PRIVATE src/frontend/server/window_none.cpp)
else()
    target_sources(glideslope_server PRIVATE src/frontend/server/window.cpp)
    target_link_libraries(glideslope_server PRIVATE SDL3::SDL3-static)
endif()
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
if(GLIDESLOPE_SERVER_ONLY)
    install(TARGETS glideslope_cli glideslope_server
            RUNTIME DESTINATION . COMPONENT glideslope)
else()
    install(TARGETS glideslope glideslope_cli glideslope_server RUNTIME DESTINATION . COMPONENT glideslope)
endif()
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

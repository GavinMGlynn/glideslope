# Sdl.cmake - SDL3, from the pinned submodule, linked statically.
#
# SDL is the window, the input devices and - through SDL_GPU - the one graphics
# API over Vulkan, Direct3D 12 and Metal (REQUIREMENTS.md section 3). Only the
# presentation links it: cmake/Layering.cmake refuses it in the simulation.
#
# Static, like JSBSim, so a package is a program and its data rather than a
# program, its data and a set of shared libraries to find. Its own tests,
# examples and install rules are off; they would build and install things this
# project does not ship.

set(_sdl "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl")
if(NOT EXISTS "${_sdl}/CMakeLists.txt")
    message(FATAL_ERROR
        "ext/sdl is empty - the submodules are not checked out.\n"
        "  git submodule update --init --depth 1 ext/sdl")
endif()

set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)
set(SDL_GPU          ON  CACHE BOOL "" FORCE)

add_subdirectory("${_sdl}" "${CMAKE_CURRENT_BINARY_DIR}/ext/sdl" EXCLUDE_FROM_ALL)
set_target_properties(SDL3-static PROPERTIES SYSTEM TRUE)

file(STRINGS "${_sdl}/include/SDL3/SDL_version.h" _sdl_version_lines
     REGEX "^#define SDL_(MAJOR|MINOR|MICRO)_VERSION +[0-9]+")
set(GLIDESLOPE_SDL_VERSION "")
foreach(_part MAJOR MINOR MICRO)
    foreach(_line IN LISTS _sdl_version_lines)
        if(_line MATCHES "SDL_${_part}_VERSION +([0-9]+)")
            list(APPEND GLIDESLOPE_SDL_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endforeach()
list(JOIN GLIDESLOPE_SDL_VERSION "." GLIDESLOPE_SDL_VERSION)
message(STATUS "glideslope: SDL ${GLIDESLOPE_SDL_VERSION} from ext/sdl")

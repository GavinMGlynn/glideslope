# A tiny project that links SDL into a target called glideslope_sim in one of
# several ways, chosen by -DCASE, and runs the real link check over it. Nothing
# here is built; the check runs at configure time. The "SDL" is an empty library
# under SDL's own target names, because the check is about names in the link
# graph, not about SDL's code.
cmake_minimum_required(VERSION 3.28)
project(glideslope_link_probe LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${GLIDESLOPE_ROOT}/cmake")
include(Layering)

set(_empty "${CMAKE_CURRENT_BINARY_DIR}/empty.cpp")
file(WRITE "${_empty}" "void glideslope_link_probe() {}\n")

add_library(fake_sdl STATIC "${_empty}")
add_library(SDL3::SDL3-static ALIAS fake_sdl)
add_library(helper STATIC "${_empty}")
add_library(helper_interface INTERFACE)
add_library(glideslope_sim STATIC "${_empty}")

if(CASE STREQUAL "direct")
    target_link_libraries(glideslope_sim PRIVATE SDL3::SDL3-static)
elseif(CASE STREQUAL "through-a-public-dependency")
    target_link_libraries(helper PUBLIC SDL3::SDL3-static)
    target_link_libraries(glideslope_sim PRIVATE helper)
elseif(CASE STREQUAL "through-a-private-dependency")
    target_link_libraries(helper PRIVATE SDL3::SDL3-static)
    target_link_libraries(glideslope_sim PRIVATE helper)
elseif(CASE STREQUAL "through-an-interface-library")
    target_link_libraries(helper_interface INTERFACE SDL3::SDL3-static)
    target_link_libraries(glideslope_sim PUBLIC helper_interface)
elseif(CASE STREQUAL "by-library-name")
    target_link_libraries(glideslope_sim PRIVATE SDL3)
elseif(CASE STREQUAL "by-file-path")
    target_link_libraries(glideslope_sim PRIVATE /opt/sdl/lib/libSDL3.so)
elseif(CASE STREQUAL "none")
    target_link_libraries(glideslope_sim PRIVATE helper helper_interface)
else()
    message(FATAL_ERROR "unknown CASE '${CASE}'")
endif()

glideslope_check_sim_links(glideslope_sim)

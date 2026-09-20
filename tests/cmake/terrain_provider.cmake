# terrain_provider.cmake - a terrain provider draws, or says why it cannot.
#
#   cmake -DPROGRAM=<glideslope> -DPROVIDER=<open|ion|google> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -P terrain_provider.cmake
#
# **A key belongs to the user and is never in the repository.** A provider that
# needs one and has not been given one must say so plainly rather than fail, and
# this holds it to that: with the config directory pointed at an empty place and
# no key in the environment, the provider is asked for and its answer must name
# what is missing and where to put it.
#
# Given a key, the same provider must draw: a shot of Mount Taranaki with the
# provider's own attribution along the bottom of it, which is what its terms
# ask for. Without a key the test reports itself skipped - skipped, never
# passed - because there is nothing here to prove.
#
# The open provider needs no key and is held to both halves the same way: it
# always draws, and its attribution is always there.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/bmp.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

# Mount Taranaki, seen from the north-east: a cone that stands alone, which
# every provider has and which is unmistakable in a shot.
set(_at "-39.0,174.3,4000")
set(_toward "-39.296,174.064,2518")
# Small: a streamed provider refines until it runs out of levels, and every
# tile the view needs is waited for, so the frame's size is most of what the
# test costs.
set(_size 320x240)

# --- with no key at all -----------------------------------------------------
#
# The config directory is pointed at an empty place. Each platform reads a
# different variable for it, so all three are set: XDG_CONFIG_HOME on Linux,
# APPDATA on Windows, HOME on macOS.
set(_nowhere "${WORK}/no-keys-${PROVIDER}")
file(REMOVE_RECURSE "${_nowhere}")
file(MAKE_DIRECTORY "${_nowhere}")
# Kept, so that the user's own are put back for the second half below.
set(_was_xdg "$ENV{XDG_CONFIG_HOME}")
set(_was_appdata "$ENV{APPDATA}")
set(_was_home "$ENV{HOME}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{GLIDESLOPE_CESIUM_ION_TOKEN} "")
set(ENV{GLIDESLOPE_GOOGLE_MAPS_KEY} "")
set(ENV{XDG_CONFIG_HOME} "${_nowhere}")
set(ENV{APPDATA} "${_nowhere}")
set(ENV{HOME} "${_nowhere}")
execute_process(
    COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size ${_size}
            --screen terrain --terrain ${PROVIDER} --at ${_at} --toward ${_toward}
            --shot-at 2 --shot "${WORK}/nokey-${DRIVER}-${PROVIDER}.bmp"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(PROVIDER STREQUAL "open")
    # It needs nothing of the user, so it must simply have worked.
    if(NOT _rc EQUAL 0 AND _err MATCHES "could not download")
        message(STATUS "the DEM could not be had: ${_err}")
        cmake_language(EXIT 77)
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the open provider needs no key and still failed: "
                            "exited ${_rc}\n${_err}")
    endif()
else()
    if(_rc EQUAL 0)
        message(FATAL_ERROR
                "${PROVIDER} drew with no key at all, which it cannot have "
                "done honestly")
    endif()
    # It must say what is missing and where to put it, not merely fail.
    if(NOT _err MATCHES "own token|own Google Maps Platform key")
        message(FATAL_ERROR
                "${PROVIDER} without a key did not say what it needs:\n${_err}")
    endif()
    if(NOT _err MATCHES "config directory|GLIDESLOPE_")
        message(FATAL_ERROR
                "${PROVIDER} without a key did not say where to put one:\n${_err}")
    endif()
    message(STATUS "${PROVIDER} with no key says: ${_err}")
endif()

# --- with whatever key the user has -----------------------------------------
#
# Put back exactly what was there, so that the provider looks where the user
# actually keeps a key. Setting these to an empty string rather than removing
# them would be a home directory named "", which is not the same thing.
if(_was_xdg STREQUAL "")
    unset(ENV{XDG_CONFIG_HOME})
else()
    set(ENV{XDG_CONFIG_HOME} "${_was_xdg}")
endif()
if(_was_appdata STREQUAL "")
    unset(ENV{APPDATA})
else()
    set(ENV{APPDATA} "${_was_appdata}")
endif()
if(_was_home STREQUAL "")
    unset(ENV{HOME})
else()
    set(ENV{HOME} "${_was_home}")
endif()
unset(ENV{GLIDESLOPE_CESIUM_ION_TOKEN})
unset(ENV{GLIDESLOPE_GOOGLE_MAPS_KEY})

set(_shot "${WORK}/provider-${DRIVER}-${PROVIDER}.bmp")
file(REMOVE "${_shot}")
execute_process(
    COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size ${_size}
            --screen terrain --terrain ${PROVIDER} --at ${_at} --toward ${_toward}
            --shot-at 2 --shot "${_shot}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT _rc EQUAL 0)
    if(_err MATCHES "own token|own Google Maps Platform key")
        message(STATUS "${PROVIDER} has no key on this machine, so there is "
                       "nothing to draw: ${_err}")
        cmake_language(EXIT 77)
    endif()
    if(_err MATCHES "could not download")
        message(STATUS "the terrain could not be had: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "${PROVIDER} exited ${_rc}\n${_err}")
endif()
glideslope_judge_leaks("${_err}")

if(NOT EXISTS "${_shot}")
    message(FATAL_ERROR "${PROVIDER} wrote no shot")
endif()

# It drew something: tiles were put on screen, not an empty sky.
if(NOT _out MATCHES "terrain of ([0-9]+) tiles")
    message(FATAL_ERROR "${PROVIDER} did not say what it drew:\n${_out}")
endif()
if(CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "${PROVIDER} drew no terrain at all")
endif()
message(STATUS "${PROVIDER} drew ${CMAKE_MATCH_1} tiles")

# **Its attribution is on screen.** The credits are drawn along the bottom in
# the HUD's own font, so the bottom of the frame must not be empty: the rows
# the credits occupy carry text where an undrawn frame would be flat sky or
# flat ground. Counting the pixels that are the credit colour is what says so.
bmp_load("${_shot}")
math(EXPR _from "${BMP_HEIGHT} - 120")
if(_from LESS 0)
    set(_from 0)
endif()
set(_lit 0)
math(EXPR _last_y "${BMP_HEIGHT} - 1")
math(EXPR _last_x "${BMP_WIDTH} - 1")
foreach(_y RANGE ${_from} ${_last_y})
    foreach(_x RANGE 0 ${_last_x} 2)
        bmp_pixel(_p ${_x} ${_y})
        list(GET _p 0 _r)
        list(GET _p 1 _g)
        list(GET _p 2 _b)
        # The credit text is the HUD's green: strong green, little red or blue.
        if(_g GREATER 150 AND _r LESS 120 AND _b LESS 120)
            math(EXPR _lit "${_lit} + 1")
        endif()
    endforeach()
endforeach()
if(_lit LESS 200)
    message(FATAL_ERROR
            "${PROVIDER} drew its terrain with no attribution under it: only "
            "${_lit} pixels of credit text along the bottom of ${_shot}")
endif()
message(STATUS "${PROVIDER}: ${_lit} pixels of attribution on screen")

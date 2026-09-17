# Jsbsim.cmake - JSBSim's library, from the pinned submodule, and nothing else of
# it.
#
# **Its src/ directory, not its top-level project.** JSBSim's top-level
# CMakeLists.txt also runs Doxygen at configure time if it finds it, builds a
# Python module if it finds Cython, asks lsb_release about the machine, and
# includes CPack with its own package names - every one of which would leak into
# this project. src/ builds the library and its command-line program, and needs
# only a few variables from above it: PROJECT_VERSION, which it compiles into
# JSBSIM_VERSION, LIBRARY_VERSION and LIBRARY_SOVERSION for the library's
# properties, and SYSTEM_EXPAT. They are set inside a block() so that none of
# them changes this project's own variables.
#
# ext/ is untouched, as everywhere else: the version is read from JSBSim's own
# CMakeLists.txt rather than written here, and the warning set is never applied
# to its targets. The sanitizers are, in the sanitized presets, because a
# program half-instrumented by AddressSanitizer reports container overflows
# that are not there.

set(_jsbsim "${CMAKE_CURRENT_SOURCE_DIR}/ext/jsbsim")
if(NOT EXISTS "${_jsbsim}/src/CMakeLists.txt")
    message(FATAL_ERROR
        "ext/jsbsim is empty - the submodules are not checked out.\n"
        "  git submodule update --init --depth 1 ext/jsbsim")
endif()

file(STRINGS "${_jsbsim}/CMakeLists.txt" _version_lines
     REGEX "^set\\(PROJECT_VERSION_(MAJOR|MINOR|PATCH) ")
set(GLIDESLOPE_JSBSIM_VERSION "")
foreach(_part MAJOR MINOR PATCH)
    foreach(_line IN LISTS _version_lines)
        if(_line MATCHES "PROJECT_VERSION_${_part} \"([0-9]+)\"")
            list(APPEND GLIDESLOPE_JSBSIM_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endforeach()
list(JOIN GLIDESLOPE_JSBSIM_VERSION "." GLIDESLOPE_JSBSIM_VERSION)
if(NOT GLIDESLOPE_JSBSIM_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "could not read JSBSim's version from ${_jsbsim}/CMakeLists.txt")
endif()

block()
    set(PROJECT_VERSION "${GLIDESLOPE_JSBSIM_VERSION}")
    set(LIBRARY_VERSION "${GLIDESLOPE_JSBSIM_VERSION}")
    string(REGEX REPLACE "\\..*" "" LIBRARY_SOVERSION "${GLIDESLOPE_JSBSIM_VERSION}")
    set(SYSTEM_EXPAT OFF)
    add_subdirectory("${_jsbsim}/src" "${CMAKE_CURRENT_BINARY_DIR}/ext/jsbsim" EXCLUDE_FROM_ALL)
endblock()

# Its headers are somebody else's: included as system headers, so this project's
# warning set does not fire inside them.
set_target_properties(libJSBSim PROPERTIES SYSTEM TRUE)

# Every target JSBSim's src/ tree defines, for the sanitizers.
function(_glideslope_targets_under dir out)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_sub IN LISTS _subdirs)
        _glideslope_targets_under("${_sub}" _more)
        list(APPEND _targets ${_more})
    endforeach()
    set(${out} "${_targets}" PARENT_SCOPE)
endfunction()
_glideslope_targets_under("${_jsbsim}/src" _jsbsim_targets)
foreach(_t IN LISTS _jsbsim_targets)
    get_target_property(_type ${_t} TYPE)
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
        glideslope_sanitize(${_t})
    endif()
endforeach()

message(STATUS "glideslope: JSBSim ${GLIDESLOPE_JSBSIM_VERSION} from ext/jsbsim")

# The licences a program with JSBSim linked into it has to carry: JSBSim's own
# (LGPL-2.1), and those of the two libraries it bundles in its source tree -
# expat, its XML parser, and GeographicLib (both MIT). Installed into licenses/
# in a package, under the names given here.
set(GLIDESLOPE_JSBSIM_LICENCES
    "COPYING=JSBSim.txt"
    "src/simgear/xml/COPYING=expat.txt"
    "src/GeographicLib/LICENSE.txt=GeographicLib.txt")

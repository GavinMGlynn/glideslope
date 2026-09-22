# CesiumNative.cmake - Cesium Native, from ext/cesium-native.
#
# Its dependencies were installed by vcpkg at project() (cmake/Vcpkg.cmake);
# this checks they are the ones Cesium Native asks for, and adds its libraries.
# Only those something links are built.

set(_cesium "${PROJECT_SOURCE_DIR}/ext/cesium-native")
if(NOT EXISTS "${_cesium}/CMakeLists.txt")
    message(FATAL_ERROR
        "ext/cesium-native is empty: run `git submodule update --init ext/cesium-native`")
endif()

# **vcpkg.json must list what Cesium Native's own manifest lists**, less curl,
# so that upgrading Cesium Native cannot quietly leave a dependency out - and
# this project's own packages besides, each named here.
#
# The check was once that the two lists were equal, which meant this project
# could not add a package of its own at all: doing so failed the configure at
# once. It now holds that ours is theirs, less curl, plus exactly the list
# below. That keeps what the check was for - a dependency of theirs going
# missing is still caught - while letting this project depend on something
# they do not.
#
# **libsodium** is the transport's: `Noise_IK_25519_ChaChaPoly_BLAKE2b` needs
# X25519, ChaCha20-Poly1305 and BLAKE2b, and libsodium has all three
# (REQUIREMENTS.md 6.7, docs/TRANSPORT.md). It comes through vcpkg rather than
# as a submodule under ext/ because it ships no CMake build of its own -
# autotools on Unix, Visual Studio solutions on Windows - so a submodule would
# mean writing a CMakeLists here for somebody else's library.
set(_glideslope_own_packages libsodium)
function(_glideslope_manifest_names file out)
    file(READ "${file}" _json)
    string(JSON _count LENGTH "${_json}" dependencies)
    math(EXPR _last "${_count} - 1")
    set(_names "")
    foreach(_i RANGE ${_last})
        string(JSON _type TYPE "${_json}" dependencies ${_i})
        if(_type STREQUAL "OBJECT")
            string(JSON _name GET "${_json}" dependencies ${_i} name)
        else()
            string(JSON _name GET "${_json}" dependencies ${_i})
        endif()
        list(APPEND _names "${_name}")
    endforeach()
    list(SORT _names)
    set(${out} "${_names}" PARENT_SCOPE)
endfunction()
_glideslope_manifest_names("${PROJECT_SOURCE_DIR}/vcpkg.json" _ours)
_glideslope_manifest_names("${_cesium}/vcpkg.json" _theirs)
list(REMOVE_ITEM _theirs curl)
set(_wanted ${_theirs} ${_glideslope_own_packages})
list(SORT _wanted)
if(NOT _ours STREQUAL _wanted)
    message(FATAL_ERROR
            "vcpkg.json lists\n  ${_ours}\nbut ext/cesium-native/vcpkg.json, less "
            "curl, lists\n  ${_theirs}\nand this project's own are\n  "
            "${_glideslope_own_packages}\nwhich together want\n  ${_wanted}")
endif()

# Cesium Native's options: no tests, no clang-tidy, no curl, nothing installed -
# the package installs what it needs itself - and the static C runtime on
# Windows, as everything here uses.
set(CESIUM_USE_EZVCPKG OFF CACHE BOOL "" FORCE)
set(CESIUM_TESTS_ENABLED OFF CACHE BOOL "" FORCE)
set(CESIUM_ENABLE_CLANG_TIDY OFF CACHE BOOL "" FORCE)
set(CESIUM_DISABLE_CURL ON CACHE BOOL "" FORCE)
set(CESIUM_INSTALL_STATIC_LIBS OFF CACHE BOOL "" FORCE)
set(CESIUM_INSTALL_HEADERS OFF CACHE BOOL "" FORCE)
set(CESIUM_MSVC_STATIC_RUNTIME_ENABLED ON CACHE BOOL "" FORCE)

# Its packages are all vcpkg's, found through the toolchain's prefix path; the
# directories on PATH are not searched for them. Under WSL, PATH holds Windows'
# directories, and searching those for thirty packages takes minutes.
block()
    set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF)
    add_subdirectory("${_cesium}" "${CMAKE_CURRENT_BINARY_DIR}/ext/cesium-native"
                     EXCLUDE_FROM_ALL SYSTEM)
endblock()

# Its libraries are built with its own warnings, which are not this project's to
# fix: they are not made errors here, where the compilers are not all the ones
# Cesium Native is tested with. They are sanitized with the rest, as JSBSim is,
# but for the alignment check, which its quantized-mesh reader breaks on every
# terrain tile - cmake/Sanitizers.cmake says why, and what would take it back.
function(_glideslope_cesium_targets dir out)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_sub IN LISTS _subdirs)
        _glideslope_cesium_targets("${_sub}" _more)
        list(APPEND _targets ${_more})
    endforeach()
    set(${out} "${_targets}" PARENT_SCOPE)
endfunction()
_glideslope_cesium_targets("${_cesium}" _cesium_targets)
foreach(_t IN LISTS _cesium_targets)
    get_target_property(_type ${_t} TYPE)
    if(_type MATCHES "^(STATIC|SHARED|OBJECT|MODULE)_LIBRARY$|^EXECUTABLE$")
        set_target_properties(${_t} PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
        glideslope_sanitize(${_t})
        # ...but not the alignment check: see cmake/Sanitizers.cmake.
        glideslope_allow_misaligned(${_t})
    endif()
endforeach()

file(READ "${_cesium}/CMakeLists.txt" _cesium_cmake)
string(REGEX MATCH "project\\(cesium-native[ \t\r\n]+VERSION ([0-9.]+)" _ "${_cesium_cmake}")
set(GLIDESLOPE_CESIUM_VERSION "${CMAKE_MATCH_1}")
message(STATUS "glideslope: Cesium Native ${GLIDESLOPE_CESIUM_VERSION} from ext/cesium-native, "
               "its dependencies from vcpkg (${VCPKG_TARGET_TRIPLET})")

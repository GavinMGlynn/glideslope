# Platform.cmake - the supported set, enforced at configure time.
#
# Glideslope targets three platforms, all 64-bit (REQUIREMENTS.md section 2):
#
#   Linux x86_64    RHEL family and Debian family
#   Windows x64     MSVC or clang-cl
#   macOS arm64     Apple silicon; x86_64 is optional and nothing builds it
#
# **Refused rather than assumed.** A 32-bit build is one no machine here tests,
# so it stops at configure time with a message saying what was found and what is
# needed, rather than failing later somewhere stranger. A platform outside the
# set is allowed but warned about, because it may well build and nothing checks
# that it works.
#
# Written against CMAKE_SYSTEM_NAME rather than WIN32 and APPLE, and with no
# try_compile, so that tests/cmake/platform_gate.cmake can run the same function
# in script mode with a toolchain described by -D flags - which is how a 32-bit
# toolchain and old compilers get tested on a machine that has neither.

# ---------------------------------------------------------------------------
# Compiler floors - the oldest version of each compiler that may configure.
#
#   GNU 14          REQUIREMENTS.md's RHEL note: gcc-toolset-14. CI builds with
#                   GCC 14 on Ubuntu and on Rocky 9, whose own GCC 11 is refused.
#   Clang 19        also clang-cl, which CMake reports as Clang
#   AppleClang 16   Xcode 16
#   MSVC 19.39      Visual Studio 2022 17.9
#
# The last three are gearstick's floors, carried over. Every toolchain in this
# project's CI and on its development machines is at or above them. A compiler
# with no entry here is warned about rather than refused.
# ---------------------------------------------------------------------------
set(GLIDESLOPE_COMPILER_FLOORS
    "GNU=14"
    "Clang=19"
    "AppleClang=16"
    "MSVC=19.39")

function(glideslope_check_platform)
    if(CMAKE_SIZEOF_VOID_P LESS 8)
        message(FATAL_ERROR
            "glideslope is 64-bit only.\n"
            "  found:  ${CMAKE_SIZEOF_VOID_P}-byte pointers "
            "(${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR})\n"
            "  needed: a 64-bit toolchain")
    endif()

    set(_supported "Linux x86_64, Windows x64, macOS arm64")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
       AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        set(_platform "linux-x86_64")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows"
           AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
        set(_platform "windows-x64")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # CMAKE_OSX_ARCHITECTURES is what gets built; the processor is only
        # what the machine doing the building happens to be.
        set(_arch "${CMAKE_OSX_ARCHITECTURES}")
        if(NOT _arch)
            set(_arch "${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        set(_platform "macos-${_arch}")
        if(NOT _arch STREQUAL "arm64")
            message(WARNING
                "glideslope builds macOS for arm64 only in CI; ${_arch} may "
                "build, but nothing verifies that it works.")
        endif()
    else()
        set(_platform "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
        message(WARNING
            "glideslope has no CI for ${_platform}; it may build, but nothing "
            "verifies that it works. Supported: ${_supported}.")
    endif()

    set(_floor "")
    foreach(_entry IN LISTS GLIDESLOPE_COMPILER_FLOORS)
        string(REPLACE "=" ";" _pair "${_entry}")
        list(GET _pair 0 _id)
        list(GET _pair 1 _min)
        if(CMAKE_CXX_COMPILER_ID STREQUAL _id)
            set(_floor "${_min}")
        endif()
    endforeach()

    if(NOT _floor)
        message(WARNING
            "glideslope has no compiler floor for ${CMAKE_CXX_COMPILER_ID}; it "
            "may build, but nothing verifies that it works.")
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS _floor)
        set(_hint "")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(_hint
                "On RHEL or Rocky 9, whose own GCC is 11, enable the toolset first:\n"
                "  source /opt/rh/gcc-toolset-14/enable")
        endif()
        message(FATAL_ERROR
            "glideslope needs a newer compiler.\n"
            "  found:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
            "  required: ${CMAKE_CXX_COMPILER_ID} ${_floor} or newer\n"
            ${_hint})
    endif()

    message(STATUS
        "glideslope: ${_platform}, ${CMAKE_CXX_COMPILER_ID} "
        "${CMAKE_CXX_COMPILER_VERSION} (C++20)")
    set(GLIDESLOPE_PLATFORM "${_platform}" PARENT_SCOPE)
endfunction()

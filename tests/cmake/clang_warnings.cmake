# clang_warnings.cmake - every first-party source compiled under clang, with
# this project's own warnings.
#
#   cmake -DBUILD=<build dir> -DSOURCE=<project dir> -P clang_warnings.cmake
#
# **Warnings are errors, in every build type - but only for the compiler doing
# the building.** Both of this project's Linux builds are GCC, and GCC does
# not warn about an unused constant at namespace scope in C++: not under
# `-Wall -Wextra`, and not under `-Wunused-const-variable` at any level, which
# it honours for C alone. Clang does. So that class of fault is invisible here
# and reds macOS and Windows clang-cl together, which is what happened on
# 2026-09-21 - a commit both Linux jobs passed.
#
# This closes that: the compile commands CMake already exports are re-run
# under clang, syntax only, with the warning set the build uses and the
# unused-constant warning added. It is the same flags on the same files, so a
# warning here is a warning the other compiler would give.
#
# **What is left out.** Flags GCC understands and clang does not - its module
# mapper, its dependency format, and a warning suppression named for a GCC
# warning - are dropped, and are listed below with their reason. Third-party
# sources are not compiled: they are somebody else's, and `ext/` is untouched
# by this project's warnings anyway.
#
# Where clang is not installed this reports itself skipped - skipped, never
# passed.

cmake_minimum_required(VERSION 3.28)

find_program(_clang NAMES clang++ clang++-19 clang++-18 clang++-17)
if(NOT _clang)
    message(STATUS "clang++ is not installed, so nothing was compiled with it")
    cmake_language(EXIT 77)
endif()

set(_commands "${BUILD}/compile_commands.json")
if(NOT EXISTS "${_commands}")
    message(FATAL_ERROR "${_commands} is not there; the build exports it")
endif()
file(READ "${_commands}" _json)

string(JSON _count LENGTH "${_json}")
math(EXPR _last "${_count} - 1")

# **A build whose compiler speaks MSVC's language is not re-run here.** Its
# commands are cl's - /nologo, -std:c++20, /EHsc - and clang++ reads each of
# those as a file it cannot find, which failed every Windows run from
# 2026-09-21 until a later break hid it. Nor is it needed there: Windows CI
# also builds every source with clang-cl, warnings as errors, and that build
# is the clang pass this test exists to supply for the GCC-only Linux jobs. So
# an MSVC-style build - cl or clang-cl - reports itself skipped, never passed.
string(JSON _probe GET "${_json}" 0 command)
separate_arguments(_probe_argv WINDOWS_COMMAND "${_probe}")
list(GET _probe_argv 0 _probe_compiler)
# Its backslashes made forward, or CMake on any host but Windows takes the
# whole of C:\...\cl.exe for the file's name.
file(TO_CMAKE_PATH "${_probe_compiler}" _probe_compiler)
get_filename_component(_probe_name "${_probe_compiler}" NAME_WE)
string(TOLOWER "${_probe_name}" _probe_name)
if(_probe_name STREQUAL "cl" OR _probe_name STREQUAL "clang-cl")
    message(STATUS "this build compiles with ${_probe_name}, whose commands clang++ "
                   "cannot take; Windows' clang-cl build is its clang pass")
    cmake_language(EXIT 77)
endif()

set(_walked 0)
set(_skipped 0)
set(_wrong "")
foreach(_i RANGE ${_last})
    string(JSON _file GET "${_json}" ${_i} file)
    # First-party only: src/ and tests/ of this project, and nothing in ext/.
    if(NOT _file MATCHES "^${SOURCE}/(src|tests)/")
        math(EXPR _skipped "${_skipped} + 1")
        continue()
    endif()
    # Objective-C++ is macOS's own and is not compiled here.
    if(_file MATCHES "\\.mm$")
        math(EXPR _skipped "${_skipped} + 1")
        continue()
    endif()
    string(JSON _directory GET "${_json}" ${_i} directory)
    string(JSON _command GET "${_json}" ${_i} command)

    separate_arguments(_argv UNIX_COMMAND "${_command}")
    set(_keep "")
    set(_drop_next OFF)
    set(_first ON)
    foreach(_arg IN LISTS _argv)
        if(_drop_next)
            set(_drop_next OFF)
            continue()
        endif()
        if(_first)
            set(_first OFF)
            continue() # the compiler itself; clang takes its place
        endif()
        # -o takes the object file's name, and nothing is written, so both
        # it and its argument go. -c takes no argument - it means compile
        # rather than link - and the source file merely follows it, so only
        # the flag goes. Dropping what follows -c drops the input, and clang
        # then says "no input files", which is how this was found.
        if(_arg STREQUAL "-o")
            set(_drop_next ON)
            continue()
        endif()
        if(_arg STREQUAL "-c")
            continue()
        endif()
        # GCC's, which clang does not have: its module mapper and dependency
        # format, and -Wno-maybe-uninitialized, named for a GCC warning.
        if(_arg MATCHES "^-fmodules-ts$" OR _arg MATCHES "^-fmodule-mapper=" OR
           _arg MATCHES "^-fdeps-format=" OR _arg STREQUAL "-Wno-maybe-uninitialized")
            continue()
        endif()
        # Dependency files: -MD, -MT X, -MF X. Nothing is built, so nothing
        # should be written beside it.
        if(_arg STREQUAL "-MD")
            continue()
        endif()
        if(_arg STREQUAL "-MT" OR _arg STREQUAL "-MF")
            set(_drop_next ON)
            continue()
        endif()
        list(APPEND _keep "${_arg}")
    endforeach()

    execute_process(
        COMMAND "${_clang}" ${_keep} -fsyntax-only -Wunused-const-variable
        WORKING_DIRECTORY "${_directory}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        string(APPEND _wrong "\n${_err}")
    endif()
    math(EXPR _walked "${_walked} + 1")
endforeach()

if(NOT _wrong STREQUAL "")
    message(FATAL_ERROR
            "clang says what GCC did not, in ${_walked} first-party sources:${_wrong}")
endif()

# **The space this walked, stated.** A build that stopped exporting commands,
# or a filter that quietly matched nothing, would otherwise pass in silence.
if(_walked LESS 80)
    message(FATAL_ERROR
            "only ${_walked} first-party sources were compiled, which is fewer "
            "than this project has; the filter or the compile commands are wrong")
endif()
message(STATUS "clang compiled ${_walked} first-party sources and said nothing; "
               "${_skipped} others were left out")

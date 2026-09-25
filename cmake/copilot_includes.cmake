# copilot_includes.cmake - every file the copilot's compiled sources open is
# in its stage, or outside the project's source and build trees (the system's
# and the compiler's headers), however the include that opened it was spelled.
#
#   cmake -DCXX=<compiler> -DCXX_STYLE=<gnu|msvc> -DSTAGE=<stage>
#         -DSOURCES=<staged sources, |-separated> -DSOURCE_DIR=<source tree>
#         -DBINARY_DIR=<build tree> -P copilot_includes.cmake
#
# Each source is preprocessed again as the build compiled it - with the stage
# as its only include directory - and the compiler lists what it opened: -M
# for GCC and Clang, /showIncludes for MSVC and clang-cl. See
# cmake/Copilot.cmake.

cmake_minimum_required(VERSION 3.28)
# The build's own settings, where it passes them in a file (Copilot.cmake).
if(DEFINED SETTINGS)
    include("${SETTINGS}")
endif()

string(REPLACE "|" ";" _sources "${SOURCES}")
file(REAL_PATH "${STAGE}" _stage)
file(REAL_PATH "${SOURCE_DIR}" _source_dir)
file(REAL_PATH "${BINARY_DIR}" _binary_dir)
set(_refused "")
set(_opened 0)
foreach(_s IN LISTS _sources)
    if(CXX_STYLE STREQUAL "msvc")
        execute_process(COMMAND "${CXX}" /nologo /Zs /std:c++20 /EHsc /showIncludes /I "${STAGE}" "${_s}"
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        string(REGEX MATCHALL "Note: including file: *[^\r\n]+" _notes "${_out}${_err}")
        set(_files "")
        foreach(_note IN LISTS _notes)
            string(REGEX REPLACE "^Note: including file: *" "" _file "${_note}")
            list(APPEND _files "${_file}")
        endforeach()
    else()
        execute_process(COMMAND "${CXX}" -std=c++20 -M -I "${STAGE}" "${_s}"
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        # A make rule: "target: dep dep \" over several lines.
        string(REGEX REPLACE "^[^:]*:" "" _deps "${_out}")
        string(REPLACE "\\\n" " " _deps "${_deps}")
        string(REGEX REPLACE "[ \t\r\n]+" ";" _files "${_deps}")
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${_s} would not preprocess:\n${_out}${_err}")
    endif()
    foreach(_file IN LISTS _files)
        if(_file STREQUAL "" OR _file STREQUAL "${_s}")
            continue()
        endif()
        math(EXPR _opened "${_opened} + 1")
        file(REAL_PATH "${_file}" _real)
        cmake_path(IS_PREFIX _stage "${_real}" NORMALIZE _in_stage)
        cmake_path(IS_PREFIX _source_dir "${_real}" NORMALIZE _in_source)
        cmake_path(IS_PREFIX _binary_dir "${_real}" NORMALIZE _in_binary)
        if(NOT _in_stage AND (_in_source OR _in_binary))
            string(APPEND _refused "  ${_s}\n    opened ${_real}\n")
        endif()
    endforeach()
endforeach()
if(_refused)
    message(FATAL_ERROR
        "the copilot opened what its stage does not show:\n${_refused}"
        "It may open only the headers in its stage. See cmake/Copilot.cmake.")
endif()
if(_opened EQUAL 0)
    message(FATAL_ERROR "the copilot's sources opened nothing at all: the compiler listed nothing")
endif()
message(STATUS "glideslope: the copilot's sources opened ${_opened} files, none past its stage")

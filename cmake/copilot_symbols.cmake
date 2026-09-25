# copilot_symbols.cmake - every name of glideslope's in an archive or object's
# symbols, defined or called, must be the copilot's own or on NAMES.
#
#   cmake -DNM=<nm or dumpbin> [-DNM_STYLE=<nm|dumpbin>] -DARCHIVE=<file>
#         -DNAMES=<part::name|part::name...> -P copilot_symbols.cmake
#
# The symbols are read demangled - `nm -C`, or `dumpbin /SYMBOLS`, which puts
# each one's undecorated name beside it - and every `glideslope::PART::NAME`
# in each (a function's own name, its arguments' types, a template's) is
# looked up. Anything of JSBSim's is refused outright. A symbol mangled by hand is demangled here like any other, so an
# `extern "C"` declaration of a simulation function under its mangled name is
# caught too. See cmake/Copilot.cmake.

cmake_minimum_required(VERSION 3.28)
# The build's own settings, where it passes them in a file (Copilot.cmake).
if(DEFINED SETTINGS)
    include("${SETTINGS}")
endif()

if(NM_STYLE STREQUAL "dumpbin")
    execute_process(COMMAND "${NM}" /NOLOGO /SYMBOLS "${ARCHIVE}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _symbols ERROR_VARIABLE _err)
else()
    execute_process(COMMAND "${NM}" -C "${ARCHIVE}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _symbols ERROR_VARIABLE _err)
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${NM} could not read ${ARCHIVE}: ${_err}")
endif()
string(REPLACE "|" ";" _allowed "${NAMES}")
# Lines of nm's output; CMake's list characters kept out of the way.
string(REPLACE ";" "@GS_SEMI@" _symbols "${_symbols}")
string(REPLACE "[" "@GS_OPEN@" _symbols "${_symbols}")
string(REPLACE "]" "@GS_CLOSE@" _symbols "${_symbols}")
string(REPLACE "\n" ";" _lines "${_symbols}")
set(_refused "")
set(_seen 0)
foreach(_line IN LISTS _lines)
    if(_line MATCHES "JSBSim::")
        string(APPEND _refused "  ${_line}\n    <- names JSBSim's own\n")
        continue()
    endif()
    string(REGEX MATCHALL "glideslope::[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*" _named "${_line}")
    foreach(_name IN LISTS _named)
        math(EXPR _seen "${_seen} + 1")
        string(REGEX REPLACE "^glideslope::" "" _short "${_name}")
        if(_short MATCHES "^copilot::")
            continue()
        endif()
        if(NOT _short IN_LIST _allowed)
            string(REPLACE "@GS_SEMI@" ";" _shown "${_line}")
            string(REPLACE "@GS_OPEN@" "[" _shown "${_shown}")
            string(REPLACE "@GS_CLOSE@" "]" _shown "${_shown}")
            string(STRIP "${_shown}" _shown)
            string(APPEND _refused "  ${_shown}\n    <- names ${_short}\n")
            break()
        endif()
    endforeach()
endforeach()
if(_refused)
    message(FATAL_ERROR
        "${ARCHIVE} defines or calls what the copilot may not:\n${_refused}"
        "The copilot may define or call only its own names and those in "
        "GLIDESLOPE_COPILOT_NAMES. See cmake/Copilot.cmake.")
endif()
if(_seen EQUAL 0)
    message(FATAL_ERROR "${ARCHIVE} names nothing of glideslope's at all: nm read nothing")
endif()
message(STATUS "glideslope: the copilot's ${_seen} names of glideslope's are all its own or allowed")

# server_flags.cmake - every flag the server prints in its usage is one it
# takes.
#
#   cmake -DSERVER=<glideslope_server> -P server_flags.cmake
#
# **Why this exists.** A flag is added in three places: the usage text, the
# parser and the settings block. Adding it to two of the three leaves a flag
# that is documented and silently ignored, which is worse than one that does
# not exist - `--plain` was written that way and nothing noticed until it was
# tried by hand. This walks the usage text itself, so a flag added to it
# without a parser fails here.
#
# **The whole space is the usage text**, and this says how big it is and
# checks that number, so a flag that stopped being listed fails too.
#
# Each flag is run alone, followed by `--dry-run`. A flag that wants a value
# takes `--dry-run` for its value and complains about the value, which is
# still proof the flag is known; a flag that wants none leaves `--dry-run` to
# do its work. The one thing that fails is the parser saying it has never
# heard of it.

cmake_minimum_required(VERSION 3.28)

execute_process(COMMAND "${SERVER}" --help
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _usage ERROR_VARIABLE _err)
if(_usage STREQUAL "")
    set(_usage "${_err}")
endif()
if(_usage STREQUAL "")
    message(FATAL_ERROR "the server printed no usage at all")
endif()

# Every --flag in it, in the order they appear, without repeats.
string(REGEX MATCHALL "--[a-z][a-z-]+" _found "${_usage}")
set(_flags "")
foreach(_flag IN LISTS _found)
    if(NOT _flag IN_LIST _flags)
        list(APPEND _flags "${_flag}")
    endif()
endforeach()
list(LENGTH _flags _count)

# The flags REQUIREMENTS.md 6.6 and 8 name, plus the ones the server grew to
# fly aircraft for a test. Written out so that this test fails when the set
# changes, rather than quietly walking a smaller one.
# --window and its three test flags, --window-dump, --window-shot and
# --window-press, draw the dashboard in an SDL window and read it back
# (cmake/server_window.cmake).
set(_expected --headless --players --port --store --key --timeout --plain
              --seconds --dry-run --data --ai --plan --fly --on-leave --version
              --help --window --window-dump --window-shot --window-press)
list(LENGTH _expected _wanted)
foreach(_flag IN LISTS _expected)
    if(NOT _flag IN_LIST _flags)
        message(FATAL_ERROR "the usage no longer lists ${_flag}")
    endif()
endforeach()
foreach(_flag IN LISTS _flags)
    if(NOT _flag IN_LIST _expected)
        message(FATAL_ERROR "the usage lists ${_flag}, which this test does not "
                            "know about: add it here and say what it is for")
    endif()
endforeach()
if(NOT _count EQUAL _wanted)
    message(FATAL_ERROR "the usage lists ${_count} flags and ${_wanted} were "
                        "expected")
endif()

set(_walked 0)
foreach(_flag IN LISTS _flags)
    # Two flags print something and stop, and are only themselves: they take
    # no other argument, so they are run alone.
    if(_flag STREQUAL "--help" OR _flag STREQUAL "--version")
        execute_process(COMMAND "${SERVER}" "${_flag}"
                        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "${_flag} exited ${_rc}:\n${_err}")
        endif()
        if(_flag STREQUAL "--help" AND NOT _out MATCHES "usage")
            message(FATAL_ERROR "--help printed no usage")
        endif()
        if(_flag STREQUAL "--version" AND NOT _out MATCHES "[0-9]")
            message(FATAL_ERROR "--version printed no version: ${_out}")
        endif()
        math(EXPR _walked "${_walked} + 1")
        continue()
    endif()
    execute_process(COMMAND "${SERVER}" "${_flag}" --dry-run
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(_err MATCHES "there is no option")
        message(FATAL_ERROR "the usage lists ${_flag} and the parser has never "
                            "heard of it:\n${_err}")
    endif()
    math(EXPR _walked "${_walked} + 1")
endforeach()

if(NOT _walked EQUAL _count)
    message(FATAL_ERROR "only ${_walked} of ${_count} flags were tried")
endif()
message(STATUS "all ${_walked} flags in the server's usage are flags it takes")

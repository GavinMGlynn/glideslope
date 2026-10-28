# test_server_timeouts.cmake - every `--timeout` a test gives a server is long
# enough for a slow machine, unless the test is about the silence.
#
#   cmake -DTESTS=<the tests directory> -DMIN=<seconds> -P test_server_timeouts.cmake
#
# **Why**: `--timeout` is the wall-clock silence after which the server lets
# a client go. On a loaded CI runner a debug client goes quiet for five
# seconds - preparing its ground, say - and was let go; the test then failed
# for a reason it was not testing (run 40660270759, Ubuntu linux-debug: the
# server logged "let go ... after 5.0 s of silence" and the client's glide was
# half done; a STOVL test beside 40 busy loops went the same way). A client
# that leaves says goodbye and is let go at once, so a long timeout costs a
# test nothing; only a test that needs a client let go *for silence* needs a
# short one.
#
# **What it walks**: every `--timeout VALUE` in the scripts under
# tests/cmake/ and in tests/CMakeLists.txt, outside comments, where VALUE is
# a number or a `${variable}` the same file sets to numbers (each number it
# sets is a timeout of its own). A `--timeout` followed by anything else is a
# flag name in a list or a pattern to match, not a timeout.
#
# **The rule**: each is MIN seconds or more, or is in the named list below,
# with the reason its test needs a short one. The list is checked back: an
# exception whose file no longer gives a timeout under MIN fails, so it
# cannot go stale, and the counts are stated.

cmake_minimum_required(VERSION 3.28)

foreach(_v TESTS MIN)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "test_server_timeouts.cmake needs -D${_v}")
    endif()
endforeach()

# **The named exceptions**: the file, then why its test needs a short timeout.
# Every timeout under MIN in the file is covered by the one entry.
set(_exceptions
    "cmake/client_joins_again.cmake|a client let go for its silence must notice and join again by itself"
    "cmake/client_kept_while_away.cmake|a window kept away longer than the timeout must not be let go: the timeout is what it outlasts"
    "cmake/client_rides_along.cmake|a client standing still for five seconds must not be let go by a server that lets a silent one go after three"
    "cmake/server_behind.cmake|a server made a quarter of real time must hear every client within the timeout, or let it go: the short one is the situation"
    "cmake/server_initiation_again.cmake|a silent client waits to be let go for silence, then its initiation is replayed"
    "cmake/server_initiation_replayed_while_live.cmake|a session is let go for silence while its initiation is replayed beside a live one"
    "cmake/server_joins_again.cmake|a client let go for its silence joins again by itself"
    "cmake/server_leave.cmake|the silence way: a client that leaves without goodbye is let go only by the timeout"
    "cmake/server_stale_rejoin.cmake|a client let go with its updates held joins again against a session the server has let go"
    "cmake/server_timeout.cmake|it pins the timeout itself: a silent client is let go after the time given"
    "CMakeLists.txt|--dry-run on the server's own arguments: it parses 2.5 and refuses 0, and listens on nothing, so no client waits")

set(_files "${TESTS}/CMakeLists.txt")
file(GLOB _scripts "${TESTS}/cmake/*.cmake")
list(SORT _scripts)
list(APPEND _files ${_scripts})

set(_total 0)
set(_long 0)
set(_short 0)
set(_short_files "")
set(_problems "")
foreach(_file IN LISTS _files)
    if(_file STREQUAL "${CMAKE_CURRENT_LIST_FILE}")
        continue()
    endif()
    file(RELATIVE_PATH _rel "${TESTS}" "${_file}")
    file(STRINGS "${_file}" _lines)
    set(_values "")
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "^[ \t]*#")
            continue()
        endif()
        string(REGEX MATCHALL "--timeout[ \t]+([0-9][0-9.]*|\\\${[A-Za-z0-9_]+})" _uses "${_line}")
        foreach(_use IN LISTS _uses)
            string(REGEX REPLACE "^--timeout[ \t]+" "" _value "${_use}")
            if(_value MATCHES "^\\\${([A-Za-z0-9_]+)}$")
                set(_name "${CMAKE_MATCH_1}")
                set(_set "")
                foreach(_l IN LISTS _lines)
                    if(_l MATCHES "^[ \t]*set\\(${_name}[ \t]+([0-9][0-9.]*)\\)")
                        list(APPEND _set "${CMAKE_MATCH_1}")
                    endif()
                endforeach()
                if(NOT _set)
                    list(APPEND _problems
                         "${_rel}: --timeout \${${_name}} and the file sets it to no number this can read")
                endif()
                list(APPEND _values ${_set})
            else()
                list(APPEND _values "${_value}")
            endif()
        endforeach()
    endforeach()
    set(_short_here 0)
    foreach(_value IN LISTS _values)
        math(EXPR _total "${_total} + 1")
        if(_value LESS ${MIN})
            math(EXPR _short "${_short} + 1")
            math(EXPR _short_here "${_short_here} + 1")
        else()
            math(EXPR _long "${_long} + 1")
        endif()
    endforeach()
    if(_short_here GREATER 0)
        set(_named FALSE)
        foreach(_e IN LISTS _exceptions)
            if(_e MATCHES "^([^|]+)\\|")
                if(CMAKE_MATCH_1 STREQUAL _rel)
                    set(_named TRUE)
                endif()
            endif()
        endforeach()
        string(REPLACE ";" ", " _values_text "${_values}")
        if(NOT _named)
            list(APPEND _problems
                 "${_rel}: gives a server --timeout under ${MIN} s (${_values_text}) and is not a named exception")
        endif()
        list(APPEND _short_files "${_rel}")
    endif()
endforeach()

# The list, checked back.
set(_named_count 0)
foreach(_e IN LISTS _exceptions)
    math(EXPR _named_count "${_named_count} + 1")
    string(REGEX REPLACE "\\|.*$" "" _rel "${_e}")
    if(NOT _rel IN_LIST _short_files)
        list(APPEND _problems
             "${_rel}: named as an exception and gives no --timeout under ${MIN} s: remove it from the list")
    endif()
endforeach()

if(_problems)
    list(JOIN _problems "\n  " _all)
    message(FATAL_ERROR "the tests' server timeouts:\n  ${_all}")
endif()
math(EXPR _sum "${_long} + ${_short}")
if(NOT _total EQUAL _sum OR _total LESS 40)
    message(FATAL_ERROR "walked ${_total} timeouts, ${_long} long and ${_short} short: "
                        "fewer than expected, or the counts do not add up")
endif()
message(STATUS "walked ${_total} server --timeout uses: ${_long} at ${MIN} s or more, "
               "${_short} under it in ${_named_count} named exception files, none unnamed")

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
# **What it walks**: every non-comment `--timeout` in the scripts under
# tests/cmake/ and in tests/CMakeLists.txt, counted in any form. Those read as
# a use are `--timeout VALUE` with VALUE a number, or a `${variable}` the same
# file sets to numbers (each number it sets is a timeout of its own). Every
# other occurrence - `--timeout=5`, a value on the next line, a flag name in a
# list, a pattern to match - is not read, and must be one of the named
# non-value occurrences below, or the check fails: a form this cannot read is
# never passed over.
#
# **The rule**: each use is MIN seconds or more, or is one of the named short
# uses below, exactly: file, value and how many times, with the reason its test
# needs a short one. A new short timeout in a file that has some fails, as does
# a named one that is gone. The totals are asserted exactly, not as a floor.

cmake_minimum_required(VERSION 3.28)

foreach(_v TESTS MIN)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "test_server_timeouts.cmake needs -D${_v}")
    endif()
endforeach()

# **The totals**, changed only deliberately, with the list below.
set(_expect_uses 60)
set(_expect_long 48)
set(_expect_short 12)
set(_expect_nonvalue 4)

# **The named short uses**: "file|value|times|reason".
set(_short_uses
    "cmake/client_joins_again.cmake|3|2|a client let go for its silence must notice and join again by itself (two servers)"
    "cmake/client_kept_while_away.cmake|2|1|a window kept away longer than the timeout must not be let go: the timeout is what it outlasts"
    "cmake/server_behind.cmake|3|1|a server made a quarter of real time must hear every client within the timeout, or let it go: the short one is the situation"
    "cmake/server_initiation_again.cmake|3|1|a silent client waits to be let go for silence, then its initiation is replayed"
    "cmake/server_initiation_replayed_while_live.cmake|6|1|a session is let go for silence while its initiation is replayed beside a live one"
    "cmake/server_joins_again.cmake|3|1|a client let go for its silence joins again by itself"
    "cmake/server_leave.cmake|1|1|the silence way: a client that leaves without goodbye is let go only by the timeout"
    "cmake/server_stale_rejoin.cmake|3|1|a client let go with its updates held joins again against a session the server has let go"
    "cmake/server_timeout.cmake|0.5|1|it pins the timeout itself: a silent client is let go after the time given"
    "CMakeLists.txt|2.5|1|--dry-run on the server's own arguments: parsed, and listens on nothing, so no client waits"
    "CMakeLists.txt|0|1|--dry-run refusing a timeout of nothing: listens on nothing, so no client waits")

# **The named non-value occurrences**: "file|text the line holds|what it is".
set(_nonvalue
    "cmake/server_flags.cmake|set(_expected --headless|the list of flag names the usage must hold"
    "cmake/server_timeout.cmake|--timeout does|words in a failure message"
    "CMakeLists.txt|--timeout SECONDS|the usage line the server prints, as a pattern to match"
    "CMakeLists.txt|--timeout is 0|the refusal the server prints for a timeout of nothing, as a pattern to match")

set(_files "${TESTS}/CMakeLists.txt")
file(GLOB _scripts "${TESTS}/cmake/*.cmake")
list(SORT _scripts)
list(APPEND _files ${_scripts})

set(_uses 0)
set(_long 0)
set(_short 0)
set(_occurrences 0)
set(_read 0)
set(_named_nonvalue_seen 0)
set(_short_seen "")        # "file|value" once per use
set(_nonvalue_seen "")
set(_problems "")
foreach(_file IN LISTS _files)
    if(_file STREQUAL "${CMAKE_CURRENT_LIST_FILE}")
        continue()
    endif()
    file(RELATIVE_PATH _rel "${TESTS}" "${_file}")
    file(STRINGS "${_file}" _lines)
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "^[ \t]*#")
            continue()
        endif()
        string(REGEX MATCHALL "--timeout" _all "${_line}")
        list(LENGTH _all _n_all)
        if(_n_all EQUAL 0)
            continue()
        endif()
        math(EXPR _occurrences "${_occurrences} + ${_n_all}")
        string(REGEX MATCHALL "--timeout[ \t]+([0-9][0-9.]*|\\\${[A-Za-z0-9_]+})" _found "${_line}")
        list(LENGTH _found _n_read)
        math(EXPR _read "${_read} + ${_n_read}")
        foreach(_use IN LISTS _found)
            string(REGEX REPLACE "^--timeout[ \t]+" "" _value "${_use}")
            if(_value MATCHES "^\\\${([A-Za-z0-9_]+)}$")
                set(_name "${CMAKE_MATCH_1}")
                set(_values "")
                foreach(_l IN LISTS _lines)
                    if(_l MATCHES "^[ \t]*set\\(${_name}[ \t]+([0-9][0-9.]*)\\)")
                        list(APPEND _values "${CMAKE_MATCH_1}")
                    endif()
                endforeach()
                if(NOT _values)
                    list(APPEND _problems
                         "${_rel}: --timeout \${${_name}} and the file sets it to no number this can read")
                endif()
            else()
                set(_values "${_value}")
            endif()
            foreach(_v IN LISTS _values)
                math(EXPR _uses "${_uses} + 1")
                if(_v LESS ${MIN})
                    math(EXPR _short "${_short} + 1")
                    list(APPEND _short_seen "${_rel}|${_v}")
                else()
                    math(EXPR _long "${_long} + 1")
                endif()
            endforeach()
        endforeach()
        # What was not read as a use must be named.
        math(EXPR _unread "${_n_all} - ${_n_read}")
        if(_unread GREATER 0)
            set(_matched 0)
            foreach(_nv IN LISTS _nonvalue)
                string(REPLACE "|" ";" _parts "${_nv}")
                list(GET _parts 0 _nv_file)
                list(GET _parts 1 _nv_text)
                string(FIND "${_line}" "${_nv_text}" _at)
                if(_nv_file STREQUAL _rel AND NOT _at EQUAL -1)
                    math(EXPR _matched "${_matched} + ${_unread}")
                    list(APPEND _nonvalue_seen "${_nv_file}|${_nv_text}")
                endif()
            endforeach()
            if(NOT _matched EQUAL _unread)
                string(STRIP "${_line}" _shown)
                list(APPEND _problems
                     "${_rel}: a --timeout this cannot read as a use and no named non-value occurrence explains: ${_shown}")
            endif()
        endif()
    endforeach()
endforeach()

# **The named short uses, exactly**: every short use is named, as many times as
# it occurs, and every name is used as many times as it says.
set(_named_short 0)
set(_named_files "")
foreach(_s IN LISTS _short_uses)
    string(REPLACE "|" ";" _parts "${_s}")
    list(GET _parts 0 _f)
    list(GET _parts 1 _v)
    list(GET _parts 2 _times)
    set(_have 0)
    foreach(_seen IN LISTS _short_seen)
        if(_seen STREQUAL "${_f}|${_v}")
            math(EXPR _have "${_have} + 1")
        endif()
    endforeach()
    if(NOT _have EQUAL _times)
        list(APPEND _problems
             "${_f}: named with --timeout ${_v} ${_times} time(s) and it has ${_have}")
    endif()
    math(EXPR _named_short "${_named_short} + ${_times}")
    list(APPEND _named_files "${_f}")
endforeach()
list(REMOVE_DUPLICATES _named_files)
list(LENGTH _named_files _n_named_files)
foreach(_seen IN LISTS _short_seen)
    set(_named FALSE)
    foreach(_s IN LISTS _short_uses)
        string(REPLACE "|" ";" _parts "${_s}")
        list(GET _parts 0 _f)
        list(GET _parts 1 _v)
        if(_seen STREQUAL "${_f}|${_v}")
            set(_named TRUE)
        endif()
    endforeach()
    if(NOT _named)
        string(REPLACE "|" " --timeout " _text "${_seen}")
        list(APPEND _problems
             "${_text}: under ${MIN} s and not a named short use")
    endif()
endforeach()
foreach(_nv IN LISTS _nonvalue)
    string(REPLACE "|" ";" _parts "${_nv}")
    list(GET _parts 0 _f)
    list(GET _parts 1 _t)
    if(NOT "${_f}|${_t}" IN_LIST _nonvalue_seen)
        list(APPEND _problems "${_f}: names a non-value --timeout holding '${_t}' and has none")
    endif()
endforeach()
list(LENGTH _nonvalue_seen _n_nonvalue)

# **The counts**: every occurrence is a use read or a named non-value one, and
# the totals are the ones stated above.
math(EXPR _accounted "${_read} + ${_n_nonvalue}")
if(NOT _occurrences EQUAL _accounted)
    list(APPEND _problems
         "${_occurrences} occurrences of --timeout and ${_accounted} accounted for")
endif()
foreach(_c uses long short nonvalue)
    set(_got ${_${_c}})
    if(_c STREQUAL "nonvalue")
        set(_got ${_n_nonvalue})
    endif()
    if(NOT _got EQUAL _expect_${_c})
        list(APPEND _problems "${_got} ${_c} timeouts, not the ${_expect_${_c}} this check states: "
                              "change the stated totals with the lists, deliberately")
    endif()
endforeach()
if(NOT _named_short EQUAL _short)
    list(APPEND _problems "${_short} short uses and ${_named_short} named")
endif()

if(_problems)
    list(JOIN _problems "\n  " _all)
    message(FATAL_ERROR "the tests' server timeouts:\n  ${_all}")
endif()
message(STATUS "walked ${_occurrences} occurrences of --timeout: ${_uses} server timeouts, ${_long} at "
               "${MIN} s or more, ${_short} under it, each named exactly (${_n_named_files} files, "
               "none unnamed); ${_n_nonvalue} more are not timeouts and are named too")

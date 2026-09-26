# test_ports.cmake - every port a test listens on lies in the tests' block,
# outside every platform's ephemeral range, and no two tests share one.
#
#   cmake -DCTEST=<ctest> -DBUILD=<build dir> -DCONFIG=<config>
#         -DFIRST=<first port of the block> -DLAST=<last port of the block>
#         -P test_ports.cmake
#
# **Why**: a fixed port inside the range the system hands out for sockets
# that ask for none can be taken by a client before its test's server
# listens on it - "cannot listen on port 47853" on CI, 2026-09-26, when the
# tests' ports were 478xx and Linux hands out 32768 to 60999. macOS and
# Windows hand out 49152 to 65535. The block is stated once, in
# tests/CMakeLists.txt, and this checks it and every port against both.
#
# **What it walks** is every test this build has, as ctest itself lists them
# (`--show-only=json-v1`), not a list kept by hand:
#
#   - a test given `-DPORT=N` claims N, and N + K for every `${PORT} + K` or
#     `K + ${PORT}` its script derives in a math() (a relay in front of the
#     server, say); any other set() or math() on the port, or on a variable
#     derived from it, is one this cannot read, and fails;
#   - a -D definition naming a port other than PORT fails, rather than being
#     passed over;
#   - a test whose script uses `${PORT}` must have been given one: the tests
#     given a port and the tests whose script needs one are counted apart, and
#     the two counts must agree, test for test;
#   - a test whose command, or the ARGS it hands a script to run, has the
#     word `--port` followed by N listens on N, unless a word of it is also
#     `--dry-run`, which reads its arguments and listens on nothing: those are
#     named as left out, with that reason. A pattern to match (a
#     -D..._MATCHES) is not a command line and is not read.
#
# A test that asks for port 0 is given one by the system and needs no block;
# none of those is counted here.

foreach(_v CTEST BUILD FIRST LAST)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "test_ports.cmake needs -D${_v}")
    endif()
endforeach()

# **The block itself**: above the well-known ports (0 to 1023) and below the
# lowest ephemeral port of any platform (Linux's 32768).
set(_ephemeral_first 32768)
if(FIRST LESS_EQUAL 1023 OR LAST GREATER_EQUAL _ephemeral_first OR FIRST GREATER LAST)
    message(FATAL_ERROR "the tests' port block ${FIRST}-${LAST} is not inside 1024-32767")
endif()

set(_config_args "")
if(CONFIG)
    set(_config_args -C "${CONFIG}")
endif()
execute_process(
    COMMAND "${CTEST}" --test-dir "${BUILD}" ${_config_args} --show-only=json-v1
    OUTPUT_VARIABLE _json ERROR_VARIABLE _err RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "ctest could not list the tests (${_rc}): ${_err}")
endif()
string(JSON _count LENGTH "${_json}" tests)
if(_count EQUAL 0)
    message(FATAL_ERROR "ctest listed no tests in ${BUILD}")
endif()

set(_claims "")        # "port;test" pairs, flattened as port=test
set(_given 0)          # tests given -DPORT
set(_needing 0)        # tests whose script uses ${PORT}
set(_left_out "")      # tests that name a port and listen on nothing
set(_problems "")
math(EXPR _last_index "${_count} - 1")
foreach(_i RANGE ${_last_index})
    # Each test's own entry once: string(JSON) parses all it is given.
    string(JSON _test GET "${_json}" tests ${_i})
    string(JSON _name GET "${_test}" name)
    string(JSON _argc ERROR_VARIABLE _no_command LENGTH "${_test}" command)
    if(_no_command OR _argc EQUAL 0)
        continue()
    endif()
    math(EXPR _last_arg "${_argc} - 1")
    set(_port "")
    set(_script "")
    set(_dry_run FALSE)
    set(_named_ports "")
    # The words a program is run with: each argument of the command, and the
    # words of an ARGS a script is handed to run a program with
    # (expect_run.cmake). A -D<NAME>_MATCHES and the like are patterns, not
    # command lines, and are not read as words.
    set(_words "")
    set(_next_is_script FALSE)
    foreach(_j RANGE ${_last_arg})
        string(JSON _arg GET "${_test}" command ${_j})
        if(_next_is_script)
            set(_script "${_arg}")
            set(_next_is_script FALSE)
        elseif(_arg STREQUAL "-P")
            set(_next_is_script TRUE)
        elseif(_arg MATCHES "^-D([A-Za-z0-9_]*)=(.*)$")
            # string(CONCAT), not set(): a name such as CACHE would be read
            # by set() as its keyword.
            string(CONCAT _var "${CMAKE_MATCH_1}")
            string(CONCAT _value "${CMAKE_MATCH_2}")
            if(_var STREQUAL "PORT")
                set(_port "${_value}")
            elseif(_var MATCHES "PORT")
                list(APPEND _problems
                     "${_name}: -D${_var}= names a port this check does not read")
            elseif(_var STREQUAL "ARGS")
                separate_arguments(_split UNIX_COMMAND "${_value}")
                list(APPEND _words ${_split})
            endif()
        else()
            list(APPEND _words "${_arg}")
        endif()
    endforeach()
    set(_next_is_port FALSE)
    foreach(_w IN LISTS _words)
        if(_next_is_port)
            list(APPEND _named_ports "${_w}")
            set(_next_is_port FALSE)
        elseif(_w STREQUAL "--port")
            set(_next_is_port TRUE)
        elseif(_w MATCHES "^--port=(.*)$")
            list(APPEND _named_ports "${CMAKE_MATCH_1}")
        elseif(_w STREQUAL "--dry-run")
            set(_dry_run TRUE)
        endif()
    endforeach()
    if(_next_is_port)
        list(APPEND _problems "${_name}: --port is its last word, with no port after it")
    endif()

    # A port named on a command line, as the program is run.
    foreach(_named IN LISTS _named_ports)
        if(_dry_run)
            list(APPEND _left_out "${_name} (--port ${_named} with --dry-run: listens on nothing)")
        elseif(NOT _named MATCHES "^[0-9]+$")
            list(APPEND _problems "${_name}: --port ${_named} is not a port this check can read")
        elseif(NOT _named EQUAL 0)
            list(APPEND _claims "${_named}=${_name}")
        endif()
    endforeach()

    set(_script_text "")
    # This script names ${PORT} in its own text, and listens on nothing.
    if(_script AND EXISTS "${_script}" AND NOT _script STREQUAL CMAKE_CURRENT_LIST_FILE)
        file(READ "${_script}" _script_text)
    endif()
    string(FIND "${_script_text}" "\${PORT}" _uses_port)
    if(NOT _uses_port EQUAL -1)
        math(EXPR _needing "${_needing} + 1")
        if(_port STREQUAL "")
            list(APPEND _problems "${_name}: its script uses \${PORT} and it is given none")
        endif()
    endif()
    if(NOT _port STREQUAL "")
        math(EXPR _given "${_given} + 1")
        if(NOT _port MATCHES "^[0-9]+$")
            list(APPEND _problems "${_name}: -DPORT=${_port} is not a number")
            continue()
        endif()
        if(_uses_port EQUAL -1)
            list(APPEND _problems "${_name}: it is given -DPORT=${_port} and its script uses none")
        endif()
        list(APPEND _claims "${_port}=${_name}")
        # **Every port its script derives from it**, read from its set() and
        # math() calls. Two forms are read: `set(V ${PORT})`, a second name
        # for the port, and `math(EXPR V "${PORT} + K")` or "K + ${PORT}",
        # which claims PORT + K. Any other set() or math() that uses ${PORT},
        # or uses a variable so made, is a port this cannot read - a second
        # step from a derived port, or one counted up in a loop - and fails.
        string(REGEX MATCHALL "(set|math)\\([^)]*\\)" _calls "${_script_text}")
        set(_derived_vars "")
        set(_unread "")
        foreach(_call IN LISTS _calls)
            if(NOT _call MATCHES "\\\${PORT}")
                continue()
            endif()
            unset(_k)
            if(_call MATCHES "^set\\(([A-Za-z0-9_]+) +\"?\\\${PORT}\"?\\)$")
                list(APPEND _derived_vars "${CMAKE_MATCH_1}")
                set(_k 0)
            elseif(_call MATCHES "^math\\(EXPR +([A-Za-z0-9_]+) +\"\\\${PORT} *\\+ *([0-9]+)\"\\)$")
                # One regex to an if(), not two joined by OR: CMake evaluates
                # both, and the second's failure empties the first's matches.
                list(APPEND _derived_vars "${CMAKE_MATCH_1}")
                string(CONCAT _k "${CMAKE_MATCH_2}")
            elseif(_call MATCHES "^math\\(EXPR +([A-Za-z0-9_]+) +\"([0-9]+) *\\+ *\\\${PORT}\"\\)$")
                list(APPEND _derived_vars "${CMAKE_MATCH_1}")
                string(CONCAT _k "${CMAKE_MATCH_2}")
            endif()
            if(DEFINED _k)
                if(NOT _k EQUAL 0)
                    math(EXPR _p "${_port} + ${_k}")
                    list(APPEND _claims "${_p}=${_name} (\${PORT} + ${_k})")
                endif()
            else()
                list(APPEND _unread "${_call}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES _derived_vars)
        foreach(_call IN LISTS _calls)
            foreach(_v IN LISTS _derived_vars)
                if(_call MATCHES "\\\${${_v}}")
                    list(APPEND _unread "${_call}")
                endif()
            endforeach()
        endforeach()
        foreach(_call IN LISTS _unread)
            string(REPLACE "\n" " " _call "${_call}")
            list(APPEND _problems "${_name}: its script derives a port this cannot read: ${_call}")
        endforeach()
    endif()
endforeach()

if(NOT _given EQUAL _needing)
    list(APPEND _problems
         "${_given} tests are given a port and ${_needing} tests' scripts use one")
endif()

# **Every claim**: inside the block, and nobody else's.
set(_ports 0)
foreach(_claim IN LISTS _claims)
    string(REGEX MATCH "^[0-9]+" _p "${_claim}")
    string(REGEX REPLACE "^[0-9]+=" "" _who "${_claim}")
    math(EXPR _ports "${_ports} + 1")
    if(_p GREATER_EQUAL _ephemeral_first)
        list(APPEND _problems
             "${_who}: port ${_p} is in an ephemeral range (Linux 32768-60999, macOS and Windows 49152-65535)")
    elseif(_p LESS FIRST OR _p GREATER LAST)
        list(APPEND _problems "${_who}: port ${_p} is outside the tests' block ${FIRST}-${LAST}")
    endif()
    if(DEFINED _owner_${_p})
        list(APPEND _problems "${_who}: port ${_p} is also taken by ${_owner_${_p}}")
    else()
        set(_owner_${_p} "${_who}")
    endif()
endforeach()

foreach(_skip IN LISTS _left_out)
    message(STATUS "left out: ${_skip}")
endforeach()
list(LENGTH _left_out _left)
if(_problems)
    list(JOIN _problems "\n  " _all)
    message(FATAL_ERROR "the tests' ports:\n  ${_all}")
endif()
message(STATUS "walked ${_count} tests: ${_needing} run a script that needs a port and "
               "${_given} are given one; ${_ports} ports claimed, all in ${FIRST}-${LAST} and no two "
               "the same; ${_left} left out as listening on nothing")

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
#     -D..._MATCHES) is not a command line and is not read;
#   - a test whose script uses `${PORT}` holds it machine-wide, through
#     `include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)`, before its first
#     execute_process: two builds on one machine are the same tests on the
#     same ports (port_lock.cmake says what that did on 2026-11-04);
#   - every port it derives from PORT (a relay at PORT + K) it holds too, by
#     `port_lock_hold(${V})` after the math() that makes it and before the
#     next execute_process;
#   - every call in such a script that listens on a held port - a program
#     run with `--port ${V}`, a relay (`"${IMPAIR}" ${V}`), or a script handed
#     a server and the port (-DSERVER= with -DPORT=${PORT}) - is followed by
#     `port_listened(...)`, handed that call's standard error, before any
#     message(FATAL_ERROR) and before the next call that listens: a server
#     that could not listen fails its test as that, not as what its client
#     heard from whatever holds the port. Each script is read once, however
#     many tests run it; a script that uses a port and listens on nothing is
#     named below, with its reason, and one that is not named fails.
#
# A test that asks for port 0 is given one by the system and needs no block;
# none of those is counted here.

cmake_minimum_required(VERSION 3.28)

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
set(_locking 0)        # ... and holds it, through port_lock.cmake, before running anything
set(_left_out "")      # tests that name a port and listen on nothing
set(_read_scripts "")      # scripts whose listening calls have been read, each once
set(_listening_scripts 0)  # ... those with a call that listens on a held port
set(_checked_scripts 0)    # ... and every such call of theirs checked by port_listened
set(_listening_calls 0)
set(_checked_calls 0)
set(_relays 0)             # ports a script derives from PORT, script by script
set(_relays_held 0)        # ... and holds before it runs anything on them
# **Scripts that use a port and listen on nothing**, with the reason. Any
# other script that uses a port and has no call listening on it fails.
set(_listen_on_nothing
    "frame_terrain_imagery_refused.cmake=its PORT is one nobody listens on, so every fetch is refused"
    "imagery_stale_refused.cmake=its PORT is one nobody listens on, so every fetch is refused")
# **Tests that listen and were not seen to fail as "cannot listen"** with
# every port held (2026-11-04, docs/status): each skips before it runs
# anything on a development machine, and its script's calls are checked
# here like any other. Each must still be a test given a port.
set(_live "a live model call costs money, made only with GLIDESLOPE_LIVE_MODEL=1")
set(_not_run_held
    "a_players_aircraft_handed_to_the_ai_in_the_air_is_planned_by_chatgpt_and_flown_asking_openai_now=${_live}"
    "a_players_aircraft_handed_to_the_ai_in_the_air_is_planned_by_claude_and_flown_asking_anthropic_now=${_live}"
    "a_players_copilot_asked_on_the_players_machine_is_flown_by_the_server_asking_anthropic_now=${_live}"
    "a_players_copilot_glides_its_aircraft_on_a_server_when_the_engine_stops_asking_anthropic_now=${_live}"
    "a_players_copilot_glides_when_the_engine_stops_while_it_is_thinking_asking_anthropic_now=${_live}"
    "a_players_copilot_stands_by_when_its_pilot_takes_the_aircraft_back_asking_anthropic_now=${_live}"
    "an_aircraft_left_by_a_player_who_goes_is_planned_in_the_air_by_the_servers_claude_asking_anthropic_now=${_live}"
    "an_aircraft_left_by_a_take_over_is_planned_in_the_air_by_the_servers_chatgpt_asking_openai_now=${_live}"
    "the_servers_window_shows_the_terminals_dashboard_and_its_drop_button_drops_a_player=it opens a window, and WSL has no display to run it headless on")
set(_given_names "")
set(_problems "")
# **Every call that listens on a held port, checked first**: reads one
# script's calls, and queues in _rl_queue each script it runs on its port
# (a script handed a server), to be read in turn. A call is the text from
# its execute_process( to the first line it closes; what follows it, up to
# the next call that listens, must call port_listened before it judges
# anything. A macro, so that its counts and problems are the caller's.
macro(_read_listeners _rl_script)
    get_filename_component(_script_name "${_rl_script}" NAME)
    file(READ "${_rl_script}" _rl_text)
    # The ports it holds: PORT, and each name it gives one derived from it.
    string(REGEX MATCHALL "(set|math)\\(([A-Za-z0-9_]+|EXPR +[A-Za-z0-9_]+) +\"?\\\${PORT}[^)]*\\)"
           _rl_derivations "${_rl_text}")
    set(_held_vars PORT)
    foreach(_rl_d IN LISTS _rl_derivations)
        if(_rl_d MATCHES "^(set\\(|math\\(EXPR +)([A-Za-z0-9_]+)")
            list(APPEND _held_vars "${CMAKE_MATCH_2}")
        endif()
    endforeach()
    list(JOIN _held_vars "|" _held_alt)
    set(_listens "")       # where each call that listens starts
    set(_listens_end "")   # ... and ends
    set(_listens_err "")   # ... and the variable it catches stderr in, or -
    set(_offset 0)
    set(_rest "${_rl_text}")
    while(TRUE)
        string(FIND "${_rest}" "execute_process(" _at)
        if(_at EQUAL -1)
            break()
        endif()
        math(EXPR _offset "${_offset} + ${_at}")
        string(SUBSTRING "${_rest}" ${_at} -1 _rest)
        string(FIND "${_rest}" ")\n" _end)
        if(_end EQUAL -1)
            string(LENGTH "${_rest}" _end)
        endif()
        string(SUBSTRING "${_rest}" 0 ${_end} _run)
        set(_listening FALSE)
        if(_run MATCHES "--port +\\\${(${_held_alt})}")
            set(_listening TRUE)
        elseif(_run MATCHES "\\\${IMPAIR}\" +\\\${(${_held_alt})}")
            set(_listening TRUE)
        elseif(_run MATCHES "-DSERVER=" AND _run MATCHES "-DPORT=\\\${PORT}")
            set(_listening TRUE)
            # The script it runs is read too: its own calls are its server's.
            if(_run MATCHES "-P +\"?\\\${CMAKE_CURRENT_LIST_DIR}/([A-Za-z0-9_]+\\.cmake)")
                get_filename_component(_rl_dir "${_rl_script}" DIRECTORY)
                list(APPEND _rl_queue "${_rl_dir}/${CMAKE_MATCH_1}")
            else()
                list(APPEND _problems "${_script_name}: it runs a script on its port that "
                                      "this cannot name, to read its calls")
            endif()
        endif()
        if(_listening)
            math(EXPR _ends_at "${_offset} + ${_end}")
            list(APPEND _listens ${_offset})
            list(APPEND _listens_end ${_ends_at})
            if(_run MATCHES "ERROR_VARIABLE +([A-Za-z0-9_]+)")
                list(APPEND _listens_err "${CMAKE_MATCH_1}")
            else()
                list(APPEND _listens_err "-")
            endif()
        endif()
        string(SUBSTRING "${_rest}" 16 -1 _rest)
        math(EXPR _offset "${_offset} + 16")
    endwhile()
    list(LENGTH _listens _calls_here)
    set(_named_reason "")
    foreach(_named_script IN LISTS _listen_on_nothing)
        if(_named_script MATCHES "^([^=]+)=(.*)$" AND CMAKE_MATCH_1 STREQUAL _script_name)
            set(_named_reason "${CMAKE_MATCH_2}")
        endif()
    endforeach()
    if(_calls_here EQUAL 0)
        if(_named_reason STREQUAL "")
            list(APPEND _problems
                 "${_script_name}: it uses \${PORT} and this finds no call listening on "
                 "it - name it as listening on nothing, with its reason, or teach "
                 "this check its listener")
        else()
            list(APPEND _left_out "${_script_name} (${_named_reason})")
        endif()
    else()
        if(NOT _named_reason STREQUAL "")
            list(APPEND _problems "${_script_name}: named as listening on nothing, "
                                  "and ${_calls_here} of its calls listen")
        endif()
        math(EXPR _listening_scripts "${_listening_scripts} + 1")
        math(EXPR _listening_calls "${_listening_calls} + ${_calls_here}")
        set(_all_checked TRUE)
        math(EXPR _last_call "${_calls_here} - 1")
        foreach(_c RANGE ${_last_call})
            list(GET _listens_end ${_c} _from)
            list(GET _listens_err ${_c} _err_var)
            math(EXPR _n "${_c} + 1")
            if(_n LESS _calls_here)
                list(GET _listens ${_n} _to)
                math(EXPR _span "${_to} - ${_from}")
                string(SUBSTRING "${_rl_text}" ${_from} ${_span} _after)
            else()
                string(SUBSTRING "${_rl_text}" ${_from} -1 _after)
            endif()
            string(FIND "${_after}" "port_listened(" _check)
            string(FIND "${_after}" "message(FATAL_ERROR" _fatal)
            string(SUBSTRING "${_rl_text}" 0 ${_from} _before)
            string(REGEX MATCHALL "\n" _lines "${_before}")
            list(LENGTH _lines _line)
            math(EXPR _line "${_line} + 1")
            if(_check EQUAL -1 OR (NOT _fatal EQUAL -1 AND _fatal LESS _check))
                list(APPEND _problems
                     "${_script_name}:${_line}: a call that listens on a held port is "
                     "not checked by port_listened(...) before anything is judged")
                set(_all_checked FALSE)
                continue()
            endif()
            string(SUBSTRING "${_after}" ${_check} -1 _check_text)
            string(FIND "${_check_text}" ")" _close)
            string(SUBSTRING "${_check_text}" 0 ${_close} _check_text)
            string(FIND "${_check_text}" "\${${_err_var}}" _hands)
            if(_err_var STREQUAL "-")
                list(APPEND _problems
                     "${_script_name}:${_line}: a call that listens on a held port "
                     "catches no standard error for port_listened to read")
                set(_all_checked FALSE)
            elseif(_hands EQUAL -1)
                list(APPEND _problems
                     "${_script_name}:${_line}: port_listened is not handed "
                     "\${${_err_var}}, that call's standard error")
                set(_all_checked FALSE)
            else()
                math(EXPR _checked_calls "${_checked_calls} + 1")
            endif()
        endforeach()
        if(_all_checked)
            math(EXPR _checked_scripts "${_checked_scripts} + 1")
        endif()
    endif()
endmacro()

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
        # **And holds it machine-wide** (port_lock.cmake) before it runs
        # anything: another build's run of the same test is on the same port.
        string(FIND "${_script_text}" "\ninclude(\${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)\n"
               _locks)
        string(FIND "${_script_text}" "execute_process(" _runs)
        if(_locks EQUAL -1)
            list(APPEND _problems "${_name}: its script uses \${PORT} and does not hold it "
                                  "(include port_lock.cmake)")
        elseif(NOT _runs EQUAL -1 AND _runs LESS _locks)
            list(APPEND _problems "${_name}: its script runs a program before it holds its port")
        else()
            math(EXPR _locking "${_locking} + 1")
        endif()
    endif()
    if(NOT _port STREQUAL "")
        math(EXPR _given "${_given} + 1")
        list(APPEND _given_names "${_name}")
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
                    # **Held too**, after this math() and before the next
                    # execute_process: another version's test may use it.
                    list(GET _derived_vars -1 _v)
                    string(FIND "${_script_text}" "${_call}" _at)
                    string(SUBSTRING "${_script_text}" ${_at} -1 _after)
                    string(FIND "${_after}" "port_lock_hold(\${${_v}})" _held)
                    string(FIND "${_after}" "execute_process(" _next_run)
                    if(NOT _script IN_LIST _read_scripts)
                        math(EXPR _relays "${_relays} + 1")
                    endif()
                    if(_held EQUAL -1 OR (NOT _next_run EQUAL -1 AND _next_run LESS _held))
                        list(APPEND _problems "${_name}: its script derives port ${_p} "
                                              "(\${PORT} + ${_k}) and does not hold it, "
                                              "port_lock_hold(\${${_v}}), before it runs anything")
                    elseif(NOT _script IN_LIST _read_scripts)
                        math(EXPR _relays_held "${_relays_held} + 1")
                    endif()
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

        # **Every call that listens on a held port, checked first**, read
        # once per script, and the scripts it runs on its port with it.
        set(_rl_queue "${_script}")
        while(_rl_queue)
            list(POP_FRONT _rl_queue _rl_next)
            if(NOT _rl_next IN_LIST _read_scripts)
                list(APPEND _read_scripts "${_rl_next}")
                _read_listeners("${_rl_next}")
            endif()
        endwhile()
    endif()
endforeach()

if(NOT _locking EQUAL _needing)
    list(APPEND _problems
         "${_needing} tests' scripts use a port and ${_locking} of them hold it first")
endif()
set(_not_held_count 0)
foreach(_entry IN LISTS _not_run_held)
    string(REGEX REPLACE "=.*$" "" _entry_name "${_entry}")
    string(REGEX REPLACE "^[^=]*=" "" _entry_reason "${_entry}")
    if(NOT _entry_name IN_LIST _given_names)
        list(APPEND _problems "${_entry_name}: named as not run held, and no test given a port "
                              "has that name")
    else()
        math(EXPR _not_held_count "${_not_held_count} + 1")
        message(STATUS "not run held: ${_entry_name} (${_entry_reason})")
    endif()
endforeach()
if(NOT _checked_calls EQUAL _listening_calls OR NOT _checked_scripts EQUAL _listening_scripts)
    list(APPEND _problems
         "${_listening_scripts} scripts make ${_listening_calls} calls that listen on a held "
         "port, and ${_checked_scripts} scripts' ${_checked_calls} calls are checked by "
         "port_listened first")
endif()
if(NOT _relays_held EQUAL _relays)
    list(APPEND _problems "scripts derive ${_relays} ports from PORT and hold ${_relays_held} of them")
endif()
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
               "${_given} are given one, ${_locking} holding it machine-wide before they run "
               "anything; ${_relays_held} of ${_relays} ports derived from one held too; "
               "${_listening_scripts} scripts make ${_listening_calls} calls that listen on a "
               "held port, ${_checked_calls} of them, in ${_checked_scripts} scripts, checked "
               "by port_listened before anything is judged; ${_ports} ports claimed, all in "
               "${FIRST}-${LAST} and no two the same; ${_left} left out as listening on nothing; "
               "${_not_held_count} named as not run with their ports held")

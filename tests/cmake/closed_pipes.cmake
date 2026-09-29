# closed_pipes.cmake - every program a test runs in a pipeline carries on when
# the program it writes to has gone.
#
#   cmake -DPIPE=<glideslope_closed_pipe> -DSCRIPTS=<tests/cmake>
#         -DTESTS=<tests/CMakeLists.txt> -D<target>=<its file>...
#         -P closed_pipes.cmake
#
# where each <target> is one in the table below - glideslope_server=..., and
# so on.
#
# **Why.** A test that runs several programs at once does it in one
# execute_process, which pipes each one's standard output into the next one's
# standard input. A client that says goodbye and then prints what it did is
# writing to the server its goodbye just ended; with SIGPIPE at its default
# action the write killed it. On macOS CI, twice (2026-09-28 and 29): exit
# codes "1;0;SIGPIPE;0;0" and "0;0;SIGPIPE;0;0", the client that heard
# everything and left last, in runs that had otherwise gone well. Linux was
# spared only by accident - libcurl's setup, which ignores SIGPIPE, had run in
# each of them first. Windows has no SIGPIPE, so this is not run there.
#
# **Built, not hoped for.** Each program is run by glideslope_closed_pipe with
# its output into a pipe whose reader was closed before it started, and with
# SIGPIPE put back to its default action; it must exit as it does when read.
# Run once read as well, it must have written something, or nothing was tested.
# Its first write is the one tested, and that is enough: ignoring SIGPIPE
# holds for the whole process from main() on, and nothing sets it back.
#
# **Coverage is asserted.** Every execute_process in SCRIPTS with more than
# one COMMAND is read, and every COMMAND in it must be `${VAR}`: a literal
# program fails this. Each VAR is resolved per script, from the
# -DVAR=$<TARGET_FILE:target> of every test in TESTS that runs that script, so
# a name that means different programs in different scripts - CHECK, CLIENT -
# is each of them; one no test resolves fails this. Every target so found must
# be in the table below, or left out with the reason, and every target in the
# table must be found.

cmake_minimum_required(VERSION 3.28)

# The programs: how each is run so that it writes, and the exit code that is.
set(_table glideslope_server glideslope_cli glideslope glideslope_impair
           glideslope_cache_collision glideslope_doc_client glideslope_datagram_check
           glideslope_ion_stall glideslope_http_stub)
set(_run_glideslope_server --version)
set(_exit_glideslope_server 0)
set(_run_glideslope_cli --version)
set(_exit_glideslope_cli 0)
set(_run_glideslope --version)
set(_exit_glideslope 0)
# The tests' own tools; with no arguments each says how it is used, on
# standard error, which is as much a write as any.
set(_exit_glideslope_impair 2)
set(_exit_glideslope_cache_collision 2)
set(_exit_glideslope_doc_client 1)
set(_exit_glideslope_datagram_check 2)
set(_exit_glideslope_ion_stall 2)
set(_exit_glideslope_http_stub 2)
# Left out: cmake itself, which is not this project's to change; and
# glideslope_hold_open, which writes until nobody reads - so read, it never
# stops - and whose whole work is to outlive a closed pipe: its exit of 0 after
# the relay has gone is half of what impair_gives_up.cmake asserts.
set(_left_out CMAKE_COMMAND)
set(_left_out_targets glideslope_hold_open)

# **What each script's variables are**, from the tests that run it.
file(READ "${TESTS}" _rest)
set(_tests 0)
while(TRUE)
    string(FIND "${_rest}" "add_test(" _at)
    if(_at EQUAL -1)
        break()
    endif()
    math(EXPR _at "${_at} + 9")
    string(SUBSTRING "${_rest}" ${_at} -1 _rest)
    string(FIND "${_rest}" "add_test(" _next)
    string(SUBSTRING "${_rest}" 0 ${_next} _test)
    if(NOT _test MATCHES "-P[ \t\n]+\\$\\{CMAKE_CURRENT_SOURCE_DIR\\}/cmake/([a-z_0-9]+)\\.cmake")
        continue()
    endif()
    set(_script "${CMAKE_MATCH_1}")
    math(EXPR _tests "${_tests} + 1")
    string(REGEX MATCHALL "-D[A-Z_]+=\\$<TARGET_FILE:[A-Za-z_0-9]+>" _given "${_test}")
    foreach(_d IN LISTS _given)
        string(REGEX REPLACE "-D([A-Z_]+)=.*" "\\1" _var "${_d}")
        string(REGEX REPLACE ".*TARGET_FILE:([A-Za-z_0-9]+)>" "\\1" _target "${_d}")
        list(APPEND _of_${_script}_${_var} "${_target}")
        list(REMOVE_DUPLICATES _of_${_script}_${_var})
    endforeach()
endwhile()

# **Every program in every pipeline.**
file(GLOB _scripts "${SCRIPTS}/*.cmake")
set(_found "")
set(_pipelines 0)
set(_commands_seen 0)
set(_problems "")
foreach(_path IN LISTS _scripts)
    get_filename_component(_script "${_path}" NAME_WE)
    file(READ "${_path}" _rest)
    # Comments out, so that a COMMAND among them is not read as one.
    string(REGEX REPLACE "#[^\n]*" "" _rest "${_rest}")
    # Each call runs from its name to its first keyword after the commands.
    while(TRUE)
        string(FIND "${_rest}" "execute_process(" _at)
        if(_at EQUAL -1)
            break()
        endif()
        math(EXPR _at "${_at} + 16")
        string(SUBSTRING "${_rest}" ${_at} -1 _rest)
        string(FIND "${_rest}" "execute_process(" _next)
        string(SUBSTRING "${_rest}" 0 ${_next} _call)
        string(REGEX REPLACE
               "(RESULTS?_VARIABLE|OUTPUT_VARIABLE|ERROR_VARIABLE|OUTPUT_FILE|INPUT_FILE|ERROR_FILE|TIMEOUT|WORKING_DIRECTORY).*"
               "" _call "${_call}")
        # Every COMMAND, wherever on its line, and the word after it.
        string(REGEX MATCHALL "(^|[ \t\n(])COMMAND[ \t\n]+[^ \t\n)]+" _commands "${_call}")
        list(LENGTH _commands _count)
        if(_count LESS 2)
            continue()
        endif()
        math(EXPR _pipelines "${_pipelines} + 1")
        foreach(_command IN LISTS _commands)
            math(EXPR _commands_seen "${_commands_seen} + 1")
            string(REGEX REPLACE ".*COMMAND[ \t\n]+" "" _program "${_command}")
            string(REPLACE "\"" "" _program "${_program}")
            if(NOT _program MATCHES "^\\$\\{([A-Za-z_]+)\\}$")
                string(APPEND _problems "\n  ${_script}: a pipeline runs ${_program}, which "
                                        "is not a variable this can resolve")
                continue()
            endif()
            set(_var "${CMAKE_MATCH_1}")
            if(_var IN_LIST _left_out)
                continue()
            endif()
            if(NOT DEFINED _of_${_script}_${_var})
                string(APPEND _problems "\n  ${_script}: no test gives ${_var} as a "
                                        "$<TARGET_FILE:...>, so what it runs is not known")
                continue()
            endif()
            list(APPEND _found ${_of_${_script}_${_var}})
        endforeach()
    endwhile()
endforeach()
list(REMOVE_DUPLICATES _found)
if(_tests LESS 100 OR _pipelines LESS 30)
    message(FATAL_ERROR "found only ${_tests} tests running scripts and ${_pipelines} "
                        "pipelines: the reading is broken, not the tests")
endif()
foreach(_target IN LISTS _found)
    if(NOT _target IN_LIST _table AND NOT _target IN_LIST _left_out_targets)
        string(APPEND _problems "\n  ${_target} is run in a pipeline and not checked here: "
                                "add it to the table, or leave it out with the reason")
    endif()
endforeach()
foreach(_target IN LISTS _table)
    if(NOT _target IN_LIST _found)
        string(APPEND _problems "\n  ${_target} is in the table but in no pipeline: take it out")
    endif()
endforeach()
if(_problems)
    message(FATAL_ERROR "the pipelines are not all covered:${_problems}")
endif()

# **Each of them, with nobody reading.**
list(LENGTH _table _expected)
set(_checked 0)
set(_failed "")
foreach(_target IN LISTS _table)
    if(NOT DEFINED ${_target})
        message(FATAL_ERROR "no -D${_target}=<its file> was given")
    endif()
    execute_process(
        COMMAND "${PIPE}" ${_exit_${_target}} "${${_target}}" ${_run_${_target}}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _said ERROR_VARIABLE _said TIMEOUT 60)
    message(STATUS "${_said}")
    math(EXPR _checked "${_checked} + 1")
    if(NOT _rc EQUAL 0)
        list(APPEND _failed "${_target}")
    endif()
endforeach()
if(NOT _checked EQUAL _expected)
    message(FATAL_ERROR "checked ${_checked} of ${_expected} programs")
endif()
if(_failed)
    message(FATAL_ERROR "died of, or went otherwise with, a pipe nobody read: ${_failed}")
endif()
message(STATUS "${_checked} programs outlive a closed pipe: every one run in the "
               "${_commands_seen} commands of ${_pipelines} pipelines, read from ${_tests} "
               "tests' scripts, but cmake itself and ${_left_out_targets}")

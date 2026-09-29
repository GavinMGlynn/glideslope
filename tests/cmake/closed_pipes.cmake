# closed_pipes.cmake - every program a test runs in a pipeline carries on when
# the program it writes to has gone.
#
#   cmake -DPIPE=<glideslope_closed_pipe> -DSCRIPTS=<tests/cmake>
#         -DSERVER=<glideslope_server> -DCLI=<glideslope_cli> -DWINDOW=<glideslope>
#         -DIMPAIR=<glideslope_impair> -DCACHE_TOOL=<glideslope_cache_collision>
#         -DDOC_CLIENT=<glideslope_doc_client> -DDATAGRAM_CHECK=<glideslope_datagram_check>
#         -DSTALL=<glideslope_ion_stall> -DHTTP_STUB=<glideslope_http_stub>
#         -P closed_pipes.cmake
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
#
# **Coverage is asserted.** Every execute_process in SCRIPTS with more than
# one COMMAND is read, and every program named in one must be in the table
# below - or listed as left out, with the reason. A new program in a pipeline
# that is in neither fails this.

cmake_minimum_required(VERSION 3.28)

# The variable a pipeline names its program by, and what each stands for here:
# the programs, how each is run so that it writes, and the exit code that is.
# CLIENT is glideslope_cli in most scripts and the client with the window in
# some, so it is both. CHECK is the datagram check in the only pipeline that
# has one.
set(_names SERVER CLIENT IMPAIR TOOL DOC_CLIENT CHECK STALL STUB)
set(_SERVER "SERVER")
set(_CLIENT "CLI;WINDOW")
set(_IMPAIR "IMPAIR")
set(_TOOL "CACHE_TOOL")
set(_DOC_CLIENT "DOC_CLIENT")
set(_CHECK "DATAGRAM_CHECK")
set(_STALL "STALL")
set(_STUB "HTTP_STUB")
# Left out: cmake itself, which is not this project's to change.
set(_left_out CMAKE_COMMAND)

set(_run_SERVER --version)
set(_exit_SERVER 0)
set(_run_CLI --version)
set(_exit_CLI 0)
set(_run_WINDOW --version)
set(_exit_WINDOW 0)
# The rest are the tests' own tools; with no arguments each says how it is
# used, on standard error, which is as much a write as any.
set(_exit_IMPAIR 2)
set(_exit_CACHE_TOOL 2)
set(_exit_DOC_CLIENT 1)
set(_exit_DATAGRAM_CHECK 2)
set(_exit_STALL 2)
set(_exit_HTTP_STUB 2)

# **Every program in every pipeline.**
file(GLOB _scripts "${SCRIPTS}/*.cmake")
set(_named "")
set(_pipelines 0)
foreach(_script IN LISTS _scripts)
    file(READ "${_script}" _rest)
    # Each call runs from its name to its first keyword after the commands -
    # not to its closing bracket, since a comment among them can have one.
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
               "(RESULTS?_VARIABLE|OUTPUT_VARIABLE|ERROR_VARIABLE|OUTPUT_FILE|INPUT_FILE|TIMEOUT).*"
               "" _call "${_call}")
        string(REGEX MATCHALL "(^|\n)[ \t]*COMMAND[ \t]+\"?\\$\\{[A-Z_]+\\}" _commands "${_call}")
        list(LENGTH _commands _count)
        if(_count LESS 2)
            continue()
        endif()
        math(EXPR _pipelines "${_pipelines} + 1")
        foreach(_command IN LISTS _commands)
            string(REGEX REPLACE ".*\\$\\{([A-Z_]+)\\}" "\\1" _name "${_command}")
            list(APPEND _named "${_name}")
        endforeach()
    endwhile()
endforeach()
list(REMOVE_DUPLICATES _named)
if(_pipelines LESS 30)
    message(FATAL_ERROR "found only ${_pipelines} pipelines in ${SCRIPTS}: the reading of "
                        "the scripts is broken, not the scripts")
endif()
foreach(_name IN LISTS _named)
    if(NOT _name IN_LIST _names AND NOT _name IN_LIST _left_out)
        message(FATAL_ERROR "${_name} is run in a pipeline and not checked here: add it "
                            "to the table in closed_pipes.cmake, or leave it out with the "
                            "reason")
    endif()
endforeach()
foreach(_name IN LISTS _names)
    if(NOT _name IN_LIST _named)
        message(FATAL_ERROR "${_name} is in the table but in no pipeline: take it out")
    endif()
endforeach()

# **Each of them, with nobody reading.**
set(_programs "")
foreach(_name IN LISTS _names)
    list(APPEND _programs ${_${_name}})
endforeach()
list(REMOVE_DUPLICATES _programs)
list(LENGTH _programs _expected)
set(_checked 0)
set(_failed "")
foreach(_program IN LISTS _programs)
    execute_process(
        COMMAND "${PIPE}" ${_exit_${_program}} "${${_program}}" ${_run_${_program}}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _said ERROR_VARIABLE _said TIMEOUT 60)
    message(STATUS "${_said}")
    math(EXPR _checked "${_checked} + 1")
    if(NOT _rc EQUAL 0)
        list(APPEND _failed "${_program}")
    endif()
endforeach()
if(NOT _checked EQUAL _expected)
    message(FATAL_ERROR "checked ${_checked} of ${_expected} programs")
endif()
if(_failed)
    message(FATAL_ERROR "died of, or went otherwise with, a pipe nobody read: ${_failed}")
endif()
list(REMOVE_ITEM _named ${_left_out})
list(LENGTH _named _in_pipelines)
message(STATUS "${_checked} programs, for the ${_in_pipelines} names in ${_pipelines} "
               "pipelines (cmake itself left out), outlive a closed pipe")

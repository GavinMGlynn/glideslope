# server_leave.cmake - what becomes of an aircraft when the person flying it
# goes: both settings, walked.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data dir>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -P server_leave.cmake
#
# `REQUIREMENTS.md` 6.5 makes this a session setting with two values, and the
# item's verification is "both settings, tested". Both are run here and
# counted, so dropping one fails rather than passing quietly.
#
# **A client leaves in one of two ways, and both are walked with both
# settings**: by saying goodbye, and by going quiet. Either way it stays two
# seconds and stops. Saying goodbye, it is let go at once, with a timeout of a
# minute that it cannot have been let go by; going quiet (`--no-goodbye`), the
# server's `--timeout 1` lets it go. The server outlives it and says what it
# did, and its closing lines say what is still in the sky - the same, whichever
# way the client went.
#
# **Under `ai` the aircraft must have moved.** Handing it to an AI pilot that
# then flew nothing would leave it hanging where its owner left it, and a
# check that only counted aeroplanes would pass.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()

set(_store "${WORK}/leave.sqlite")
file(REMOVE "${_store}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

set(_settings remove ai)
set(_ways goodbye silence)
set(_walked 0)
foreach(_what IN LISTS _settings)
    # Each setting's server on a port of its own, written out so that
    # test_ports.cmake can read which ports this takes: PORT and PORT + 1.
    # Both ways of leaving use it in turn, one server after the other.
    if(_what STREQUAL "remove")
        set(_port ${PORT})
    else()
        math(EXPR _port "${PORT} + 1")
    endif()
    foreach(_way IN LISTS _ways)
        if(_way STREQUAL "goodbye")
            set(_quiet "")
            set(_timeout 60)
            set(_let_go "let go [^\n]+ after it said it was leaving")
        else()
            set(_quiet --no-goodbye)
            set(_timeout 1)
            set(_let_go "let go [^\n]+ of silence\n")
        endif()
        execute_process(
            COMMAND "${CLIENT}" connect "127.0.0.1:${_port}" "${_key}" 2 ${_quiet}
            COMMAND "${SERVER}" --port ${_port} --seconds 7 --ai 1 --headless
                    --data "${DATA}" --timeout ${_timeout} --on-leave ${_what}
                    --store "${_store}"
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "--on-leave ${_what}: the server stopped badly:\n${_err}")
        endif()

        # It really did let the client go, the way it went, or this says nothing.
        if(NOT _out MATCHES "${_let_go}")
            message(FATAL_ERROR "--on-leave ${_what}, leaving by ${_way}: the server "
                                "never let the client go that way, so nothing "
                                "happened to its aircraft:\n${_out}")
        endif()
        if(NOT _out MATCHES "their aircraft ([^\n]+)\n")
            message(FATAL_ERROR "--on-leave ${_what}: the server did not say what "
                                "became of the aircraft:\n${_out}")
        endif()
        set(_said "${CMAKE_MATCH_1}")

        string(REGEX MATCHALL "flew [^\n]+" _flew "${_out}")
        list(LENGTH _flew _left)
        if(NOT _out MATCHES "ran ([0-9]+) AI aircraft")
            message(FATAL_ERROR "--on-leave ${_what}: the server did not count its "
                                "AI aircraft:\n${_out}")
        endif()
        set(_ai_at_end ${CMAKE_MATCH_1})

        if(_what STREQUAL "remove")
            if(NOT _said STREQUAL "is out of the sky")
                message(FATAL_ERROR "--on-leave remove said '${_said}'")
            endif()
            if(NOT _left EQUAL 1)
                message(FATAL_ERROR "--on-leave remove left ${_left} aircraft "
                                    "flying, and only the server's one should "
                                    "be:\n${_out}")
            endif()
            if(NOT _ai_at_end EQUAL 1)
                message(FATAL_ERROR "--on-leave remove ended with ${_ai_at_end} AI "
                                    "aircraft, not one")
            endif()
        else()
            if(NOT _said STREQUAL "is now flown by an AI pilot")
                message(FATAL_ERROR "--on-leave ai said '${_said}'")
            endif()
            if(NOT _left EQUAL 2)
                message(FATAL_ERROR "--on-leave ai left ${_left} aircraft flying, "
                                    "and the server's one plus the one handed over "
                                    "is two:\n${_out}")
            endif()
            if(NOT _ai_at_end EQUAL 2)
                message(FATAL_ERROR "--on-leave ai ended with ${_ai_at_end} AI "
                                    "aircraft, not two")
            endif()
            # **The one handed over is named, and it has gone somewhere.** An AI
            # pilot that flew nothing would leave it where its owner left it.
            if(NOT _out MATCHES "flew [^\n]*was slot ([0-9]+)[^\n]*at ([-0-9.]+), ([-0-9.]+)")
                message(FATAL_ERROR "--on-leave ai: no aircraft is named as one "
                                    "that was somebody's:\n${_out}")
            endif()
            set(_lat ${CMAKE_MATCH_2})
            if(_lat GREATER -33.9049 OR _lat LESS -33.9051)
                # It moved, which is what is wanted.
            else()
                message(FATAL_ERROR "--on-leave ai: the handed-over aircraft is "
                                    "still at ${_lat}, where the plan starts, so "
                                    "the AI pilot is flying nothing")
            endif()
        endif()
        message(STATUS "--on-leave ${_what}, leaving by ${_way}: the aircraft ${_said}, "
                       "${_left} flying at the end")
        math(EXPR _walked "${_walked} + 1")
    endforeach()
endforeach()

list(LENGTH _settings _how_many)
list(LENGTH _ways _how_many_ways)
math(EXPR _how_many "${_how_many} * ${_how_many_ways}")
if(NOT _walked EQUAL _how_many OR NOT _how_many EQUAL 4)
    message(FATAL_ERROR "only ${_walked} of the ${_how_many} settings and ways of "
                        "leaving were walked, and there are four")
endif()
message(STATUS "both settings walked, each for both ways of leaving: an aircraft "
               "is removed, or handed to an AI pilot that flies it on")

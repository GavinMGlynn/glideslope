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
# (cmake/server_window.cmake). --until-empty stops a server once its clients
# have gone, for tests that wait on clients rather than on the clock, and
# --ready-file says when it is flying, for a test that joins late, and
# --no-take-over forbids a player taking over an AI's aircraft.
# --test-step-ms makes every step take at least that long, to put a server
# behind real time on any machine (server_behind.cmake), and --test-pace runs
# its clock at a set fraction of real time (server_paced_prediction.cmake).
# --drop-once-flown
# drops the first player it has flown, as the drop button would, headless
# (server_drop_keeps_out.cmake), and --lose-goodbyes sends it no goodbye, as
# though every copy were lost (client_server_gone.cmake); --stop-once-flown
# stops the server, telling nobody, once a player is flown and that long has
# gone, for a test that starts it again under its client (the same script).
# --ai-planner and --ai-task give an AI aircraft a model to plan its flight and the task it is asked, --ai-playback
# plays that model's answers back from a recording, --ai-spacing sets how
# long apart planned aircraft take off, and --steps flies a number
# of steps as fast as they go (server_planned.cmake). --fail-engine-at stops
# every player's first engine at a time on the simulation's clock, for a
# player's copilot to glide from (server_copilot.cmake). --hand-over-planner
# gives an aircraft the AI is given in the air a model to plan it, and
# --hand-over-playback and --hand-over-record play its answers back or keep
# them (server_hand_over_planner.cmake). --weather flies a station's weather,
# fetched again every --weather-refresh; --metar a METAR given, observed at
# --station, and --metar-then another at a time, each blended in over
# --weather-blend - the weather every client is sent (server_weather.cmake);
# --changed-file is written once --metar-then's has taken over, for a client
# to join while it blends in (server_weather_join.cmake).
# --players-on-final starts every player on final to a runway, at the learnt
# landing's gate (server_learnt_landing.cmake), and --ai-on-final the AI
# aircraft, outside it, to be landed (server_ai_learnt_landing.cmake).
set(_expected --headless --players --port --store --key --timeout --plain
              --seconds --dry-run --data --ai --plan --fly --on-leave --version
              --help --window --window-dump --window-shot --window-press
              --until-empty --ready-file --no-take-over --test-step-ms --test-pace
              --drop-once-flown --lose-goodbyes --stop-once-flown
              --ai-planner --ai-task --ai-playback --ai-spacing
              --steps --fail-engine-at --hand-over-planner --hand-over-playback
              --hand-over-record --weather --metar --station --metar-then --changed-file
              --weather-blend --weather-refresh --players-on-final --ai-on-final)
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

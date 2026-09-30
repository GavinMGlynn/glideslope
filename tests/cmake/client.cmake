# client.cmake - runs the client for a test.
#
#   include(client.cmake)
#   glideslope_client(<out-var> <argument>...)
#
# Runs PROGRAM with the arguments, fails the test if it exits non-zero, and
# puts what it printed in <out-var>.
#
# **Leaks, in the sanitized build.** A GPU driver or windowing library that
# the client loads at run time can leak - Xlib keeps its resource database and
# input method, Mesa a worker thread's state - and is unloaded before the
# process exits. What decides whose leak it is is who allocated it: the first
# frame of the stack outside the sanitizer's and the C and C++ runtimes' - a
# frame LeakSanitizer could name, since frames of an unloaded library are
# sometimes mislabelled as the sanitizer's, at offsets far past its end. If
# that frame is in a shared library, or a module LeakSanitizer can no longer
# name, the leak is the library's, reported and ignored, even when glideslope or
# SDL called the library. So is a frame given as the client's with no function
# named: the client is built with its symbols, and its frames are named, but a
# driver unloaded before the report - Mesa's lavapipe, whose threads start
# straight from the sanitizer - is sometimes given as the client, at offsets
# past the end of its image. If the frame is in the client itself -
# glideslope's code, SDL's, Cesium Native's, or a standard container inlined
# into any of them - the test fails.

# **Each test names its own Cesium cache.** Cesium Native keeps its cache in
# one SQLite file, and `CesiumAsync::SqliteCache` alone refused a second
# writer at once - "database is locked" - and did not store the entry. The
# client now opens it through gfx::open_cesium_cache, which waits for the
# other writer instead, so sharing a file costs nothing but the wait
# (two_programs_writing_one_cesium_cache_at_once_both_store_everything). The
# names were the first fix and are kept, so that each test finds only its own
# fetches: a file per test, named after the script running it and the things
# that tell its runs apart, in CACHE, so each still finds it on the next run.
# The names below are no longer what keeps a run from being refused.
#
# **So does VIEW**, since the view test became one test a view: the seven ran
# at once against one file and three were refused on macOS the first time.
#
# **IMAGERY tells two of them apart too**, and was missing: both Mount
# Taranaki shots run frame_terrain.cmake with the same driver, one with the
# imagery draped and one without, so they shared a file - and when ctest ran
# them at the same moment on macOS, one was refused with "database is locked".
if(DEFINED CACHE)
    get_filename_component(_who "${CMAKE_SCRIPT_MODE_FILE}" NAME_WE)
    set(_imagery "")
    if(IMAGERY)
        set(_imagery "imagery")
    endif()
    # A script run for several cases - who is flying, say - has one per case.
    set(_case "")
    if(DEFINED FLYING)
        string(MAKE_C_IDENTIFIER "${FLYING}" _case)
    endif()
    # And a run told to ask the weather of somewhere else - nowhere, or a
    # stub answering WEATHER_ANSWER - runs beside the same case with weather.
    set(_weather "")
    if(DEFINED WEATHER_SERVICE)
        set(_weather "noweather${WEATHER_ANSWER}")
    endif()
    # Riding along, and riding along to take over, are two cases of one script.
    if(TAKE_OVER)
        string(APPEND _case "take_over")
    endif()
    # So are joining again, and being dropped (client_joins_again.cmake).
    if(DROP)
        string(APPEND _case "drop")
    endif()
    set(ENV{GLIDESLOPE_CESIUM_CACHE}
        "${CACHE}/cesium-${_who}${PROVIDER}${DRIVER}${AIRCRAFT}${_imagery}${VIEW}${_case}${_weather}.sqlite")
endif()

# Judges the leak reports in a run's standard error, as described above: fails
# the test for a leak allocated in the client, and says how many were not.
#
# **It also refuses a locked cache.** Two programs treading on one SQLite file
# lose nothing but the caching, which is why it went unnoticed for so long, so
# this makes it loud: a run that reports one fails.
function(glideslope_judge_leaks stderr_text)
    if(stderr_text MATCHES "database is locked")
        message(FATAL_ERROR
                "the Cesium cache was locked, so this run shared one with "
                "another: GLIDESLOPE_CESIUM_CACHE is $ENV{GLIDESLOPE_CESIUM_CACHE}")
    endif()
    string(REPLACE ";" "," _text "${stderr_text}")
    string(REPLACE "[" "<" _text "${_text}")
    string(REPLACE "]" ">" _text "${_text}")
    string(REPLACE "\n" ";" _lines "${_text}")
    set(_block "")
    set(_ours OFF)
    set(_decided OFF)
    set(_foreign 0)
    set(_report "")
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "(Direct|Indirect) leak of")
            if(_ours)
                string(APPEND _report "${_block}\n")
            elseif(NOT _block STREQUAL "")
                math(EXPR _foreign "${_foreign} + 1")
            endif()
            set(_block "${_line}\n")
            set(_ours OFF)
            set(_decided OFF)
        elseif(NOT _block STREQUAL "" AND _line MATCHES "^ *#([0-9]+) ")
            string(APPEND _block "${_line}\n")
            if(NOT _decided AND NOT CMAKE_MATCH_1 EQUAL 0
               AND NOT (_line MATCHES " in [^ ]"
                        AND _line MATCHES "libsanitizer|libasan|liblsan|libubsan|libstdc\\+\\+|libc\\+\\+|/libc\\.so|libgcc_s"))
                set(_decided ON)
                if(NOT _line MATCHES "\\(<unknown module>\\) *$"
                   AND NOT _line MATCHES "\\(/[^()]*\\.so[.0-9]*\\+0x[0-9a-f]+\\)"
                   AND NOT _line MATCHES "^ *#[0-9]+ 0x[0-9a-f]+ +\\(")
                    set(_ours ON)
                endif()
            endif()
        endif()
    endforeach()
    if(_ours)
        string(APPEND _report "${_block}\n")
    elseif(NOT _block STREQUAL "")
        math(EXPR _foreign "${_foreign} + 1")
    endif()
    if(NOT _report STREQUAL "")
        message(FATAL_ERROR "glideslope leaked, with frames in glideslope:\n${_report}")
    endif()
    if(_foreign GREATER 0)
        message(STATUS "${_foreign} leak reports were allocated by libraries loaded at run time; not glideslope's")
    endif()
endfunction()

# **The one rule for a run that could not download what it needed**, given
# its exit code and standard error, and any other words (ALSO, a regular
# expression) that say the same of this test's own downloads. It reports the
# test skipped (exit 77), never passed:
#
# - "the weather could not be had": always. The weather is live, somebody
#   else's and never kept, so a service having a bad minute is not a fault
#   here, even where the network is required.
# - "could not download", or ALSO: unless GLIDESLOPE_REQUIRE_NETWORK is set.
#   What else is downloaded - the DEM, the geoid, imagery - is pinned and
#   kept, so CI requires it and a failure there is a failure.
#
# Anything else, or a run that exited 0, it leaves to the caller.
function(glideslope_skip_when_not_downloaded rc err)
    if(rc EQUAL 0)
        return()
    endif()
    if(err MATCHES "the weather could not be had")
        message(STATUS "there is no weather to fly in, so this is skipped: ${err}")
        cmake_language(EXIT 77)
    endif()
    set(_pattern "could not download")
    if(ARGC GREATER 2)
        string(APPEND _pattern "|${ARGV2}")
    endif()
    if(err MATCHES "${_pattern}" AND "$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "what this needs could not be downloaded, so it is skipped: ${err}")
        cmake_language(EXIT 77)
    endif()
endfunction()

function(glideslope_client out)
    # Leaks are judged below rather than by LeakSanitizer's exit code.
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    execute_process(COMMAND "${PROGRAM}" ${ARGN}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope ${ARGN} exited ${_rc}\n${_out}${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
    set(${out} "${_out}" PARENT_SCOPE)
endfunction()

# **The frame rate a step bound is claimed for.** How far what the client
# with the window shows steps at a switch is a claim about what a player
# sees at a playable frame rate: 20 fps and above, a frame of 50 ms at most
# (the owner's decision, 2026-09-30; docs/REQUIREMENTS.md). Slower, a frame
# carries the aircraft metres and the step measures the machine, not the
# client - a sanitized build on a software renderer draws one in 250 ms and
# more. This is the one place the number is kept.
set(GLIDESLOPE_PLAYABLE_FRAME_MS 50)

# **A bound is believed only at that frame rate**, from what the client says
# of its own frames (client/shown.hpp): the longest of those around each
# switch, and with OTHERWISE, the one the largest step away from a switch
# came in, when the test bounds that too. Slower, the test fails - it neither
# passes nor skips, since a green tick then would say nothing, as the "too
# slow" guard on the speed says.
function(glideslope_require_playable_frames out)
    cmake_parse_arguments(PARSE_ARGV 1 _arg "OTHERWISE" "" "")
    if(NOT out MATCHES "own aircraft's frames: the longest within four of a switch ([0-9]+) ms, and the largest step otherwise in one ([0-9]+) ms long")
        message(FATAL_ERROR "the client did not say how long its frames were:\n${out}")
    endif()
    set(_around "${CMAKE_MATCH_1}")
    set(_otherwise "${CMAKE_MATCH_2}")
    # Where the longest pass around a switch spent its time, said first, so
    # that a failure on a machine nobody here has says which part was long.
    set(_parts "")
    if(out MATCHES "(the longest pass around a switch [^\n]*)")
        set(_parts "${CMAKE_MATCH_1}")
    endif()
    if(_around GREATER GLIDESLOPE_PLAYABLE_FRAME_MS)
        message(FATAL_ERROR "frames of ${_around} ms around the switch: slower than the 20 fps "
                            "this bound is claimed for, so the bound tests nothing; "
                            "${_parts}:\n${out}")
    endif()
    if(_arg_OTHERWISE AND _otherwise GREATER GLIDESLOPE_PLAYABLE_FRAME_MS)
        message(FATAL_ERROR "a frame of ${_otherwise} ms where the largest step away from a "
                            "switch came: slower than the 20 fps this bound is claimed for, "
                            "so the bound tests nothing:\n${out}")
    endif()
    message(STATUS "frames of ${_around} ms at most around the switches, and "
                   "${_otherwise} ms where the largest step otherwise came: "
                   "${GLIDESLOPE_PLAYABLE_FRAME_MS} ms at most, 20 fps; ${_parts}")
endfunction()

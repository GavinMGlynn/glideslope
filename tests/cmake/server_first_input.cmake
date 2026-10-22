# server_first_input.cmake - a player's first input keeps the trim the server
# held her on: through a real server, a predicting client flying an F-35B or a
# C172P from what the server held her on - touching nothing, or with a small
# stick movement as her first input - holds her height and attitude for
# thirty seconds of the session's clock, and her prediction agrees with the
# server.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P server_first_input.cmake
#
# **Built, not hoped for.** Until her first input the server holds a player's
# aircraft on what her trim found - her pitch trim, and the aileron and rudder
# against her engine's torque as her roll and yaw trim, the stick and pedals
# centred (server_untouched.cmake; the project owner, 2026-10-22). Her client sends
# inputs from its first frame, so the first input is not a choice the player
# makes: it is there as she joins. Until 2026-10-21 that input carried the
# client's own pitch trim, aileron and rudder, at nought, and dropped the trim
# the moment she joined. Each client here joins as the windowed client does
# (`--from-held`): it asks for its own aircraft's levers (`WATCH`), sends
# nothing until it has them, begins its prediction on them, and sends them as
# every input (frontend::keep_held) - a player who touches nothing - or, with
# `--nudge`, with the stick moved back and right a fortieth for her first
# half second and then the stick and pedals centred. It joins through a relay that holds every datagram
# 100 ms each way, so that the prediction is flown ahead of the server's word.
#
# **Every case, not a sample**: the two aircraft the item names, each touched
# not at all and nudged - four runs, counted. The figures (linux-debug,
# 2026-10-21) are in the status file; the bounds:
#   - height within 100 ft (30.48 m) of where she was first heard, as an
#     aircraft sent nothing is held (server_untouched.cmake);
#   - her pitch over no more than 5 degrees, and her bank within 5 degrees:
#     rolled off on her engine's torque once her stick is centred, as where
#     the aileron and rudder are held as the stick and pedals rather than as
#     trims, a C172P is at 37 degrees in thirty seconds;
#   - **the prediction, in server_stovl_landing.cmake's bounds and for its
#     reason**: the median under 60 mm, and the 95th percentile within four
#     steps' travel at her start speed. On a busy machine the clocks'
#     difference places some words a step off, or two, and that update is
#     off by that step's travel (server_stovl_landing.cmake says how): a
#     step is 1.378 m for the F-35B here and 0.462 m for the C172P - the
#     quanta her errors come in (the worst of an idle run here is exactly
#     one). So 5.512 m and 1.848 m. Measured (status file): the median
#     0.000 m in every run here and on CI; the 95th percentile 0.000 m here,
#     1.378 m on CI's macOS, Ubuntu debug, Windows release and clang, and
#     2.756 m on Windows debug - one step and two. And its worst, as there,
#     within four steps' travel at 330 kt for the F-35B (5.6 m: she flies
#     321 kt, and CI's Windows release has put a word exactly four of her
#     steps off, 5.514 m), and four of the C172P's steps (1.848 m).
# Nudged, the F-35B rose 10.1 m (33 ft) and banked 2.6 degrees: her flight
# controls hold the attitude a stick movement leaves her at. A twentieth
# rose her 25 m, too near the 30.48 m bound for a rule tested by its margin.
# Seen red with the client keeping its own pitch trim, aileron and rudder,
# as before 2026-10-21: the F-35B untouched was a wreck; and with the
# aileron and rudder held as the stick and pedals, not as trims: the C172P
# nudged then centred banked 36.8 degrees and fell 90 m.
#
# **On the session's clock and its events**: each client stays until it has
# heard its own aircraft for thirty simulated seconds (`--until-held 30`),
# the server until its client has gone (`--until-empty`), and the relay until
# the server has (`--until-input-ends`).
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/first_input.sqlite")
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

# A figure in metres or degrees, to two places, as hundredths: CMake has no
# floating point.
function(hundredths value out)
    set(_v "${value}")
    set(_sign "")
    if(_v MATCHES "^-")
        set(_sign "-")
        string(SUBSTRING "${_v}" 1 -1 _v)
    endif()
    if(NOT _v MATCHES "\\.")
        set(_v "${_v}.00")
    endif()
    string(REGEX REPLACE "\\.([0-9])$" ".\\10" _v "${_v}")
    string(REGEX REPLACE "\\.([0-9][0-9])[0-9]*$" "\\1" _v "${_v}")
    string(REGEX REPLACE "^0+([0-9])" "\\1" _v "${_v}")
    set(${out} "${_sign}${_v}" PARENT_SCOPE)
endfunction()

math(EXPR _relay "${PORT} + 1")
set(_cases 0)
foreach(_id IN ITEMS f35b c172p)
    foreach(_touch IN ITEMS untouched nudged)
        set(_heard "${WORK}/${_id}-${_touch}.txt")
        file(REMOVE "${_heard}")
        set(_nudge "")
        if(_touch STREQUAL "nudged")
            set(_nudge "--nudge")
        endif()
        execute_process(
            COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 600 --after 1
                    --aircraft ${_id} --predict --from-held ${_nudge} --until-held 30
                    --heard "${_heard}"
            COMMAND "${SERVER}" --port ${PORT} --seconds 900 --until-empty --ai 1 --headless
                    --data "${DATA}" --timeout 5 --store "${_store}"
            COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
                    --loss 0 --seed 1 --until-input-ends --seconds 900
            RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        if(NOT _rcs STREQUAL "0;0;0")
            message(FATAL_ERROR "the ${_id}, ${_touch}: the programs' exit codes were "
                                "${_rcs}:\n${_out}\n${_err}")
        endif()
        if(_out MATCHES "is a wreck" OR _err MATCHES "is a wreck")
            message(FATAL_ERROR "the ${_id}, ${_touch}, was a wreck:\n${_out}\n${_err}")
        endif()
        # The server's stdout reaches standard error through the relay.
        if(NOT _err MATCHES "asked for ${_id}, and flies it\n")
            message(FATAL_ERROR "the server did not give the ${_id} asked for:\n${_err}")
        endif()
        file(READ "${_heard}" _said)
        if(NOT _said MATCHES "my aircraft heard from ([0-9.]+) s to ([0-9.]+) s: ([-0-9.]+) m at first, ([-0-9.]+) m to ([-0-9.]+) m")
            message(FATAL_ERROR "the ${_id}'s client did not say how high it heard her:\n${_said}")
        endif()
        set(_from ${CMAKE_MATCH_1})
        set(_to ${CMAKE_MATCH_2})
        set(_first ${CMAKE_MATCH_3})
        set(_low ${CMAKE_MATCH_4})
        set(_high ${CMAKE_MATCH_5})
        if(NOT _said MATCHES "my aircraft's attitude meanwhile: pitch ([-0-9.]+) to ([-0-9.]+) degrees, bank ([0-9.]+) degrees at most")
            message(FATAL_ERROR "the ${_id}'s client did not say her attitude:\n${_said}")
        endif()
        set(_pitch_low ${CMAKE_MATCH_1})
        set(_pitch_high ${CMAKE_MATCH_2})
        set(_bank ${CMAKE_MATCH_3})
        if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9.]+) m")
            message(FATAL_ERROR "the ${_id}'s client did not say its prediction error:\n${_said}")
        endif()
        set(_compared ${CMAKE_MATCH_1})
        set(_worst ${CMAKE_MATCH_2})
        if(NOT _said MATCHES "prediction error median: ([0-9.]+) m")
            message(FATAL_ERROR "the ${_id}'s client did not say its median error:\n${_said}")
        endif()
        set(_median ${CMAKE_MATCH_1})
        if(NOT _said MATCHES "prediction error 95th percentile: ([0-9.]+) m")
            message(FATAL_ERROR "the ${_id}'s client did not say its 95th percentile:\n${_said}")
        endif()
        set(_p95 ${CMAKE_MATCH_1})
        foreach(_v IN ITEMS from to first low high pitch_low pitch_high bank worst median p95)
            hundredths("${_${_v}}" _${_v}_c)
        endforeach()
        message(STATUS "the ${_id}, ${_touch}, from ${_from} s to ${_to} s of the session: "
                       "${_first} m at first, ${_low} m to ${_high} m; pitch ${_pitch_low} to "
                       "${_pitch_high} degrees, bank ${_bank} at most; prediction error median "
                       "${_median} m, 95th percentile ${_p95} m, worst ${_worst} m over "
                       "${_compared} updates")
        math(EXPR _heard_c "${_to_c} - ${_from_c}")
        math(EXPR _fell_c "${_first_c} - ${_low_c}")
        math(EXPR _rose_c "${_high_c} - ${_first_c}")
        math(EXPR _pitch_span_c "${_pitch_high_c} - ${_pitch_low_c}")
        if(_heard_c LESS 3000)
            message(FATAL_ERROR "the ${_id}, ${_touch}, was heard from ${_from} s to ${_to} s, "
                                "not thirty")
        endif()
        if(_fell_c GREATER 3048 OR _rose_c GREATER 3048)
            message(FATAL_ERROR "the ${_id}, ${_touch}, went from ${_first} m to between ${_low} m "
                                "and ${_high} m in thirty seconds: more than 100 ft (30.48 m) "
                                "from where she began")
        endif()
        if(_pitch_span_c GREATER 500 OR _bank_c GREATER 500)
            message(FATAL_ERROR "the ${_id}, ${_touch}, pitched from ${_pitch_low} to "
                                "${_pitch_high} degrees and banked ${_bank}: more than 5 degrees")
        endif()
        if(_compared LESS 100)
            message(FATAL_ERROR "the ${_id}, ${_touch}: only ${_compared} updates were compared")
        endif()
        # Four steps' travel at her start speed, in centimetres.
        if(_id STREQUAL "f35b")
            set(_four_steps_c 551)
        else()
            set(_four_steps_c 185)
        endif()
        if(_median_c GREATER 6 OR _p95_c GREATER _four_steps_c)
            message(FATAL_ERROR "the ${_id}, ${_touch}: the prediction was off by ${_median} m "
                                "at the median and ${_p95} m at the 95th percentile, past "
                                "60 mm and four steps' travel (${_four_steps_c} cm):\n${_said}")
        endif()
        if(_id STREQUAL "f35b")
            set(_worst_bound_c 560)
        else()
            set(_worst_bound_c 185)
        endif()
        if(_worst_c GREATER _worst_bound_c)
            message(FATAL_ERROR "the ${_id}, ${_touch}: the prediction's worst update was "
                                "${_worst} m off, past four steps' travel (${_worst_bound_c} "
                                "cm):\n${_said}")
        endif()
        math(EXPR _cases "${_cases} + 1")
    endforeach()
endforeach()
if(NOT _cases EQUAL 4)
    message(FATAL_ERROR "${_cases} cases were flown, not the four: two aircraft, each "
                        "untouched and nudged")
endif()

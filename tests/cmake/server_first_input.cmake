# server_first_input.cmake - a player's first input keeps the trim the server
# held her on: through a real server, a predicting client flying an F-35B or a
# C172P from what the server held her on - touching nothing, or with a small
# stick movement as her first input - holds her height and attitude for
# thirty seconds of the session's clock, and her prediction agrees with the
# server.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> [-DAIRCRAFT=f22 -DTOUCHES=untouched]
#         -P server_first_input.cmake
#
# **The F-22A untouched, alone** (`-DAIRCRAFT=f22 -DTOUCHES=untouched`): the
# server holds her on her held trim, a deflection after her pitch loop that
# no control sets (tools/make_f22.py, "The held trim"), and tells it in her
# `WATCH`; her client puts its prediction's at what it is told
# (sim::Prediction::held_by_server). The same bounds, four steps' travel at
# her 300 kt start speed being 5.144 m. **Those bounds do not see the held
# trim**: untold, her prediction's pitch loop winds its integral up to take
# the held trim's place, and she is predicted alike all the same (linux-debug,
# 2026-10-22: median and 95th percentile 0.000 m either way). So her client
# also says the held trim its prediction was put at, and the most its pitch
# loop's integral reached: the held trim must be the one told, and not
# nought, and the integral within 0.004 - told, 0.0011; untold, the held
# trim nought and the integral 0.0095. Seen red so.
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
#   - **the prediction**: the median under 60 mm and the 95th percentile
#     within four steps' travel at her start speed, server_stovl_landing.
#     cmake's bounds for its reason - on a busy machine some words are
#     placed a step off, or more, and that update is off by those steps'
#     travel: 1.378 m a step for the F-35B here, 0.462 m for the C172P, so
#     5.512 m and 1.848 m. And **every update within 60 mm of the path her
#     prediction flew**: a word whose input reached a server or relay held
#     up is placed whole steps along a path flown alike - CI's Windows clang
#     and debug, neither client stalling a frame over 171 ms, put words 5
#     and 18 of her steps off (6.9 and 24.7 m), and 40 busy loops here 16
#     steps (22.0 m) at 3 mm off the path - where a prediction gone wrong is
#     off it. Seen red with her prediction given 5 m/s down once in 400
#     steps: 0.123 m off the path, where the worst at its step stayed
#     under four steps' travel. And a machine's stalls capped and each
#     recovered from (machine_stalls_check.cmake), as every prediction test.
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

include("${CMAKE_CURRENT_LIST_DIR}/machine_stalls_check.cmake")

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
if(NOT DEFINED AIRCRAFT)
    set(AIRCRAFT f35b c172p)
endif()
if(NOT DEFINED TOUCHES)
    set(TOUCHES untouched nudged)
endif()
list(LENGTH AIRCRAFT _aircraft_n)
list(LENGTH TOUCHES _touches_n)
math(EXPR _expected "${_aircraft_n} * ${_touches_n}")
set(_cases 0)
foreach(_id IN LISTS AIRCRAFT)
    foreach(_touch IN LISTS TOUCHES)
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
        elseif(_id STREQUAL "f22")
            set(_four_steps_c 515)
        else()
            set(_four_steps_c 185)
        endif()
        if(_median_c GREATER 6 OR _p95_c GREATER _four_steps_c)
            message(FATAL_ERROR "the ${_id}, ${_touch}: the prediction was off by ${_median} m "
                                "at the median and ${_p95} m at the 95th percentile, past "
                                "60 mm and four steps' travel (${_four_steps_c} cm):\n${_said}")
        endif()
        # **Off where, apart from off when.** Every update outside the
        # machine's stalls against the path this client flew: a word whose
        # input reached the server late is on that path, whole steps along it
        # from where its input placed it, and its error above is those steps'
        # travel - a relay or server held up, which no client can see, the
        # plan's "On a busy machine the server's word is placed a step off".
        # A prediction gone wrong is off the path.
        if(NOT _said MATCHES "prediction error off the path: the worst ([0-9.]+) m from the path flown over ([0-9]+) updates, ([0-9]+) nearest at an end of the steps kept; placed ([0-9]+) steps")
            message(FATAL_ERROR "the ${_id}'s client did not say its error off the path:\n${_said}")
        endif()
        set(_off_path ${CMAKE_MATCH_1})
        set(_on_path_compared ${CMAKE_MATCH_2})
        set(_at_an_end ${CMAKE_MATCH_3})
        set(_steps_off ${CMAKE_MATCH_4})
        # In millimetres, for a bound of 60 mm.
        string(REGEX REPLACE "^([0-9]+)\\.([0-9][0-9][0-9]).*" "\\1\\2" _off_path_mm "${_off_path}")
        math(EXPR _off_path_mm "${_off_path_mm} + 0")
        message(STATUS "the ${_id}, ${_touch}: off the path flown by ${_off_path} m at worst over "
                       "${_on_path_compared} updates, placed up to ${_steps_off} steps along it, "
                       "${_at_an_end} nearest at an end")
        # A word placed so far along that the nearest step kept is the first
        # or the newest cannot be held against the path - it may lie beyond
        # it - and is judged by its step alone: no more of them than the
        # 95th percentile leaves above it, a twentieth (CI's Ubuntu debug
        # once placed one 13 steps off, at an end, 0.000 m off the path
        # in every other).
        math(EXPR _most_at_an_end "${_compared} / 20")
        if(_on_path_compared LESS 100 OR _at_an_end GREATER _most_at_an_end)
            message(FATAL_ERROR "the ${_id}, ${_touch}: ${_on_path_compared} updates held against "
                                "the path flown, and ${_at_an_end} nearest an end of it - past "
                                "a twentieth (${_most_at_an_end}):\n${_said}")
        endif()
        if(_off_path_mm GREATER 60)
            message(FATAL_ERROR "the ${_id}, ${_touch}: an update was ${_off_path} m off the path "
                                "her prediction flew, past 60 mm:\n${_said}")
        endif()
        check_machine_stalls("${_said}" updates "${_compared}" _stalls)
        message(STATUS "the ${_id}, ${_touch}: ${_stalls}")
        if(_id STREQUAL "f22")
            if(NOT _said MATCHES "held trim: the prediction's ([-0-9.]+), told ([-0-9.]+); its pitch loop's integral ([0-9.]+) at most")
                message(FATAL_ERROR "the f22's client did not say her held trim:\n${_said}")
            endif()
            set(_put "${CMAKE_MATCH_1}")
            set(_told "${CMAKE_MATCH_2}")
            set(_integral "${CMAKE_MATCH_3}")
            message(STATUS "the f22's held trim: her prediction's ${_put}, told ${_told}; her "
                           "pitch loop's integral ${_integral} at most")
            if(NOT _put STREQUAL _told OR _told STREQUAL "0.0000")
                message(FATAL_ERROR "the f22's prediction was put at a held trim of ${_put}, "
                                    "told ${_told}")
            endif()
            string(REGEX REPLACE "^0\\.([0-9][0-9][0-9][0-9])$" "\\1" _integral_e4 "${_integral}")
            if(NOT _integral MATCHES "^0\\.[0-9][0-9][0-9][0-9]$" OR _integral_e4 GREATER 40)
                message(FATAL_ERROR "the f22's prediction's pitch loop integral reached "
                                    "${_integral}, past 0.004")
            endif()
        endif()
        math(EXPR _cases "${_cases} + 1")
    endforeach()
endforeach()
if(NOT _cases EQUAL _expected)
    message(FATAL_ERROR "${_cases} cases were flown, not the ${_expected}: ${AIRCRAFT}, each "
                        "${TOUCHES}")
endif()

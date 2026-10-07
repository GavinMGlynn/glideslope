# client_rides_along.cmake - the client with the window rides along in an AI
# aircraft: its seat, its instruments and its controls.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> [-DTAKE_OVER=ON] -P client_rides_along.cmake
#
# **Built, not hoped for.** A server with one AI Cessna, and the client with
# the window joining it with `--ride-along` - which watches the first AI
# aircraft from the start - in the cockpit view, taking a shot ten seconds in.
# It must say it rode along in the AI's Cessna, with the camera at its seat -
# within 5 m of its centre of gravity, where a Cessna's pilot sits a metre or
# two from it - and that the HUD said the AI was flying it and where each of
# its controls was: what riding along is.
#
# **A slow machine is not let go.** The client stands still five seconds
# after joining (`--slow-start 5`), as a slow machine building its flight
# does, against a server that lets a silent client go after three: it must
# stay in the session, and so be told the controls at all. A Windows debug
# build was let go, heard one update, and drew its HUD without them.
#
# With TAKE_OVER, the client takes the AI's Cessna over four seconds in, with
# `--take-over-after 4`, and by the shot must say it took it over and is
# flying it, the server saying the pilot has it and having flown it by inputs
# sent since, and the HUD saying the pilot has it: what taking over is. The
# last update from before the take-over is heard again after it
# (`--late-update-after-take-over`), as a network reorders them, and must take
# nothing over again: it is taken over once. (What
# the server says of it, which goes down the pipe here, server_take_over.cmake
# checks.) And what it shows must not step at the take-over: the client says,
# at the shot, how many switches of its own aircraft it measured - the one -
# and the largest step at one (frontend/shown.hpp), which must be under 2.5 m.
# **Half the network checks' 5 m, so that the test sees the blend gone**:
# taken over, the aircraft goes from being drawn 100 ms behind the clock to
# being predicted from the update that gave it, and nothing blended that is a
# step of its speed over those 100 ms whatever the network - 5.1 m for the
# AI's Cessna, at the network checks' bound and not past it.
#
# With A_DURING, A is pressed in the frame the take-over is asked
# (`--press-a-with-take-over`), before the server has answered: A must wait
# for the answer, and then hand to the AI whichever aircraft is the client's
# own by then. A_DURING=TAKEN, on a server that allows the take-over: the
# aircraft taken, and the server must say the AI has it. A_DURING=REFUSED,
# on one started `--no-take-over`: the server must say it refused
# (`TAKE_OVER_REFUSED`), and A then hands over the aircraft kept. Sent at
# once for the aircraft it had, the first A goes to the one being left, and
# the server - which has made another aircraft the client's own - does nothing
# with it; without the refusal said, the second waits for ever.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/ride.sqlite")
set(_shot "${WORK}/ride.bmp")
file(REMOVE "${_store}" "${_shot}")

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

set(_take_over)
if(TAKE_OVER)
    set(_take_over --take-over-after 4 --late-update-after-take-over)
endif()
set(_server_take_over)
if(DEFINED A_DURING)
    set(_take_over --take-over-after 4 --press-a-with-take-over)
    if(A_DURING STREQUAL "REFUSED")
        set(_server_take_over --no-take-over)
    endif()
endif()
set(_slow)
if(DEFINED SLOW_FRAMES)
    set(_slow --slow-frames ${SLOW_FRAMES})
endif()
set(ENV{LSAN_OPTIONS} "exitcode=0")
# **The client connects once the server is flying** (its --ready-file): its
# handshake gives up five seconds after it begins, and a debug server on a
# loaded runner took longer than that to build its terrain (client.cmake).
set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
            ${_server_take_over}
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 1200 --view cockpit --ride-along --slow-start 5
            ${_take_over} ${_slow} --after-ready "${_ready}"
            --server 127.0.0.1 ${PORT} --server-key ${_key}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT EXISTS "${_shot}")
    if(_err MATCHES "no GPU|could not|device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the client drew nothing:\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()

if(DEFINED A_DURING)
    if(NOT _out MATCHES "asked to take over aircraft ([0-9]+)\nglideslope: A held until the server answers the take-over of aircraft ([0-9]+)\n")
        message(FATAL_ERROR "A, pressed as the take-over was asked, was not held for "
                            "its answer:\n${_out}")
    endif()
    set(_asked "${CMAKE_MATCH_1}")
    if(A_DURING STREQUAL "TAKEN")
        if(NOT _out MATCHES "took over aircraft ${_asked}, the c172p")
            message(FATAL_ERROR "the client did not take aircraft ${_asked} over:\n${_out}")
        endif()
        set(_for "${_asked}")
    else()
        if(_out MATCHES "took over aircraft")
            message(FATAL_ERROR "a server started --no-take-over let it be taken:\n${_out}")
        endif()
        if(NOT _out MATCHES "the server refused to take over aircraft ${_asked}\n")
            message(FATAL_ERROR "the client was not told the take-over of aircraft "
                                "${_asked} was refused:\n${_out}")
        endif()
        if(NOT _out MATCHES "A, held, is for aircraft ([0-9]+)\n")
            message(FATAL_ERROR "A, held, was never let go:\n${_out}")
        endif()
        set(_for "${CMAKE_MATCH_1}")
        if(_for EQUAL _asked)
            message(FATAL_ERROR "refused, A went to the aircraft asked for, ${_asked}:\n${_out}")
        endif()
    endif()
    if(NOT _out MATCHES "A, held, is for aircraft ${_for}\nglideslope: asked for aircraft ${_for} to be handed to the AI\n")
        message(FATAL_ERROR "A, held, did not ask for aircraft ${_for} to go to the AI:\n${_out}")
    endif()
    # What the server decides, not what the client asked: the AI has it.
    if(NOT _out MATCHES "the server says the AI has aircraft ${_for}\n")
        message(FATAL_ERROR "the server did not hand aircraft ${_for} to the AI:\n${_out}")
    endif()
    string(REGEX MATCHALL "asked for aircraft [0-9]+ to be handed" _asks "${_out}")
    list(LENGTH _asks _ask_count)
    if(NOT _ask_count EQUAL 1)
        message(FATAL_ERROR "the client asked ${_ask_count} hand-overs, not the one:\n${_out}")
    endif()
    message(STATUS "A pressed during the take-over of aircraft ${_asked} (${A_DURING}) "
                   "handed aircraft ${_for} to the AI")
    return()
endif()
if(TAKE_OVER)
    if(NOT _out MATCHES "took over aircraft ([0-9]+), the c172p")
        message(FATAL_ERROR "the client did not take the AI's Cessna over:\n${_out}")
    endif()
    set(_taken "${CMAKE_MATCH_1}")
    # **Reordered across the take-over**: the last update from before it,
    # naming the aircraft given up as its own, is heard again after it
    # (`--late-update-after-take-over`), as the network reorders them. Only
    # the newest word may change which aircraft is its own, so it must take
    # nothing over again.
    if(NOT _out MATCHES "an update from before the take-over is heard again after it")
        message(FATAL_ERROR "no update from before the take-over was heard after it:\n${_out}")
    endif()
    string(REGEX MATCHALL "took over aircraft [0-9]+" _takings "${_out}")
    list(LENGTH _takings _taking_count)
    if(NOT _taking_count EQUAL 1)
        message(FATAL_ERROR "the client took over ${_taking_count} times, not once - an update "
                            "from before the take-over took the aircraft given up back:\n${_out}")
    endif()
    # What the server decides, not what the client thinks: that the pilot
    # has it, and that it has flown it by inputs sent since.
    if(NOT _out MATCHES "flying aircraft ${_taken}, the c172p; the server says the pilot has it, and has flown it by inputs sent since it was taken over")
        message(FATAL_ERROR "the server was not flying aircraft ${_taken} by this client's "
                            "inputs at the shot:\n${_out}")
    endif()
    if(NOT _out MATCHES "the HUD reads FLYING PILOT" OR _out MATCHES "the HUD reads FLYING AI")
        message(FATAL_ERROR "the HUD did not say the pilot has the aircraft taken over:\n${_out}")
    endif()
    if(NOT _out MATCHES "own aircraft: ([0-9]+) switches; the largest step at a switch ([0-9.]+) m, and otherwise ([0-9.]+) m")
        message(FATAL_ERROR "the client did not say how far what it showed stepped:\n${_out}")
    endif()
    set(_switches "${CMAKE_MATCH_1}")
    set(_step "${CMAKE_MATCH_2}")
    set(_otherwise "${CMAKE_MATCH_3}")
    if(NOT _switches EQUAL 1)
        message(FATAL_ERROR "the client measured ${_switches} switches of its own aircraft, "
                            "not the 1 made - taken over:\n${_out}")
    endif()
    # **The rule is tested only if taking the blend out would break it**: without
    # a blend the step is the aircraft's speed over the time between the two
    # sources - at least the 100 ms the aircraft taken over was drawn
    # behind - so the speed it was carried at, which the client says of the
    # largest step, must put that past the 2.5 m bound: faster than 25 m/s.
    # Slower, the green tick would say nothing.
    if(NOT _out MATCHES "the largest step at a switch: [^\n]* carried at ([0-9.]+) m/s")
        message(FATAL_ERROR "the client did not say how fast its aircraft was at a switch:\n${_out}")
    endif()
    set(_speed "${CMAKE_MATCH_1}")
    if(CMAKE_MATCH_1 LESS_EQUAL 25)
        message(FATAL_ERROR "the aircraft was carried at ${CMAKE_MATCH_1} m/s at a switch: too slow "
                            "for a step without the blend to pass 2.5 m, so the bound tests "
                            "nothing:\n${_out}")
    endif()
    # **And only at a playable frame rate**: the bound is claimed at 20 fps
    # and above, and a slower machine's frames would measure the machine.
    glideslope_require_playable_frames("${_out}" OTHERWISE)
    if(_step GREATER_EQUAL 2.5)
        message(FATAL_ERROR "what the client showed stepped ${_step} m at the take-over, the "
                            "bound 2.5 m:\n${_out}")
    endif()
    # **And away from a switch**, where corrections of the prediction are what
    # move it: each small enough to hide is taken up over a quarter of a
    # second, so nothing steps as far as the switches' own 2.5 m.
    if(_otherwise GREATER_EQUAL 2.5)
        message(FATAL_ERROR "what the client showed stepped ${_otherwise} m away from a switch, "
                            "the bound 2.5 m:\n${_out}")
    endif()
    message(STATUS "took the AI's Cessna, aircraft ${_taken}, over, and flew it; the largest "
                   "step at the switch ${_step} m, carried at ${_speed} m/s, and otherwise "
                   "${_otherwise} m")
    # What made the step, and how the prediction was put right: a pass says
    # as much as a failure, so a margin eaten away shows before it fails.
    string(REGEX MATCH "glideslope: predicted: [^\n]*" _predicted "${_out}")
    string(REGEX MATCH "glideslope: the largest step at a switch: [^\n]*" _what "${_out}")
    message(STATUS "${_predicted}")
    message(STATUS "${_what}")
    return()
endif()
if(NOT _out MATCHES "riding along in aircraft ([0-9]+), the c172p; the camera ([0-9.]+) m from its centre")
    message(FATAL_ERROR "the client did not ride along in the AI's Cessna:\n${_out}")
endif()
set(_away "${CMAKE_MATCH_2}")
if(_away GREATER_EQUAL 5)
    message(FATAL_ERROR "the camera was ${_away} m from the aircraft ridden in, not in its seat")
endif()
foreach(_line IN ITEMS "FLYING AI" "STICK [-+][0-9]\\.[0-9][0-9] [-+][0-9]\\.[0-9][0-9]"
                       "RUDDER [-+][0-9]\\.[0-9][0-9]" "THROTTLE [0-9]\\.[0-9][0-9]"
                       "FLAPS [0-9]\\.[0-9][0-9]" "GS +[0-9]+ KT")
    if(NOT _out MATCHES "the HUD reads ${_line}")
        message(FATAL_ERROR "the HUD did not read \"${_line}\" riding along:\n${_out}")
    endif()
endforeach()
# A Cessna's gear does not retract, and the HUD says nothing of it; nor of
# speedbrakes, which she has none of.
if(_out MATCHES "the HUD reads GEAR")
    message(FATAL_ERROR "the HUD showed a gear the Cessna does not have:\n${_out}")
endif()
if(_out MATCHES "the HUD reads SPEEDBRAKE")
    message(FATAL_ERROR "the HUD showed speedbrakes the Cessna does not have:\n${_out}")
endif()
message(STATUS "rode along in the AI's Cessna, the camera ${_away} m from its centre, "
               "its instruments and controls on the HUD")

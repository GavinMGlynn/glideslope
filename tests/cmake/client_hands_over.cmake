# client_hands_over.cmake - the client with the window hands its own aircraft
# to the AI on a server, and takes it back.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -DIMPAIR=<glideslope_impair> -P client_hands_over.cmake
#
# **Built, not hoped for.** A server with one AI Cessna, and the client with
# the window joining it and flying its own aircraft, which it asks the server
# to hand to the AI four seconds in (`--hand-over-after 4`, what A does on a
# server) and to give back ten seconds in (`--take-back-after 10`), taking a
# shot sixteen seconds in. It must say, once each and in that order, that the
# server said the AI had its aircraft - its HUD then reading FLYING AI - and
# then that the server said the pilot had it - its HUD then reading FLYING
# PILOT; and at the shot that the server says the pilot has it and has flown
# it by inputs sent since it was taken back: what handing over and taking
# back is. "The server said" is the controller the server's own state updates
# give the aircraft, not what the client asked for.
#
# **The shot waits on the events, not the clock**: it is held, up to a minute
# of the flight past its tick, until the server has said the pilot has it
# again and has flown it by an input sent since, and says how long it waited.
#
# **Handed over, riding along in nothing is riding along in its own
# aircraft**: six seconds in it rides along in the next aircraft, the AI's,
# and seven seconds in in the next again (`--next-aircraft-after`, what W
# does), which wraps past the last back to its own. The view must then be its
# own aircraft's seat as the updates put it - within 5 m of its centre - not
# the flight here, which is not flown while the AI has it and would sit where
# it was handed over while the aircraft flew on.
#
# **What it shows does not step at either switch**: the client says, at the
# shot, how many switches of its own aircraft it measured and the largest step
# what it showed made at one (client/shown.hpp) - measured as glideslope_cli
# measures it, worked out sixty times a second whether or not a frame is
# drawn. It must have measured both, the hand-over and the take-back, and the
# largest step must be under 5 m, the bound the network checks hold
# (server_impaired.cmake, server_take_over.cmake).
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/hand.sqlite")
set(_shot "${WORK}/hand.bmp")
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

# The client reaches the server through a relay that holds every datagram
# 100 ms each way (glideslope_impair, at PORT + 1): with the updates drawn
# 100 ms behind the clock as well, what the prediction shows and what the
# updates show are a third of a second of flight apart - a step of well over
# the bound, were nothing blended. On the bare loopback they were a tenth
# apart, and nothing blended stepped 4.2 m: under the bound, so the test
# would not have seen the blend gone.
math(EXPR _relay "${PORT} + 1")
set(ENV{LSAN_OPTIONS} "exitcode=0")
execute_process(
    # The server's standard output goes to the relay, which passes it to
    # standard error and stops when it ends; the client's is what is read.
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}"
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0 --loss 0
            --seed 1 --until-input-ends --seconds 290
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 1920 --view cockpit
            --hand-over-after 4 --take-back-after 10
            --next-aircraft-after 6 --next-aircraft-after 7
            --server 127.0.0.1 ${_relay} --server-key ${_key}
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

# Once each, and the AI first.
foreach(_who IN ITEMS "the AI" "the pilot")
    string(REGEX MATCHALL "the server says ${_who} has aircraft [0-9]+\n" _said "${_out}")
    list(LENGTH _said _times)
    if(NOT _times EQUAL 1)
        message(FATAL_ERROR "the client said ${_times} times, not once, that the server "
                            "gave its aircraft to ${_who}:\n${_out}")
    endif()
endforeach()
string(REGEX MATCH "the server says the AI has aircraft ([0-9]+)\n" _ignored "${_out}")
set(_own "${CMAKE_MATCH_1}")
string(FIND "${_out}" "the server says the AI has aircraft" _to_ai)
string(FIND "${_out}" "the server says the pilot has aircraft" _to_pilot)
if(_to_pilot LESS _to_ai)
    message(FATAL_ERROR "the server gave the aircraft back before it handed it over:\n${_out}")
endif()
# What the HUD read at each: FLYING AI between them, FLYING PILOT after.
math(EXPR _between "${_to_pilot} - ${_to_ai}")
string(SUBSTRING "${_out}" ${_to_ai} ${_between} _handed)
string(SUBSTRING "${_out}" ${_to_pilot} -1 _back)
if(NOT _handed MATCHES "the HUD reads FLYING AI" OR _handed MATCHES "the HUD reads FLYING PILOT")
    message(FATAL_ERROR "handed over, the HUD did not say the AI has it:\n${_out}")
endif()
if(NOT _back MATCHES "^the server says the pilot has aircraft [0-9]+\n(glideslope: the HUD reads [^\n]*\n)*glideslope: the HUD reads FLYING PILOT")
    message(FATAL_ERROR "taken back, the HUD did not say the pilot has it:\n${_out}")
endif()
# Wrapped back past the last aircraft while the AI had it: its own, in its seat.
if(NOT _handed MATCHES "back in your own aircraft\nglideslope: riding along in aircraft ${_own}, the c172p; the camera ([0-9.]+) m from its centre")
    message(FATAL_ERROR "handed over, W past the last aircraft did not ride along in its "
                        "own aircraft ${_own} as the updates put it:\n${_out}")
endif()
if(CMAKE_MATCH_1 GREATER_EQUAL 5)
    message(FATAL_ERROR "the camera was ${CMAKE_MATCH_1} m from its own aircraft, not in "
                        "its seat:\n${_out}")
endif()
if(NOT _out MATCHES "the shot drawn [0-9.]+ s past its tick; the server says the pilot has it, flown by an input sent since")
    message(FATAL_ERROR "the shot was drawn before the take-back was heard:\n${_out}")
endif()
# And at the shot, six seconds later, flown by this client's inputs again.
if(NOT _out MATCHES "flying aircraft ([0-9]+), the c172p; the server says the pilot has it, and has flown it by inputs sent since it was taken back")
    message(FATAL_ERROR "the server was not flying the client's aircraft by its inputs at "
                        "the shot:\n${_out}")
endif()
set(_flown "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "own aircraft: ([0-9]+) switches; the largest step at a switch ([0-9.]+) m")
    message(FATAL_ERROR "the client did not say how far what it showed stepped:\n${_out}")
endif()
set(_switches "${CMAKE_MATCH_1}")
set(_step "${CMAKE_MATCH_2}")
if(NOT _switches EQUAL 2)
    message(FATAL_ERROR "the client measured ${_switches} switches of its own aircraft, not "
                        "the 2 made - handed over and taken back:\n${_out}")
endif()
# **The rule is tested only if taking the blend out would break it**: without
# a blend the step is the aircraft's speed over the time between the two
# sources - at least the 100 ms the updates are drawn behind, and the
# relay's 100 ms on the way - so the speed it was carried at, which the
# client says of the largest step, must put that past the 5 m bound: faster
# than 25 m/s. Slower, the green tick would say nothing.
if(NOT _out MATCHES "the largest step at a switch: [^\n]* carried at ([0-9.]+) m/s")
    message(FATAL_ERROR "the client did not say how fast its aircraft was at a switch:\n${_out}")
endif()
if(CMAKE_MATCH_1 LESS_EQUAL 25)
    message(FATAL_ERROR "the aircraft was carried at ${CMAKE_MATCH_1} m/s at a switch: too slow "
                        "for a step without the blend to pass 5 m, so the bound tests "
                        "nothing:\n${_out}")
endif()
if(_step GREATER_EQUAL 5)
    message(FATAL_ERROR "what the client showed stepped ${_step} m at a switch, the bound 5 m:\n${_out}")
endif()
message(STATUS "handed aircraft ${_flown} to the AI and took it back, "
               "flying it by the pilot's inputs; the largest step at a switch ${_step} m")

# client_stays_at_gate.cmake - every aeroplane with a learnt landing, joined
# on final by the client with the window and left alone, is still at the
# learnt landing's gate thirty seconds later.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -P client_stays_at_gate.cmake
#
# **Built, not hoped for.** For each aeroplane, a server of its own starts
# its one player on final to Sydney's 16R where it starts any person who
# joins on final and says no distance (`--players-on-final YSSY/16R`): where
# her gate holds her thirty seconds (sim::learnt_gate_start_m, 2.26 to 2.35
# miles). From the gate's middle, two miles, where it started her until
# 2026-11-11, she flew out of its near edge (1.7 miles) in about seventeen
# seconds however well she held the rest. The client with the window - the
# window client's own program, run --headless as every window test runs it
# - joins in that aeroplane and touches nothing, but for L thirty seconds in
# (`--press-after 30 L`), which asks the server for the learnt landing:
# the server hands her over only if its own aircraft is at the gate then -
# on the centreline, the glidepath, the runway's heading within 5 degrees,
# her speed and her landing flap - and otherwise refuses her, saying why.
# So the server's word is the verdict, on its own aircraft, not the
# client's prediction of it.
#
# **Why it is built so**: until 2026-10-21 a client's first input sent its
# own pitch trim and no roll or yaw trim, and a C172P joined on final and
# left alone was 8.9 degrees off the runway's heading six seconds in
# (without the roll and yaw trim) or 9.6 degrees nose down and 11 kt fast
# (without the pitch trim) - out of the gate within seconds either way
# (docs/status/2026-11-11-a-172-joined-on-final-stays-at-the-gate.md).
#
# **Every aeroplane with a gate**: the gate is the learnt landing's, and an
# aeroplane has one where the data has a policy for it (rl/NAME-landing.txt).
# The test walks those it finds and fails if they are not exactly the ones
# named below; every other aeroplane in the catalogue (aircraft/NAME.aircraft)
# is named as having no gate to stay at, and the two lists together must be
# the whole catalogue.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake): another
# build's run of this test on the same port waits for it.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

# The aeroplanes with a learnt landing, and so a gate; and the rest, with
# none - the server refuses them the learnt landing outright.
set(_gated c172p c182 j3cub pa28)
set(_no_gate 737-300 747-400 787-8 a320 a380 b2 f15c f22 f35b learjet35a mosquito-fb6
             short_s23)

file(GLOB _policies RELATIVE "${DATA}/rl" "${DATA}/rl/*-landing.txt")
set(_found "")
foreach(_p IN LISTS _policies)
    string(REGEX REPLACE "-landing\\.txt$" "" _p "${_p}")
    list(APPEND _found "${_p}")
endforeach()
list(SORT _found)
if(NOT _found STREQUAL "${_gated}")
    message(FATAL_ERROR "the aeroplanes with a learnt landing are ${_found}, and this test "
                        "flies ${_gated}: add the new one here")
endif()
file(GLOB _catalogue RELATIVE "${DATA}/aircraft" "${DATA}/aircraft/*.aircraft")
set(_every "")
foreach(_a IN LISTS _catalogue)
    string(REGEX REPLACE "\\.aircraft$" "" _a "${_a}")
    list(APPEND _every "${_a}")
endforeach()
list(SORT _every)
set(_named ${_gated} ${_no_gate})
list(SORT _named)
if(NOT _every STREQUAL "${_named}")
    message(FATAL_ERROR "the catalogue is ${_every}; this test names ${_named}")
endif()
list(LENGTH _every _space)
list(LENGTH _gated _wanted)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/gate.sqlite")
file(REMOVE "${_store}")

# **A distance out said** (`@MILES`), as the server reads it: one it
# takes, and one it refuses; and none said.
execute_process(COMMAND "${SERVER}" --dry-run --players-on-final YSSY/16R@2.35
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR NOT _out MATCHES "on final  YSSY/16R, the first 2\\.35 miles out\n")
    message(FATAL_ERROR "the server did not read 2.35 miles out:\n${_out}${_err}")
endif()
execute_process(COMMAND "${SERVER}" --dry-run --players-on-final YSSY/16R@0.2
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(_rc EQUAL 0 OR NOT _err MATCHES "wants its miles out from 0\\.5 to 10")
    message(FATAL_ERROR "the server took 0.2 miles out:\n${_out}${_err}")
endif()
execute_process(COMMAND "${SERVER}" --dry-run --players-on-final YSSY/16R
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR NOT _out MATCHES "on final  YSSY/16R, the first where her gate holds her 30 s\n")
    message(FATAL_ERROR "the server did not say its own start:\n${_out}${_err}")
endif()

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
            --players-on-final YSSY/16R
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

set(ENV{LSAN_OPTIONS} "exitcode=0")
# The client connects once the server is flying (client.cmake says why),
# through a relay that holds nothing back (glideslope_impair, at PORT + 1),
# which passes the server's standard output on to standard error: so the
# client's is read from the one and the server's from the other.
math(EXPR _relay "${PORT} + 1")
port_lock_hold(${_relay})
set(_ready "${WORK}/flying")
set(_flown 0)
set(_said "")
foreach(_aircraft IN LISTS _gated)
    set(_shot "${WORK}/${_aircraft}.bmp")
    file(REMOVE "${_ready}" "${_shot}")
    # **Quit ten seconds after asking**: the server's answer is on its way
    # in milliseconds, and its verdict is in its own output whether or not
    # the client has heard it by then. The shot is never reached; the
    # headless client needs one named.
    execute_process(
        COMMAND "${SERVER}" --port ${PORT} --seconds 900 --until-empty --ai 1 --headless
                --data "${DATA}" --timeout 30 --store "${_store}" --ready-file "${_ready}"
                --players-on-final YSSY/16R
        COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
                --seed 1 --until-input-ends --seconds 900
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 320x200
                --aircraft ${_aircraft} --shot "${_shot}" --shot-at 999999
                --quit-at 4800 --view cockpit --press-after 30 L
                --after-ready "${_ready}" --server 127.0.0.1 ${_relay} --server-key ${_key}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    port_listened("${_out}" "${_err}")
    if(_err MATCHES "no GPU|could not create|no device")
        message(STATUS "the client cannot draw here: ${_err}")
        cmake_language(EXIT 77)
    endif()
    glideslope_judge_leaks("${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${_aircraft}: the client exited ${_rc}:\n${_out}\n${_err}")
    endif()
    if(NOT _out MATCHES "the server's levers kept: throttle [0-9.]+, flaps [0-9.]+, pitch trim -?[0-9.]+\n")
        message(FATAL_ERROR "${_aircraft}: the client did not keep the server's levers:\n${_out}")
    endif()
    # **Touched nothing**: L, once, thirty seconds in, and no other key.
    string(REGEX MATCHALL "glideslope: pressed [^\n]*\n" _presses "${_out}")
    # **Pressed by 30.5 s**: the client says the time to a tenth, on the
    # flight's clock since joining; a press made later would be judged
    # later, at least a second inside the margin every one has (below).
    if(NOT _presses MATCHES "^glideslope: pressed L, 30\\.[0-4] s in\n$")
        message(FATAL_ERROR "${_aircraft}: the client pressed ${_presses}, not L alone by 30.5 s")
    endif()
    if(NOT _err MATCHES "slot 0.s ${_aircraft} starts on final to YSSY 16R (2\\.[23][0-9]) miles out")
        message(FATAL_ERROR "${_aircraft}: the server did not start her where her gate holds "
                            "her:\n${_err}")
    endif()
    set(_out_nm "${CMAKE_MATCH_1}")
    if(_err MATCHES "aircraft ([0-9]+) not handed to the learnt landing: ([^\n]*)")
        message(FATAL_ERROR "${_aircraft}: left alone, she was not at the gate thirty seconds "
                            "in: ${CMAKE_MATCH_2}")
    endif()
    if(NOT _err MATCHES "aircraft [0-9]+ handed to the learnt landing, on final to YSSY 16R, ([0-9.]+) miles out, (-?[0-9]+) m before the gate's near edge")
        message(FATAL_ERROR "${_aircraft}: the server never judged her at its gate:\n${_err}")
    endif()
    set(_then_nm "${CMAKE_MATCH_1}")
    set(_spare_m "${CMAKE_MATCH_2}")
    # **At least a few seconds to spare**: 100 m, three seconds and more at
    # every one's approach speed.
    if(_spare_m LESS 100)
        message(FATAL_ERROR "${_aircraft}: at the gate, but only ${_spare_m} m before its near "
                            "edge")
    endif()
    math(EXPR _flown "${_flown} + 1")
    string(APPEND _said " ${_aircraft} (from ${_out_nm} to ${_then_nm} miles, ${_spare_m} m to "
                        "spare)")
endforeach()

if(NOT _flown EQUAL _wanted)
    message(FATAL_ERROR "${_flown} of ${_wanted} aeroplanes with a gate flown")
endif()
message(STATUS "of ${_space} aeroplanes, the ${_wanted} with a learnt landing's gate each "
               "joined on final, left alone, and still at the gate 30 s "
               "in:${_said}; the rest have no gate (${_no_gate})")

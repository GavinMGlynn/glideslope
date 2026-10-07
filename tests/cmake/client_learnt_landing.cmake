# client_learnt_landing.cmake - the client with the window, started on final,
# keeps the landing flap, works the flaps from the keyboard, is refused the
# learnt landing and told why on its HUD, and is handed to it with L and
# landed.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -P client_learnt_landing.cmake
#
# **Built, not hoped for.** The server starts its player on final to
# Sydney's 16R (`--players-on-final YSSY/16R`): two miles out, the middle of
# the learnt landing's gate, trimmed with the landing flap. The client with
# the window joins and presses keys as a player's finger would
# (`--press-after S KEY`: the key's event, and the key held for one pass of
# its frame loop), on the simulation's clock:
#
#   R at 1.0 s   the flaps up a notch, to two thirds
#   L at 1.3 s   the learnt landing asked for with the flaps running in: the
#                server refuses it, and the client must say why and show it
#                on its HUD - REFUSED, and the flaps named
#   F at 1.6 s   the flaps down a notch, to the landing flap again
#   L at 2.8 s   asked again, with the flaps out and still: the server hands
#                her over, and the client must say so, its HUD reading
#                FLYING AI LEARNT LANDING
#
# It must first have kept the server's flaps, full, on joining - begun at its
# own, up, it ran them in with its first input and was never at the gate. Its
# shot is held until the learnt landing has her at rest (`--shot-once-landed`,
# waiting on that event, up to five minutes of the flight), and the server
# must say it touched her down within 5 m of the centreline under 300 ft/min
# and stopped her on the runway, as the command-line client's test holds it
# (server_learnt_landing.cmake).
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/learnt.sqlite")
set(_shot "${WORK}/learnt.bmp")
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
set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 900 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}"
            --players-on-final YSSY/16R
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 0 --jitter 0 --loss 0
            --seed 1 --until-input-ends --seconds 900
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at 1200 --shot-once-landed --view cockpit
            --press-after 1.0 R --press-after 1.3 L --press-after 1.6 F
            --press-after 2.8 L
            --after-ready "${_ready}" --server 127.0.0.1 ${_relay} --server-key ${_key}
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

# **Joined with the server's flaps**, full.
if(NOT _out MATCHES "the server's levers kept: throttle [0-9.]+, flaps 1\\.00\n")
    message(FATAL_ERROR "the client did not keep the server's landing flap on joining:\n${_out}")
endif()

# **Refused with the flaps running in, and told why**, here and on its HUD.
foreach(_said IN ITEMS "pressed R, 1\\.[0-9] s in" "pressed L, 1\\.[0-9] s in"
                      "pressed F, [12]\\.[0-9] s in")
    if(NOT _out MATCHES "${_said}")
        message(FATAL_ERROR "the client never said it ${_said}:\n${_out}")
    endif()
endforeach()
if(NOT _out MATCHES "the server refused the learnt landing: not at the learnt landing's gate: YSSY 16R: flaps at ([0-9]+)%; the gate is the landing flap, 100%\n")
    message(FATAL_ERROR "the client was not told it was refused for its flaps:\n${_out}")
endif()
set(_flaps_at "${CMAKE_MATCH_1}")
string(FIND "${_out}" "the server refused the learnt landing" _refused)
string(SUBSTRING "${_out}" ${_refused} -1 _after_refusal)
if(NOT _after_refusal MATCHES "the HUD, of the learnt landing:\n(glideslope: the HUD reads [^\n]*\n)*glideslope: the HUD reads FLYING PILOT\nglideslope: the HUD reads REFUSED: NOT AT THE\nglideslope: the HUD reads LEARNT LANDINGS GATE:\nglideslope: the HUD reads YSSY 16R: FLAPS AT ${_flaps_at};\n")
    message(FATAL_ERROR "the HUD did not show why it was refused:\n${_after_refusal}")
endif()
# The flaps the keyboard set: two thirds then, by R.
if(NOT _after_refusal MATCHES "^[^\n]*\nglideslope: the HUD, of the learnt landing:\n(glideslope: the HUD reads [^\n]*\n)*glideslope: the HUD reads FLAPS 0\\.67\n")
    message(FATAL_ERROR "R did not put the flaps at two thirds:\n${_after_refusal}")
endif()

# **Handed over at the second L**, with the flaps back out by F.
string(FIND "${_out}" "the server says the learnt landing has this aircraft" _handed)
if(_handed EQUAL -1 OR _handed LESS _refused)
    message(FATAL_ERROR "the client was never handed to the learnt landing after its refusal:\n${_out}")
endif()
string(SUBSTRING "${_out}" ${_handed} -1 _after_handed)
if(NOT _after_handed MATCHES "the HUD, of the learnt landing:\n(glideslope: the HUD reads [^\n]*\n)*glideslope: the HUD reads FLYING AI LEARNT LANDING\n")
    message(FATAL_ERROR "handed over, the HUD did not say the learnt landing has it:\n${_after_handed}")
endif()
if(NOT _after_handed MATCHES "is at rest, landed by the learnt landing\n")
    message(FATAL_ERROR "the client never saw its aircraft at rest:\n${_after_handed}")
endif()
if(NOT _out MATCHES "the shot drawn [0-9.]+ s past its tick; landed by the learnt landing, at rest")
    message(FATAL_ERROR "the shot was drawn before the landing was over:\n${_out}")
endif()

# **And the server landed it**, within the limits.
if(NOT _err MATCHES "aircraft ([0-9]+) not handed to the learnt landing: not at the learnt landing's gate: YSSY 16R: flaps at")
    message(FATAL_ERROR "the server did not refuse it for its flaps:\n${_err}")
endif()
set(_n "${CMAKE_MATCH_1}")
if(NOT _err MATCHES "aircraft ${_n} handed to the learnt landing, on final to YSSY 16R")
    message(FATAL_ERROR "the server never handed it over:\n${_err}")
endif()
if(NOT _err MATCHES "aircraft ${_n}: the learnt landing touched down on YSSY 16R at ([0-9]+) ft/min, ([-+][0-9.]+) m across the centreline, and stopped ([0-9]+) m along, ([-+][0-9.]+) m across")
    message(FATAL_ERROR "the server never said the learnt landing had landed it:\n${_err}")
endif()
set(_sink "${CMAKE_MATCH_1}")
set(_across "${CMAKE_MATCH_2}")
set(_along "${CMAKE_MATCH_3}")
set(_stopped_across "${CMAKE_MATCH_4}")
string(REGEX REPLACE "^[-+]" "" _across_abs "${_across}")
string(REGEX REPLACE "^[-+]" "" _stopped_abs "${_stopped_across}")
if(_sink GREATER_EQUAL 300 OR _across_abs GREATER_EQUAL 5 OR _stopped_abs GREATER_EQUAL 15
   OR _along GREATER_EQUAL 3959)
    message(FATAL_ERROR "the learnt landing was outside its limits: ${_sink} ft/min, ${_across} "
                        "m across, stopped ${_along} m along and ${_stopped_across} m across")
endif()
message(STATUS "kept full flap; R, refused at ${_flaps_at}% flap and told why on the HUD; F, "
               "and L handed over: FLYING AI LEARNT LANDING; landed ${_sink} ft/min, ${_across} "
               "m across, stopped ${_along} m along, ${_stopped_across} m across")

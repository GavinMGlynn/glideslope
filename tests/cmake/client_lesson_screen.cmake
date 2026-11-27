# client_lesson_screen.cmake - every state of the window client's lessons
# screen, drawn and read back off the frame.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DCHECK=<glideslope_lesson_check>
#         -DDATA=<data> -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -P client_lesson_screen.cmake
#
# **Built, not hoped for**: a server with one AI aircraft flying its plan, and the client with
# the window joining it in a Cessna, the lesson in turns chosen by flag
# (`--lesson light-aircraft-turns`) and every state of the screen shot the
# first time it is drawn (`--shot-lesson`): closed, before it is chosen; the
# list, chosen; the instructor settling her, and demonstrating; handed over,
# with the debrief. Then, by `--test-lesson-states fighter-turns`, a lesson
# the server refuses for her class, and the turns again, taken back from the
# instructor as it demonstrates: ended. Each frame is read back glyph by
# glyph where the screen is drawn (glideslope_lesson_check) and held to what
# the client said it drew, and what it says is held to what each state is.
# Every event is waited on; the client quits once the last state is shot.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/skip_or_fail.cmake")
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
file(MAKE_DIRECTORY "${WORK}")
set(_store "${WORK}/lesson-screen.sqlite")
set(_prefix "${WORK}/screen")
set(_states closed choosing settling demonstrating handed-over refused ended)
file(REMOVE "${_store}")
foreach(_state IN LISTS _states)
    file(REMOVE "${_prefix}-${_state}.bmp" "${_prefix}-${_state}.txt")
endforeach()

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
glideslope_warmed_up("${_rc}" "${_err}")

set(ENV{LSAN_OPTIONS} "exitcode=0")
set(_ready "${WORK}/flying")
file(REMOVE "${_ready}")
# The client's words to a file, the server's down the pipe: both are read.
set(_said "${WORK}/said.txt")
file(REMOVE "${_said}")
execute_process(
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 640x480 --aircraft c172p
            --lesson light-aircraft-turns --shot-lesson "${_prefix}"
            --test-lesson-states fighter-turns --stdout "${_said}"
            --after-ready "${_ready}" --server 127.0.0.1 ${PORT} --server-key ${_key}
    COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --ai 1 --players 1
            --headless --data "${DATA}" --timeout 30 --store "${_store}" --ready-file "${_ready}"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _server ERROR_VARIABLE _err)
port_listened("${_server}" "${_err}")
file(WRITE "${WORK}/server.txt" "${_server}")
set(_out "")
if(EXISTS "${_said}")
    file(READ "${_said}" _out)
endif()
list(GET _rcs 0 _rc)
if(NOT EXISTS "${_prefix}-closed.bmp")
    if(_err MATCHES "no GPU|could not|device")
        glideslope_cannot_draw("${_err}")
    endif()
    message(FATAL_ERROR "the client drew no lessons screen:\n${_err}\n${_out}")
endif()
glideslope_judge_leaks("${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()

# **Every state, shot and read back**: seven, and the count stated.
set(_shot 0)
foreach(_state IN LISTS _states)
    if(NOT EXISTS "${_prefix}-${_state}.bmp" OR NOT EXISTS "${_prefix}-${_state}.txt")
        message(FATAL_ERROR "the lessons screen was never drawn ${_state}:\n${_out}")
    endif()
    execute_process(COMMAND "${CHECK}" "${_prefix}-${_state}.bmp" "${_prefix}-${_state}.txt"
                    RESULT_VARIABLE _check_rc OUTPUT_VARIABLE _check_out
                    ERROR_VARIABLE _check_err)
    if(NOT _check_rc EQUAL 0)
        message(FATAL_ERROR "the lessons screen, ${_state}, on the frame is not the one "
                            "drawn:\n${_check_out}${_check_err}")
    endif()
    file(READ "${_prefix}-${_state}.txt" _said_${_state})
    math(EXPR _shot "${_shot} + 1")
endforeach()
list(LENGTH _states _all)
if(NOT _shot EQUAL _all OR NOT _all EQUAL 7)
    message(FATAL_ERROR "${_shot} of the screen's ${_all} states were shot and read")
endif()

# **And each says what it is.**
string(REPLACE "\n" " " _flat_refused "${_said_refused}")
string(REPLACE "\n" " " _flat_ended "${_said_ended}")
string(REGEX REPLACE " +" " " _flat_refused "${_flat_refused}")
string(REGEX REPLACE " +" " " _flat_ended "${_flat_ended}")
if(NOT _said_closed STREQUAL "")
    message(FATAL_ERROR "the screen closed drew:\n${_said_closed}")
endif()
foreach(_want
        "choosing|^LESSONS FOR THE C172P, LIGHT-AIRCRAFT\n"
        "choosing|\n\\+ TURNS IN A LIGHT AEROPLANE\n"
        "choosing|F3 NEXT, RETURN FLY IT, F2 CLOSE\n"
        "settling|^TURNS IN A LIGHT AEROPLANE\nTHE INSTRUCTOR HAS CONTROL\nSETTLING HER BEFORE THE LESSON\n"
        "demonstrating|^TURNS IN A LIGHT AEROPLANE\nTHE INSTRUCTOR HAS CONTROL\nSTAGE 1 OF 3: "
        "handed-over|^TURNS IN A LIGHT AEROPLANE\nHANDED OVER: YOU HAVE CONTROL\nDEBRIEF: 3 OF 3 STAGES FLOWN\n"
        "handed-over|  FLOWN TO THE BOOK: NOTHING TO DO DIFFERENTLY\n")
    string(FIND "${_want}" "|" _bar)
    string(SUBSTRING "${_want}" 0 ${_bar} _state)
    math(EXPR _from "${_bar} + 1")
    string(SUBSTRING "${_want}" ${_from} -1 _pattern)
    string(REPLACE "-" "_" _var "${_state}")
    if(_state STREQUAL "handed-over")
        set(_text "${_said_handed-over}")
    else()
        set(_text "${_said_${_state}}")
    endif()
    if(NOT _text MATCHES "${_pattern}")
        message(FATAL_ERROR "the screen ${_state} does not say /${_pattern}/:\n${_text}")
    endif()
endforeach()
if(NOT _flat_refused MATCHES "^LESSON REFUSED: FIGHTER-TURNS .*C172P IS LIGHT-AIRCRAFT, AND FIGHTER-TURNS TEACHES FIGHTER")
    message(FATAL_ERROR "the screen refused does not say why:\n${_said_refused}")
endif()
if(NOT _flat_ended MATCHES "^LESSON ENDED: TURNS IN A LIGHT AEROPLANE TAKEN BACK FROM THE INSTRUCTOR")
    message(FATAL_ERROR "the screen ended does not say why:\n${_said_ended}")
endif()
# **The server's word, each**: what the client says it was told.
foreach(_told "light-aircraft-turns: settling" "light-aircraft-turns: demonstrating, stage 1 of 3"
              "light-aircraft-turns: demonstrating, stage 3 of 3"
              "light-aircraft-turns: handed-over, 3 of 3 stages flown"
              "fighter-turns: refused; c172p is light-aircraft, and fighter-turns teaches fighter"
              "light-aircraft-turns: ended; taken back from the instructor"
              "light-aircraft-turns: ended; handed to the AI")
    if(NOT _out MATCHES "the server says of the lesson ${_told}")
        message(FATAL_ERROR "the client was never told '${_told}':\n${_out}")
    endif()
endforeach()
# **Handed to the AI as the instructor demonstrates, the AI has her**: the
# server's hand-over made (sim::Controller::to_ai, holding what she is
# doing), not the lesson's last asking left flying her.
if(NOT _server MATCHES "lesson light-aircraft-turns ended: handed to the AI\naircraft ([0-9]+) handed to the AI\n")
    message(FATAL_ERROR "handed to the AI during the lesson, the AI was not given her:\n"
                        "${_server}")
endif()
message(STATUS "every one of the lessons screen's ${_all} states was drawn, read back off "
               "its frame and says what it is")

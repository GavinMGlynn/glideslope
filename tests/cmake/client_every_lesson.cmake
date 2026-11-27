# client_every_lesson.cmake - every lesson there is, chosen in the window
# client, flown first by the server's instructor, handed over and debriefed.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DCLI=<glideslope_cli>
#         -DCHECK=<glideslope_lesson_check> -DDATA=<data> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DDRIVER=<gpu driver> [-DPACE=<factor>]
#         [-DFROM=<n> -DEVERY=<k>] -P client_every_lesson.cmake
#
# **Every lesson, not a sample**: the lessons are those `glideslope_cli
# lessons` lists, each with the aeroplane that teaches it - the first of its
# class able to be taught it - and for each a server with one AI aircraft flying its plan and
# the client with the window joining it in that aeroplane, the lesson chosen
# by flag (`--lesson`). The client must be told the instructor settling her,
# where the lesson begins in the air, and flying every stage of it in turn;
# then the hand-over, 'N of N stages flown'; and it must draw the screen
# handed over with the debrief, read back off its frame
# (glideslope_lesson_check). The server must say it handed her over. The
# count of lessons flown is asserted against the count there are.
#
# **Events, not times**: the client quits once its screen shows the lesson
# handed over (`--quit-once-lesson-over`), and the server once its client
# has gone (`--until-empty`). **The server runs ahead of real time**
# (`--test-pace PACE`, 8 unless told): a circuit is minutes of flying, and
# nothing here is predicted - the instructor flies her, and the client
# draws her from the server's word until the hand-over, when it quits.
#
# **In pieces**, with FROM and EVERY: lessons FROM, FROM + EVERY, ... of the
# list, from 0, so that ctest runs the pieces side by side; the pieces of
# one EVERY cover the list between them, and each says how many it flew of
# how many it was given.
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
if(NOT DEFINED PACE)
    set(PACE 8)
endif()
if(NOT DEFINED FROM)
    set(FROM 0)
endif()
if(NOT DEFINED EVERY)
    set(EVERY 1)
endif()
file(MAKE_DIRECTORY "${WORK}")
set(_store "${WORK}/every-lesson.sqlite")
file(REMOVE "${_store}")

# The lessons, and who teaches each.
execute_process(COMMAND "${CLI}" --data "${DATA}" lessons
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _list ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR NOT _list MATCHES "\n([0-9]+) lessons\n")
    message(FATAL_ERROR "glideslope_cli lessons did not list them (${_rc}):\n${_list}${_err}")
endif()
set(_there ${CMAKE_MATCH_1})
# A row a line - an id, the aeroplane, the name - whose id is a lesson's
# file; JSBSim's words on loading a model are among them, and are not rows.
string(REPLACE ";" "," _list_lines "${_list}")
string(REPLACE "\n" ";" _list_lines "${_list_lines}")
set(_rows "")
foreach(_line IN LISTS _list_lines)
    # Two ifs: a match's groups are read after it, not in the same if().
    if(_line MATCHES "^([a-z0-9-]+) +([a-z0-9_-]+|\\(none\\)) +")
        set(_id "${CMAKE_MATCH_1}")
        set(_by "${CMAKE_MATCH_2}")
        if(EXISTS "${DATA}/lessons/${_id}.lesson")
            list(APPEND _rows "${_id} ${_by}")
        endif()
    endif()
endforeach()
list(LENGTH _rows _rows_n)
if(NOT _rows_n EQUAL _there)
    message(FATAL_ERROR "${_rows_n} rows read of the ${_there} lessons listed:\n${_list}")
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
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
glideslope_warmed_up("${_rc}" "${_err}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

set(_given 0)
set(_flown 0)
set(_index 0)
foreach(_row IN LISTS _rows)
    math(EXPR _mine "(${_index} - ${FROM}) % ${EVERY}")
    math(EXPR _index "${_index} + 1")
    if(_index LESS_EQUAL FROM OR NOT _mine EQUAL 0)
        continue()
    endif()
    math(EXPR _given "${_given} + 1")
    string(REGEX MATCH "^([a-z0-9-]+) (.+)$" _ "${_row}")
    set(_lesson "${CMAKE_MATCH_1}")
    set(_aircraft "${CMAKE_MATCH_2}")
    if(_aircraft STREQUAL "(none)")
        message(FATAL_ERROR "no aeroplane teaches ${_lesson}")
    endif()
    # How many stages it has: its file's `stage` lines.
    file(STRINGS "${DATA}/lessons/${_lesson}.lesson" _stage_lines REGEX "^stage ")
    list(LENGTH _stage_lines _stages)

    set(_prefix "${WORK}/${_lesson}")
    set(_said "${WORK}/${_lesson}-client.txt")
    set(_ready "${WORK}/flying")
    file(REMOVE "${_ready}" "${_said}" "${_prefix}-handed-over.bmp" "${_prefix}-handed-over.txt")
    execute_process(
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
                --aircraft ${_aircraft} --lesson ${_lesson} --quit-once-lesson-over
                --shot-lesson "${_prefix}" --stdout "${_said}"
                --after-ready "${_ready}" --server 127.0.0.1 ${PORT} --server-key ${_key}
        COMMAND "${SERVER}" --port ${PORT} --seconds 3600 --until-empty --ai 1 --players 1
                --headless --data "${DATA}" --timeout 30 --store "${_store}"
                --ready-file "${_ready}" --test-pace ${PACE}
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _server ERROR_VARIABLE _err)
    port_listened("${_server}" "${_err}")
    file(WRITE "${WORK}/${_lesson}-server.txt" "${_server}")
    if(EXISTS "${_said}")
        file(READ "${_said}" _out)
    else()
        set(_out "")
    endif()
    list(GET _rcs 0 _rc)
    if(NOT EXISTS "${_prefix}-closed.bmp")
        if(_err MATCHES "no GPU|could not|device")
            glideslope_cannot_draw("${_err}")
        endif()
        message(FATAL_ERROR "${_lesson}: the client drew no lessons screen:\n${_err}\n${_out}")
    endif()
    glideslope_judge_leaks("${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${_lesson}: the client exited ${_rc}:\n${_out}\n${_err}")
    endif()
    if(NOT _out MATCHES "the server gave this client aircraft [0-9]+, the ${_aircraft}\n")
        message(FATAL_ERROR "${_lesson}: the client was not given the ${_aircraft}:\n${_out}")
    endif()
    # **The instructor seen flying it**, every stage in turn.
    foreach(_k RANGE 1 ${_stages})
        if(NOT _out MATCHES "the server says of the lesson ${_lesson}: demonstrating, stage ${_k} of ${_stages}\n")
            message(FATAL_ERROR "${_lesson}: the client was never told the instructor flies "
                                "stage ${_k} of ${_stages}:\n${_out}\n${_server}")
        endif()
    endforeach()
    # **The hand-over**, all of it flown, and the debrief drawn.
    if(NOT _out MATCHES "the server says of the lesson ${_lesson}: handed-over, ${_stages} of ${_stages} stages flown([^\n]*)\n")
        message(FATAL_ERROR "${_lesson}: the client was never told it was handed over with "
                            "every stage flown:\n${_out}\n${_server}")
    endif()
    string(REGEX REPLACE "^; " "" _debrief "${CMAKE_MATCH_1}")
    if(NOT _server MATCHES "aircraft [0-9]+'s lesson ${_lesson}: handed over at ([0-9.]+) s")
        message(FATAL_ERROR "${_lesson}: the server did not say it handed her over:\n${_server}")
    endif()
    set(_at "${CMAKE_MATCH_1}")
    if(NOT EXISTS "${_prefix}-handed-over.bmp")
        message(FATAL_ERROR "${_lesson}: the screen was never drawn handed over:\n${_out}")
    endif()
    execute_process(COMMAND "${CHECK}" "${_prefix}-handed-over.bmp" "${_prefix}-handed-over.txt"
                    RESULT_VARIABLE _check_rc OUTPUT_VARIABLE _check_out ERROR_VARIABLE _check_err)
    if(NOT _check_rc EQUAL 0)
        message(FATAL_ERROR "${_lesson}: the screen handed over on the frame is not the one "
                            "drawn:\n${_check_out}${_check_err}")
    endif()
    file(READ "${_prefix}-handed-over.txt" _screen)
    if(NOT _screen MATCHES "HANDED OVER: YOU HAVE CONTROL\nDEBRIEF: ${_stages} OF ${_stages} STAGES FLOWN\n")
        message(FATAL_ERROR "${_lesson}: the screen does not show the hand-over and the "
                            "debrief:\n${_screen}")
    endif()
    if(_debrief STREQUAL "")
        set(_debrief "flown to the book")
    endif()
    message(STATUS "${_lesson} in the ${_aircraft}: ${_stages} stages flown by the "
                   "instructor, handed over at ${_at} s; debrief: ${_debrief}")
    math(EXPR _flown "${_flown} + 1")
endforeach()

# **Coverage, asserted**: every lesson this piece was given was flown, and
# the pieces of one EVERY are the whole list.
math(EXPR _whole "(${_there} - ${FROM} + ${EVERY} - 1) / ${EVERY}")
if(NOT _flown EQUAL _given OR NOT _given EQUAL _whole OR _there LESS 42)
    message(FATAL_ERROR "${_flown} of the ${_given} lessons given flown, of the ${_whole} this "
                        "piece holds of ${_there}")
endif()
message(STATUS "${_flown} of ${_given} lessons flown by the instructor, handed over and "
               "debriefed: lessons ${FROM}, +${EVERY}, of ${_there}")

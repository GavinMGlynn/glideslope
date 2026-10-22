# client_loading.cmake - the client with the window flies the F-35B at the
# loading she is given: asked for alone, told by a server's AIRCRAFT, and
# made again on a take-over of the same aeroplane loaded otherwise.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope> -DDATA=<data>
#         -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DDRIVER=<gpu driver> -DCASE=ALONE|SERVER|TAKE_OVER -P client_loading.cmake
#
# **The figure is her figures file's**, read here from the data the client
# reads (`figures/f35b.xml`'s `vertical_landing`, its `total_lbs`), not
# written into the test. The client says what she weighs as each flight of
# hers begins (`the f35b's flight begins at N lb, loaded NAME`); every case
# also flies her alone at her model's own loading first, so that the figure
# is shown to be one a flight would not have reached without it.
#
# - ALONE: `--aircraft f35b --loading vertical_landing`, no server. Her
#   flight begins at the figure.
# - SERVER: the same asked of a server (one AI Cessna). Her flight begins at
#   the figure, loaded as the server's AIRCRAFT says - the server's word, not
#   the command line's, since on a server it wins.
# - TAKE_OVER: the same, on a server whose one AI aircraft is an F-35B, which
#   the AI flies at her model's own loading: the client rides along in it
#   (`--ride-along`) and takes it over (`--take-over-after 6`). Her flight is
#   made again - the same aeroplane, loaded otherwise - and begins at her
#   model's own weight, loaded as her model is: not the figure kept. The
#   F-35B it left is the AI's now, under an AI's number, at the
#   vertical_landing loading; the client rides along in it and takes it over:
#   her flight made again, and begins at the figure - the loading the
#   aircraft taken over carried, not her model's own.
#
#   **On events, not times.** The ride along to the one left is asked in the
#   same pass as the first take-over (`--next-aircraft-after 6`), so it is
#   always asked unanswered and held until the server's answer is taken in -
#   the sky that answer gives, where the one left has its new number - and
#   the second take-over (`--take-over-after 6` again) waits for the first's
#   answer and for that ride along. Timed (9 s and 12 s), a machine slow to
#   build the flight taken over rode along in the number the one left had
#   given up, and was refused it (CI, 2026-10-22).
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
file(MAKE_DIRECTORY "${WORK}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

# Her figure, from the file the client reads it from.
file(READ "${DATA}/figures/f35b.xml" _figures)
if(NOT _figures MATCHES "<loading name=\"vertical_landing\" total_lbs=\"([0-9]+)\"")
    message(FATAL_ERROR "the F-35B's figures name no vertical_landing loading")
endif()
set(_figure "${CMAKE_MATCH_1}")

function(skip_when_no_gpu shot err out)
    if(NOT EXISTS "${shot}")
        if(err MATCHES "no GPU|could not|device")
            message(STATUS "the client cannot draw here: ${err}")
            cmake_language(EXIT 77)
        endif()
        message(FATAL_ERROR "the client drew nothing:\n${err}\n${out}")
    endif()
endfunction()

# Every "flight begins" line the client said, as WEIGHT;LOADING pairs.
function(begun out result)
    string(REGEX MATCHALL "glideslope: the f35b's flight begins at [0-9]+ lb, loaded [^\n]*\n"
           _lines "${out}")
    set(_pairs)
    foreach(_line IN LISTS _lines)
        string(REGEX MATCH "at ([0-9]+) lb, loaded ([^\n]*)" _ "${_line}")
        list(APPEND _pairs "${CMAKE_MATCH_1}:${CMAKE_MATCH_2}")
    endforeach()
    set(${result} "${_pairs}" PARENT_SCOPE)
endfunction()

# Alone, for a short flight: what her flight began at.
function(fly_alone name result)
    set(_shot "${WORK}/loading-${name}.bmp")
    file(REMOVE "${_shot}")
    execute_process(
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 320x240
                --screen flight --aircraft f35b ${ARGN} --shot-at 120 --shot "${_shot}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    glideslope_skip_when_not_downloaded("${_rc}" "${_err}")
    skip_when_no_gpu("${_shot}" "${_err}" "${_out}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope ${ARGN} exited ${_rc}\n${_out}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
    begun("${_out}" _pairs)
    set(${result} "${_pairs}" PARENT_SCOPE)
endfunction()

fly_alone(own _own)
list(LENGTH _own _n)
if(NOT _n EQUAL 1 OR NOT _own MATCHES "^([0-9]+):as her model is$")
    message(FATAL_ERROR "alone at her model's own loading, the client said '${_own}', not "
                        "one flight begun as her model is")
endif()
set(_own_lbs "${CMAKE_MATCH_1}")
if(_own_lbs EQUAL _figure)
    message(FATAL_ERROR "her model's own weight is the figure, ${_figure} lb: the test "
                        "could not tell a loading from none")
endif()

if(CASE STREQUAL "ALONE")
    fly_alone(vertical_landing _pairs --loading vertical_landing)
    if(NOT _pairs STREQUAL "${_figure}:vertical_landing")
        message(FATAL_ERROR "asked alone for her vertical_landing loading, her flight began "
                            "'${_pairs}', not at ${_figure} lb loaded vertical_landing")
    endif()
    message(STATUS "alone, her model's own ${_own_lbs} lb; at her vertical_landing loading "
                   "${_figure} lb, as her figures say")
    return()
endif()

# **On a server.**
set(_store "${WORK}/loading.sqlite")
set(_shot "${WORK}/loading-${CASE}.bmp")
set(_ready "${WORK}/flying")
file(REMOVE "${_store}" "${_shot}" "${_ready}")

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

set(_plan)
set(_take_over)
set(_shot_at 1200)
if(CASE STREQUAL "TAKE_OVER")
    # The AI's F-35B, wing-borne, north from Sydney at 5,000 ft.
    set(_plan_file "${WORK}/f35b.plan")
    file(WRITE "${_plan_file}" "aircraft f35b\n"
                               "start -33.9461 151.1772 5000 0 300\n"
                               "waypoint NORTH -33.5000 151.1772 5000 300\n")
    set(_plan --plan "${_plan_file}")
    set(_take_over --ride-along --take-over-after 6 --next-aircraft-after 6
                   --take-over-after 6)
    set(_shot_at 2400)
endif()

# **The client connects once the server is flying** (its --ready-file), as
# client_rides_along.cmake's does.
execute_process(
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 3 --store "${_store}" --ready-file "${_ready}" ${_plan}
    COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 480x300
            --shot "${_shot}" --shot-at ${_shot_at} --aircraft f35b --loading vertical_landing
            ${_take_over} --after-ready "${_ready}" --server 127.0.0.1 ${PORT}
            --server-key ${_key}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
skip_when_no_gpu("${_shot}" "${_err}" "${_out}")
glideslope_judge_leaks("${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client exited ${_rc}:\n${_out}\n${_err}")
endif()
begun("${_out}" _pairs)

# **Joined**: a client that flew alone would print the same flight line, so
# the server must have given it its aircraft, and - since on a server its word
# wins over the command line - the loading in that flight line is the
# server's AIRCRAFT's, not the command line's.
if(NOT _out MATCHES "glideslope: the server gave this client aircraft [0-9]+, the f35b\n")
    message(FATAL_ERROR "the client was not given its F-35B by the server:\n${_out}")
endif()
if(NOT _out MATCHES "glideslope: the server says she is loaded vertical_landing\n")
    message(FATAL_ERROR "the server's AIRCRAFT did not carry her vertical_landing loading:\n${_out}")
endif()

if(CASE STREQUAL "SERVER")
    if(NOT _pairs STREQUAL "${_figure}:vertical_landing")
        message(FATAL_ERROR "asked of a server, her flight began '${_pairs}', not at "
                            "${_figure} lb loaded vertical_landing:\n${_out}")
    endif()
    message(STATUS "through a server, told her vertical_landing loading and begun at "
                   "${_figure} lb (her model's own: ${_own_lbs} lb)")
    return()
endif()

if(NOT _out MATCHES "glideslope: took over aircraft ([0-9]+), the f35b")
    message(FATAL_ERROR "the client did not take the AI's F-35B over:\n${_out}")
endif()
set(_taken "${CMAKE_MATCH_1}")
set(_want "${_figure}:vertical_landing;${_own_lbs}:as her model is;${_figure}:vertical_landing")
if(NOT _pairs STREQUAL "${_want}")
    message(FATAL_ERROR "joined at her vertical_landing loading, taking over an F-35B at her "
                        "model's own and then the one she left at vertical_landing, her "
                        "flights began '${_pairs}', not '${_want}':\n${_out}")
endif()
# The ride along to the one left, asked with the take-over, held for its
# answer and then ridden: never in the number the client gave up.
string(CONCAT _held "asked to take over aircraft ${_taken}\nglideslope: W held until the "
                    "server answers the take-over of aircraft ${_taken}\n")
if(NOT _out MATCHES "${_held}")
    message(FATAL_ERROR "the ride along was not asked while the take-over was unanswered, "
                        "and held:\n${_out}")
endif()
string(CONCAT _ridden "W, held, is pressed now, aircraft ${_taken} this client's own\n"
                      "glideslope: riding along in aircraft ([0-9]+)\n")
if(NOT _out MATCHES "${_ridden}")
    message(FATAL_ERROR "W, held, did not ride along in another aircraft once answered:\n${_out}")
endif()
set(_left "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "glideslope: the server gave this client aircraft ([0-9]+), the f35b\n")
    message(FATAL_ERROR "no first aircraft:\n${_out}")
endif()
if(_left STREQUAL CMAKE_MATCH_1 OR _left STREQUAL _taken)
    message(FATAL_ERROR "riding along after the take-over, the client chose aircraft ${_left}: "
                        "the number it gave up, or its own:\n${_out}")
endif()
if(NOT _out MATCHES "glideslope: took over aircraft ${_left}, the f35b")
    message(FATAL_ERROR "the client did not take over aircraft ${_left}, the one it left:\n${_out}")
endif()
message(STATUS "joined at ${_figure} lb, vertical_landing; aircraft ${_taken}, the AI's "
               "F-35B, taken over and her flight made again at her model's own ${_own_lbs} lb; then "
               "the one she left, aircraft ${_left} now, ridden along in once the take-over "
               "was answered, taken over, and made again at ${_figure} lb")

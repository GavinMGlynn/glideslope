# client_views.cmake - the aeroplane is drawn where one view puts it.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_view_check>
#         -DDRIVER=<driver> -DVIEW=<view> -DWORK=<dir> -DCACHE=<downloads dir>
#         -DMODELS=<data/models> -P client_views.cmake
#
# Flies the flight screen headless to a fixed tick and shoots it from VIEW. For
# an outside view the same frame is shot again with --draw-aircraft off; the
# pixels that differ between the two are the aeroplane's own outline and
# nothing else's, and glideslope_view_check holds that outline to the model
# projected from the same camera. In the cockpit no aeroplane is drawn - the
# models have no interior, and its skin would be over the windscreen - so
# there the two shots are identical.
#
# **One view a test, and a separate test that they all flew the same flight.**
# This was one test shooting all seven views one after another, thirteen
# launches of the client at about half a minute each: ten minutes, which was
# the longest test in the suite by four times and put a floor under how short
# any CI shard could be. Each view is its own test now, run in parallel, and
# leaves a one-line account of the flight it flew in WORK; client_views_agree.cmake
# holds the seven accounts to one another, which is what "changing the view
# steps nothing in the flight" needs, and is the same comparison as before.
# Each view also holds its own two shots to each other: drawing the aeroplane
# or not must not change the flight either.
#
# The flight stands on the DEM and draws it, so it needs the tiles and the
# geoid, fetched into CACHE: without the network the test is skipped (exit 77)
# unless GLIDESLOPE_REQUIRE_NETWORK is set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

set(_aircraft c172p)
set(_tick 120)
set(_size 320x240)
# Five pixels, on a frame 320 across. A rendered edge and a projected vertex
# are the same place to within the pixel each lands in; a driver may round the
# edge either way; and the outline taken is the largest patch the two shots
# differ by, which can leave out a wisp drawn detached from the rest - a
# propeller blade a pixel wide - and pull an edge in by two or three.
set(_tolerance 5)

# Flies to the tick and shoots it. `view` names the view, `with` is on or off
# for the aeroplane, and the shot and what it printed are put in the variables
# named by `shot` and `said`.
function(shoot view with shot said)
    set(_shot "${WORK}/view-${DRIVER}-${view}-${with}.bmp")
    set(_said "${WORK}/view-${DRIVER}-${view}-${with}.txt")
    file(REMOVE "${_shot}" "${_said}")
    execute_process(
        COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size ${_size}
                --screen flight --aircraft ${_aircraft} --view ${view}
                --draw-aircraft ${with} --trace
                --shot-at ${_tick} --shot "${_shot}"
        RESULT_VARIABLE _rc OUTPUT_FILE "${_said}" ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0 AND _err MATCHES "could not download")
        if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
            message(STATUS "the DEM could not be had: ${_err}")
            cmake_language(EXIT 77)
        endif()
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope --view ${view} exited ${_rc}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
    set(${shot} "${_shot}" PARENT_SCOPE)
    set(${said} "${_said}" PARENT_SCOPE)
endfunction()

set(_views cockpit ahead behind left right above orbit)
if(NOT VIEW IN_LIST _views)
    message(FATAL_ERROR "VIEW is '${VIEW}'; it is one of ${_views}")
endif()

# The flight a shot flew, as the agreement test compares it.
function(flight_of said out)
    file(STRINGS "${said}" _traces REGEX "^trace tick ")
    if(NOT _traces)
        message(FATAL_ERROR "${said} traced no flight at all")
    endif()
    list(LENGTH _traces _steps)
    list(GET _traces -1 _last)
    set(${out} "${_steps} steps, ${_last}" PARENT_SCOPE)
endfunction()

# An account left by an earlier run must not stand for this one.
file(REMOVE "${WORK}/view-${DRIVER}-${VIEW}.flight")

foreach(_view IN ITEMS ${VIEW})
    shoot(${_view} on _with _with_said)

    # Every view flies the same flight. What is compared is the last state
    # traced before the shot, and how many were traced to reach it: a trace
    # line is numbered by the flight's own tick, so a view that stepped the
    # flight differently would still have a line numbered ${_tick} saying the
    # same thing - it is where the flight had got to when the frame was shot
    # that a view must not change.
    flight_of("${_with_said}" _trace)
    file(WRITE "${WORK}/view-${DRIVER}-${_view}.flight" "${_trace}\n")

    shoot(${_view} off _without _without_said)
    flight_of("${_without_said}" _trace_without)
    if(NOT _trace_without STREQUAL _trace)
        message(FATAL_ERROR
                "drawing the aeroplane or not changed the ${_view} view's flight:\n"
                "  with it:    ${_trace}\n  without it: ${_trace_without}")
    endif()

    if(_view STREQUAL "cockpit")
        # Nothing of the aeroplane is drawn from inside it, so leaving it out
        # changes nothing.
        file(SIZE "${_with}" _a)
        file(SIZE "${_without}" _b)
        file(MD5 "${_with}" _with_md5)
        file(MD5 "${_without}" _without_md5)
        if(NOT _with_md5 STREQUAL _without_md5)
            message(FATAL_ERROR
                    "the cockpit view draws the aeroplane: its shot with the "
                    "aeroplane and without it differ (${_a} and ${_b} bytes)")
        endif()
        message(STATUS "cockpit: no aeroplane drawn, as it should be")
        continue()
    endif()

    execute_process(COMMAND "${CHECK}" "${_with}" "${_without}" "${_with_said}"
                            "${MODELS}/${_aircraft}.mesh"
                            "${MODELS}/alignment.txt" ${_tolerance}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    message(STATUS "${_view}:\n${_out}${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the ${_view} view: ${_err}")
    endif()
endforeach()

message(STATUS "the ${VIEW} view draws the aeroplane where its camera puts it")

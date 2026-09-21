# client_views.cmake - the aeroplane is drawn where each view puts it.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_view_check>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -DMODELS=<data/models> -P client_views.cmake
#
# Flies the flight screen headless to a fixed tick and shoots it from every one
# of the seven views. For each of the six outside views the same frame is shot
# again with --draw-aircraft off; the pixels that differ between the two are
# the aeroplane's own outline and nothing else's, and glideslope_view_check
# holds that outline to the model projected from the same camera. In the
# cockpit no aeroplane is drawn - the models have no interior, and its skin
# would be over the windscreen - so there the two shots are identical.
#
# Changing the view steps nothing in the flight, which is held by every view
# having traced the same number of steps and left the flight in the same state
# when the frame was shot.
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
set(_first_trace "")

foreach(_view IN LISTS _views)
    shoot(${_view} on _with _with_said)

    # Every view flies the same flight. What is compared is the last state
    # traced before the shot, and how many were traced to reach it: a trace
    # line is numbered by the flight's own tick, so a view that stepped the
    # flight differently would still have a line numbered ${_tick} saying the
    # same thing - it is where the flight had got to when the frame was shot
    # that a view must not change.
    file(STRINGS "${_with_said}" _traces REGEX "^trace tick ")
    if(NOT _traces)
        message(FATAL_ERROR "the ${_view} view traced no flight at all")
    endif()
    list(LENGTH _traces _steps)
    list(GET _traces -1 _last)
    set(_trace "${_steps} steps, ${_last}")
    if(_first_trace STREQUAL "")
        set(_first_trace "${_trace}")
    elseif(NOT _trace STREQUAL _first_trace)
        message(FATAL_ERROR
                "the ${_view} view flew a different flight:\n  ${_trace}\n"
                "  against\n  ${_first_trace}")
    endif()

    shoot(${_view} off _without _without_said)

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

message(STATUS "every view draws the aeroplane where its camera puts it")

# terrain_mismatch.cmake - how far the terrain drawn is from the terrain flown.
#
#   cmake -DPROGRAM=<glideslope> -DPROVIDER=<open|ion|google> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -DPLACES=<surveyed.txt>
#         -DBOUND_MM=<millimetres> -P terrain_mismatch.cmake
#
# The bound is in millimetres because CMake's arithmetic is whole numbers
# only: "0.25 * 1000" is not a sum it can do.
#
# **The ground an aircraft meets is always the open DEM.** That is what lets a
# server and every client agree on where the ground is, whatever is drawn. A
# visual provider may put its surface somewhere else, and this measures how
# far, at the twelve surveyed runway ends of six airfields - the same places
# the DEM itself is held to.
#
# Every one of them must answer with a height, and none may be further from
# the DEM than BOUND. A provider whose key this machine has not got reports
# itself skipped - skipped, never passed.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

execute_process(
    COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 64x48
            --screen terrain --terrain ${PROVIDER}
            --at 39.9,-104.7,3000 --toward 39.89,-104.69,1622
            --mismatch "${PLACES}" --shot "${WORK}/mismatch-${PROVIDER}.bmp"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

if(NOT _rc EQUAL 0)
    if(_err MATCHES "own token|own Google Maps Platform key")
        message(STATUS "${PROVIDER} has no key on this machine: ${_err}")
        cmake_language(EXIT 77)
    endif()
    if(_err MATCHES "could not download")
        message(STATUS "the terrain could not be had: ${_err}")
        cmake_language(EXIT 77)
    endif()
endif()

# Every place it was asked about, and what it said.
string(REGEX MATCHALL "mismatch [^\n]+" _lines "${_out}")
if(NOT _lines)
    message(FATAL_ERROR "${PROVIDER} measured nothing:\n${_out}${_err}")
endif()
list(LENGTH _lines _asked)

set(_worst 0)
set(_worst_where "")
set(_no_height "")
foreach(_line IN LISTS _lines)
    # mismatch <name> <lat> <lon> drawn <m> flown <m> off <m> [not-a-height]
    if(_line MATCHES "mismatch ([^ ]+) .* off ([-+.0-9]+)( not-a-height)?$")
        set(_name "${CMAKE_MATCH_1}")
        set(_off "${CMAKE_MATCH_2}")
        if(NOT "${CMAKE_MATCH_3}" STREQUAL "")
            list(APPEND _no_height "${_name}")
            continue()
        endif()
        # Metres to millimetres, so that CMake's integers can compare them.
        string(REGEX REPLACE "^[-+]" "" _size "${_off}")
        if(_size MATCHES "^([0-9]+)\\.([0-9][0-9][0-9])$")
            math(EXPR _mm "${CMAKE_MATCH_1} * 1000 + 1${CMAKE_MATCH_2} - 1000")
        else()
            message(FATAL_ERROR "cannot read the distance in: ${_line}")
        endif()
        if(_mm GREATER _worst)
            set(_worst ${_mm})
            set(_worst_where "${_name}")
        endif()
    elseif(_line MATCHES "mismatch ([^ ]+) .* none flown")
        list(APPEND _no_height "${CMAKE_MATCH_1}")
    endif()
endforeach()

if(_no_height)
    string(REPLACE ";" ", " _named "${_no_height}")
    message(FATAL_ERROR
            "${PROVIDER} did not answer with a height at ${_named}. A surface "
            "that is not a height is not terrain that disagrees with the DEM; "
            "it is an answer that means nothing.")
endif()

set(_bound_mm ${BOUND_MM})
message(STATUS "${PROVIDER}: ${_asked} airfields, worst ${_worst} mm from the "
               "ground flown, at ${_worst_where}; held to ${_bound_mm} mm")
if(_worst GREATER _bound_mm)
    message(FATAL_ERROR
            "${PROVIDER}'s terrain is ${_worst} mm from the ground flown at "
            "${_worst_where}, beyond the ${_bound_mm} mm stated in "
            "docs/PROJECT_STATUS.md")
endif()

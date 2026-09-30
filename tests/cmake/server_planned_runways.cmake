# server_planned_runways.cmake - an AI aircraft whose airport's runways
# cannot be read is not planned, is said so, and flies the plan file; the
# server does not stop.
#
#   cmake -DSERVER=<glideslope_server> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DRECORDINGS=<dir of cbd-orbit-*.jsonl>
#         -P server_planned_runways.cmake
#
# **No network is needed to build it.** The server is given a cache of its
# own: everything in the downloads cache, linked, except OurAirports' runways,
# which is a file with no runways' columns. A file in the cache is not fetched
# again, so this is the same failure as OurAirports being unreachable with no
# copy - the runways cannot be had - reached without touching the network.
# The model is played back from its recording, so its key is not wanted.
#
# Without the DEM's tiles it reports itself skipped (exit 77), unless
# GLIDESLOPE_REQUIRE_NETWORK is set (client.cmake).

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_cache "${WORK}/cache")
file(REMOVE_RECURSE "${_cache}")
file(MAKE_DIRECTORY "${_cache}")
file(GLOB _entries RELATIVE "${CACHE}" "${CACHE}/*")
foreach(_entry IN LISTS _entries)
    if(NOT _entry STREQUAL "ourairports-runways.csv")
        # Linked where the file system lets it, copied where it does not
        # (Windows without the right to make one).
        file(CREATE_LINK "${CACHE}/${_entry}" "${_cache}/${_entry}" SYMBOLIC RESULT _linked)
        if(NOT _linked EQUAL 0)
            file(COPY "${CACHE}/${_entry}" DESTINATION "${_cache}")
        endif()
    endif()
endforeach()
file(WRITE "${_cache}/ourairports-runways.csv" "this,is,not,a,runways,file\n")
set(ENV{GLIDESLOPE_CACHE} "${_cache}")

execute_process(
    COMMAND "${SERVER}" --data "${DATA}" --headless --port 0 --ai 1
            --ai-planner 1=openai:gpt-5.4-mini-2026-03-17
            --ai-playback "1=${RECORDINGS}/cbd-orbit-openai.jsonl"
            --steps 120
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
glideslope_skip_when_not_downloaded("${_rc}" "${_err}" "cannot download|no network")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the server stopped when the runways could not be read (exit ${_rc}):\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "AI 1 cannot be planned: the runways cannot be read: [^\n]*; it flies the plan file instead")
    message(FATAL_ERROR "the aircraft was not said to be unplannable:\n${_out}")
endif()
if(_out MATCHES "planned by")
    message(FATAL_ERROR "an aircraft said it was planned with no runways:\n${_out}")
endif()
if(NOT _out MATCHES "flew c172p \\(AI 1\\)[^\n]*\n  number [0-9]+, an AI's")
    message(FATAL_ERROR "the aircraft that could not be planned did not fly the plan file:\n${_out}")
endif()
message(STATUS "runways unreadable: AI 1 is said to be unplannable and flies the plan file")

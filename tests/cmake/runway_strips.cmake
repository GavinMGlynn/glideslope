# runway_strips.cmake - assets/runways/strips.csv is what
# tools/make_runway_strips.py makes from the pinned OurAirports runways.
#
#   cmake -DROOT=<source dir> -DPYTHON=<python3, or empty> -DRUNWAYS=<runways.csv>
#         -P runway_strips.cmake
#
# Needs Python 3 and the fetched runways. Without either the test reports
# itself skipped (exit 77); in CI, GLIDESLOPE_REQUIRE_NETWORK makes missing
# runways a failure.
if(NOT PYTHON)
    message(STATUS "no Python 3 found; cannot run tools/make_runway_strips.py --check")
    cmake_language(EXIT 77)
endif()
if(NOT EXISTS "${RUNWAYS}")
    if(NOT "$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(FATAL_ERROR "the runways were not fetched to ${RUNWAYS}")
    endif()
    message(STATUS "the runways were not fetched")
    cmake_language(EXIT 77)
endif()
execute_process(COMMAND "${PYTHON}" "${ROOT}/tools/make_runway_strips.py" --runways "${RUNWAYS}" --check
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

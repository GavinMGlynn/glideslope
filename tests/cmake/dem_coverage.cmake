# dem_coverage.cmake - assets/dem/coverage.txt is what tools/make_dem_coverage.py
# makes from the pinned tile lists.
#
#   cmake -DROOT=<source dir> -DPYTHON=<python3, or empty> -DLISTS=<downloads dir>
#         -P dem_coverage.cmake
#
# Needs Python 3 and the fetched tile lists. Without either the test reports
# itself skipped (exit 77); in CI, GLIDESLOPE_REQUIRE_NETWORK makes missing
# lists a failure.
if(NOT PYTHON)
    message(STATUS "no Python 3 found; cannot run tools/make_dem_coverage.py --check")
    cmake_language(EXIT 77)
endif()
if(NOT EXISTS "${LISTS}/copernicus-dem-30m-tileList.txt")
    if(NOT "$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(FATAL_ERROR "the tile lists were not fetched to ${LISTS}")
    endif()
    message(STATUS "the tile lists were not fetched")
    cmake_language(EXIT 77)
endif()
execute_process(COMMAND "${PYTHON}" "${ROOT}/tools/make_dem_coverage.py" --lists "${LISTS}" --check
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

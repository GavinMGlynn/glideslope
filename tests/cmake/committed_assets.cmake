# committed_assets.cmake - what is committed under assets/jsbsim/ is what
# tools/make_c172p.py makes from the pinned JSBSim.
#
#   cmake -DROOT=<source dir> -DPYTHON=<python3, or empty> -P committed_assets.cmake
#
# Needs Python 3. Without it the test reports itself skipped (exit 77), never
# passed.
if(NOT PYTHON)
    message(STATUS "no Python 3 found; cannot run tools/make_c172p.py --check")
    cmake_language(EXIT 77)
endif()
execute_process(COMMAND "${PYTHON}" "${ROOT}/tools/make_c172p.py" --check
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

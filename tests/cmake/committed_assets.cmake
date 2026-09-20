# committed_assets.cmake - what is committed under assets/ is what the script
# that makes it makes: tools/make_c172p.py from the pinned JSBSim,
# tools/make_mosquito_propeller.py, or tools/make_models.py from the pinned
# FlightGear files.
#
#   cmake -DROOT=<source dir> -DPYTHON=<python3, or empty> -DSCRIPT=<tools/...py>
#         [-DARGS=<arg;arg>] -P committed_assets.cmake
#
# Needs Python 3. Without it the test reports itself skipped (exit 77), never
# passed, and a script that exits 77 itself - because what it reads has not
# been fetched - is skipped the same way.
if(NOT PYTHON)
    message(STATUS "no Python 3 found; cannot run ${SCRIPT} --check")
    cmake_language(EXIT 77)
endif()
execute_process(COMMAND "${PYTHON}" "${ROOT}/${SCRIPT}" --check ${ARGS}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(_rc EQUAL 77)
    message(STATUS "${_out}${_err}")
    cmake_language(EXIT 77)
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

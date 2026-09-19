# committed_assets.cmake - what is committed under assets/jsbsim/ is what the
# script that makes it makes: tools/make_c172p.py from the pinned JSBSim, or
# tools/make_mosquito_propeller.py.
#
#   cmake -DROOT=<source dir> -DPYTHON=<python3, or empty> -DSCRIPT=<tools/...py>
#         -P committed_assets.cmake
#
# Needs Python 3. Without it the test reports itself skipped (exit 77), never
# passed.
if(NOT PYTHON)
    message(STATUS "no Python 3 found; cannot run ${SCRIPT} --check")
    cmake_language(EXIT 77)
endif()
execute_process(COMMAND "${PYTHON}" "${ROOT}/${SCRIPT}" --check
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
endif()

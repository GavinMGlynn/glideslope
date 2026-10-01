# python_test.cmake - runs a Python test script, skipped without Python.
#
#   cmake -DPYTHON=<python3, or empty> -DSCRIPT=<script> "-DARGS=<a;b>" -P python_test.cmake
#
# Without Python 3 the test reports itself skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(NOT PYTHON)
    message(STATUS "no Python 3 found; cannot run ${SCRIPT}")
    cmake_language(EXIT 77)
endif()
execute_process(COMMAND "${PYTHON}" "${SCRIPT}" ${ARGS}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${SCRIPT} exited ${_rc}")
endif()

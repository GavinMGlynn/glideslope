# warnings_are_errors.cmake - build deliberate mistakes with the project's own
# warning set, in every build type, and require each to fail for the right
# reason.
#
#   cmake -DROOT=<source dir> -DWORK=<scratch dir> -DGENERATOR=<generator>
#         -DCXX=<compiler> -DCOMPILER_ID=<id> -DFRONTEND=<variant>
#         -P warnings_are_errors.cmake
#
# The probes are in tests/warnings/:
#   clean            must build - otherwise the failures below prove nothing
#   narrowing        double to float with no cast; must fail naming the warning
#   sign_conversion  int to unsigned with no cast; must fail naming the warning
#
# **One exclusion, named:** sign_conversion under MSVC itself (not clang-cl).
# MSVC has no warning for an implicit int-to-unsigned conversion of a
# non-constant at /W4 - its nearest, C4365, is off by default and fires
# throughout the standard library headers - so the probe is not built there.

set(_build_types Debug Release RelWithDebInfo MinSizeRel)
set(_probes clean narrowing sign_conversion)

# What each compiler says when the warning fires. GCC and Clang both put the
# warning's name in brackets; MSVC gives a number.
if(FRONTEND STREQUAL "MSVC" AND COMPILER_ID STREQUAL "MSVC")
    set(_says_narrowing "C4244")
else()
    set(_says_narrowing "float-conversion")
    set(_says_sign_conversion "sign-conversion")
endif()

set(_failures "")
set(_walked 0)
set(_excluded 0)

foreach(_type IN LISTS _build_types)
    set(_bin "${WORK}/${_type}")
    file(REMOVE_RECURSE "${_bin}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${ROOT}/tests/warnings" -B "${_bin}"
                -G "${GENERATOR}" "-DCMAKE_CXX_COMPILER=${CXX}"
                "-DCMAKE_BUILD_TYPE=${_type}" "-DGLIDESLOPE_ROOT=${ROOT}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        list(APPEND _failures "${_type}: the probe project did not configure\n${_out}${_err}")
        continue()
    endif()

    foreach(_probe IN LISTS _probes)
        if(_probe STREQUAL "sign_conversion" AND NOT DEFINED _says_sign_conversion)
            math(EXPR _excluded "${_excluded} + 1")
            continue()
        endif()

        execute_process(
            COMMAND "${CMAKE_COMMAND}" --build "${_bin}" --target probe_${_probe}
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        set(_all "${_out}${_err}")
        math(EXPR _walked "${_walked} + 1")

        if(_probe STREQUAL "clean")
            if(NOT _rc EQUAL 0)
                list(APPEND _failures "${_type} clean: should build, did not\n${_all}")
            endif()
        else()
            set(_says "${_says_${_probe}}")
            if(_rc EQUAL 0)
                list(APPEND _failures "${_type} ${_probe}: built, should have failed")
            elseif(NOT _all MATCHES "${_says}")
                list(APPEND _failures
                    "${_type} ${_probe}: failed, but not with '${_says}'\n${_all}")
            endif()
        endif()
    endforeach()
endforeach()

list(LENGTH _build_types _nt)
list(LENGTH _probes _np)
math(EXPR _space "${_nt} * ${_np}")
math(EXPR _accounted "${_walked} + ${_excluded}")
set(_why "")
if(_excluded GREATER 0)
    set(_why " (sign_conversion under MSVC, which has no equivalent warning)")
endif()
message(STATUS "walked ${_walked} of ${_space} probe builds, ${_excluded} excluded${_why}")
if(NOT _accounted EQUAL _space)
    list(APPEND _failures "accounted for ${_accounted} probe builds, expected ${_space}")
endif()

if(_failures)
    string(REPLACE ";" "\n----\n" _report "${_failures}")
    message(FATAL_ERROR "warnings are not errors everywhere they should be:\n${_report}")
endif()

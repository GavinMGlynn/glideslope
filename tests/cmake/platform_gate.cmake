# platform_gate.cmake - walk the platform gate's whole table.
#
#   cmake -DROOT=<source dir> -DCASES=<pointer-size|compiler-floors> -P platform_gate.cmake
#
# pointer-size     a 32-bit toolchain is refused on each of the three platforms
# compiler-floors  every compiler in GLIDESLOPE_COMPILER_FLOORS is refused one
#                  version below its floor, and accepted at it
#
# The cases are derived from the table in cmake/Platform.cmake rather than
# listed here, so a compiler added to the table is walked without anybody
# remembering to add it, and the count of cases walked is checked against the
# size of the table.

include("${ROOT}/cmake/Platform.cmake")
set(_runner "${ROOT}/tests/cmake/run_platform.cmake")

set(_failures "")
set(_walked 0)

# Run one described toolchain through the gate and check the outcome.
#   expect   ACCEPT or REFUSE
#   needles  text the combined output must contain
function(gate_case label expect needles)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${ROOT}" ${ARGN} -P "${_runner}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_all "${_out}${_err}")
    set(_problems "")
    if(expect STREQUAL "REFUSE" AND _rc EQUAL 0)
        list(APPEND _problems "was accepted, should have been refused")
    elseif(expect STREQUAL "ACCEPT" AND NOT _rc EQUAL 0)
        list(APPEND _problems "was refused, should have been accepted")
    endif()
    foreach(_needle IN LISTS needles)
        string(FIND "${_all}" "${_needle}" _at)
        if(_at EQUAL -1)
            list(APPEND _problems "output does not say '${_needle}'")
        endif()
    endforeach()
    if(_problems)
        string(REPLACE ";" "; " _p "${_problems}")
        set(_failures "${_failures}\n  ${label}: ${_p}\n----\n${_all}----" PARENT_SCOPE)
    endif()
    math(EXPR _n "${_walked} + 1")
    set(_walked ${_n} PARENT_SCOPE)
endfunction()

# One version below a floor: the last component down by one, borrowing when it
# is zero ("19.40" -> "19.39" is not needed; "20.0" -> "19", "16" -> "15").
function(below_floor floor out)
    string(REPLACE "." ";" _parts "${floor}")
    list(LENGTH _parts _len)
    math(EXPR _last "${_len} - 1")
    list(GET _parts ${_last} _v)
    while(_v EQUAL 0 AND _last GREATER 0)
        list(REMOVE_AT _parts ${_last})
        math(EXPR _last "${_last} - 1")
        list(GET _parts ${_last} _v)
    endwhile()
    math(EXPR _v "${_v} - 1")
    list(REMOVE_AT _parts ${_last})
    list(APPEND _parts ${_v})
    list(JOIN _parts "." _below)
    set(${out} "${_below}" PARENT_SCOPE)
endfunction()

# Where each compiler naturally runs, so a case describes a real toolchain.
set(_home_GNU        "Linux;x86_64;linux-x86_64")
set(_home_Clang      "Linux;x86_64;linux-x86_64")
set(_home_AppleClang "Darwin;arm64;macos-arm64")
set(_home_MSVC       "Windows;AMD64;windows-x64")

if(CASES STREQUAL "pointer-size")
    set(_expected 3)
    foreach(_os IN ITEMS "Linux;x86_64;GNU;14" "Windows;AMD64;MSVC;19.39"
                         "Darwin;arm64;AppleClang;16")
        list(GET _os 0 _name)
        list(GET _os 1 _cpu)
        list(GET _os 2 _cc)
        list(GET _os 3 _ver)
        gate_case("32-bit ${_name}" REFUSE "64-bit only;4-byte pointers"
            -DCMAKE_SIZEOF_VOID_P=4 -DCMAKE_SYSTEM_NAME=${_name}
            -DCMAKE_SYSTEM_PROCESSOR=${_cpu} -DCMAKE_CXX_COMPILER_ID=${_cc}
            -DCMAKE_CXX_COMPILER_VERSION=${_ver})
    endforeach()

elseif(CASES STREQUAL "compiler-floors")
    list(LENGTH GLIDESLOPE_COMPILER_FLOORS _compilers)
    math(EXPR _expected "${_compilers} * 2")
    foreach(_entry IN LISTS GLIDESLOPE_COMPILER_FLOORS)
        string(REPLACE "=" ";" _pair "${_entry}")
        list(GET _pair 0 _id)
        list(GET _pair 1 _floor)
        if(NOT DEFINED _home_${_id})
            list(APPEND _failures "\n  ${_id}: has a floor but no home platform in this test")
            continue()
        endif()
        list(GET _home_${_id} 0 _name)
        list(GET _home_${_id} 1 _cpu)
        list(GET _home_${_id} 2 _platform)
        below_floor("${_floor}" _below)
        set(_toolchain -DCMAKE_SIZEOF_VOID_P=8 -DCMAKE_SYSTEM_NAME=${_name}
                       -DCMAKE_SYSTEM_PROCESSOR=${_cpu} -DCMAKE_CXX_COMPILER_ID=${_id})
        gate_case("${_id} ${_below}" REFUSE
            "found:    ${_id} ${_below};required: ${_id} ${_floor} or newer"
            ${_toolchain} -DCMAKE_CXX_COMPILER_VERSION=${_below})
        gate_case("${_id} ${_floor}" ACCEPT
            "glideslope: ${_platform}, ${_id} ${_floor}"
            ${_toolchain} -DCMAKE_CXX_COMPILER_VERSION=${_floor})
    endforeach()

else()
    message(FATAL_ERROR "platform_gate.cmake: CASES must be pointer-size or compiler-floors")
endif()

message(STATUS "walked ${_walked} of ${_expected} cases")
if(NOT _walked EQUAL _expected)
    list(APPEND _failures "\n  walked ${_walked} cases, expected ${_expected}")
endif()
if(_failures)
    message(FATAL_ERROR "the platform gate is wrong:${_failures}")
endif()

# machine_stalls_cases.cmake - the cap on a machine's stalls holds: every case
# of what machine_stalls_check.cmake allows and refuses, each run as its own
# script so that a refusal (a fatal error) is seen as the exit code it is.
#
#   cmake -P machine_stalls_cases.cmake
#
# The cases: none; one stall recovered; two; three (refused); a quarter of the
# updates apart (allowed) and more (refused); a stall whose error was still
# over a metre at its last word (refused), at its 60th of 60 (refused), and
# back by its 60th (allowed); one at the end of a run, its words cut short and
# back within them (allowed); and the window client's corrections, the same
# cap.

cmake_minimum_required(VERSION 3.28)

# One stall's line, as glideslope_cli says it.
function(_stall_line _n _after _back _out)
    set(${_out} "client 1: the machine's stall ${_n}: 3500 ms, 22.6 s in; 53 words catching up and ${_after} after, 113 updates compared, the worst 5.500 m, back within 1 m from word ${_back} after; 113 corrections, the worst 1.400 m, 0 too large to hide\n" PARENT_SCOPE)
endfunction()

function(_text _stalls _apart _corrections _lines _out)
    set(_t "client 1: the machine's stalls: ${_stalls} frames of 250 ms and more the machine's, ${_apart} updates compared and ${_corrections} corrections in the server's catching up and the 60 words after it, counted apart\n")
    foreach(_l IN LISTS _lines)
        string(REPLACE "|" ";" _l "${_l}")
        string(APPEND _t "${_l}")
    endforeach()
    set(${_out} "${_t}" PARENT_SCOPE)
endfunction()

if(DEFINED CASE)
    include("${CMAKE_CURRENT_LIST_DIR}/machine_stalls_check.cmake")
    _stall_line(1 60 7 _ok1)
    _stall_line(2 60 4 _ok2)
    _stall_line(3 60 1 _ok3)
    # Lines carry semicolons; held with | in the list, put back in _text.
    string(REPLACE ";" "|" _ok1 "${_ok1}")
    string(REPLACE ";" "|" _ok2 "${_ok2}")
    string(REPLACE ";" "|" _ok3 "${_ok3}")
    if(CASE STREQUAL "none")
        _text(0 0 0 "" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "one")
        _text(1 113 113 "${_ok1}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "two")
        _text(2 120 120 "${_ok1};${_ok2}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "three")
        _text(3 120 120 "${_ok1};${_ok2};${_ok3}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "a_quarter")
        _text(1 150 150 "${_ok1}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "over_a_quarter")
        _text(1 151 151 "${_ok1}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "over_at_its_last_word")
        _stall_line(1 40 41 _l)
        string(REPLACE ";" "|" _l "${_l}")
        _text(1 93 93 "${_l}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "over_at_its_sixtieth")
        _stall_line(1 60 61 _l)
        string(REPLACE ";" "|" _l "${_l}")
        _text(1 113 113 "${_l}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "back_by_its_sixtieth")
        _stall_line(1 60 60 _l)
        string(REPLACE ";" "|" _l "${_l}")
        _text(1 113 113 "${_l}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "cut_short_and_back")
        _stall_line(1 16 7 _l)
        string(REPLACE ";" "|" _l "${_l}")
        _text(1 69 69 "${_l}" _t)
        check_machine_stalls("${_t}" updates 600 _s)
    elseif(CASE STREQUAL "window_corrections_over_a_quarter")
        _text(1 0 12 "${_ok1}" _t)
        check_machine_stalls("${_t}" corrections 44 _s)
    else()
        message(FATAL_ERROR "no case ${CASE}")
    endif()
    return()
endif()

set(_allowed none one two a_quarter back_by_its_sixtieth cut_short_and_back)
set(_refused three over_a_quarter over_at_its_last_word over_at_its_sixtieth
             window_corrections_over_a_quarter)
set(_covered 0)
foreach(_case IN LISTS _allowed _refused)
    execute_process(COMMAND "${CMAKE_COMMAND}" -DCASE=${_case} -P "${CMAKE_CURRENT_LIST_FILE}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if(_case IN_LIST _allowed AND NOT _rc EQUAL 0)
        message(FATAL_ERROR "${_case} was refused:\n${_e}")
    endif()
    if(_case IN_LIST _refused AND _rc EQUAL 0)
        message(FATAL_ERROR "${_case} was allowed")
    endif()
    math(EXPR _covered "${_covered} + 1")
endforeach()
if(NOT _covered EQUAL 11)
    message(FATAL_ERROR "${_covered} of 11 cases run")
endif()
message(STATUS "all 11 cases: 6 allowed and 5 refused, as the cap says")

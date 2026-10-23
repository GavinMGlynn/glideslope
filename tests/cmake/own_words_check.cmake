# own_words_check.cmake - every word a predicting client read on its own
# aircraft compared at its own step, or named: included by client_on_server
# (the client with the window, EVERY_WORD) and starved_prediction
# (glideslope_cli). Both say the same line (frontend/own_words.hpp).
#
#   check_own_words(TEXT MOST_DROPPED HELD OUT)
#
# **The space is every word read**, and each is in exactly one of:
# - compared at its own step - at once, or held until its step was flown;
# - before its clocks' difference could place any: its input sent before the
#   prediction began, a round trip's words - at most a second's, 25;
# - not finite, which measures nothing;
# - dropped with the prediction it was held for - at a switch, a take-back, a
#   loss, a start again, or held a second with its step never flown - at most
#   MOST_DROPPED, which the caller states;
# - still held when the client said it: at most a pass's words.
# A word placed at a step flown and no longer kept is in none of them, and
# fails the test; so does any word missing from the sum.
#
# **The situation is asserted built**: a pass read more than one word, so some
# were not put right from; and, with HELD true, some word was held until it
# was flown, so the hold was tested. OUT is set to a summary.

function(check_own_words _text _most_dropped _held_built _out)
    # Two matches: CMake keeps nine groups at most.
    if(NOT _text MATCHES "own words: ([0-9]+) read, ([0-9]+) compared at their own step, ([0-9]+) held until it was flown \\(at most ([0-9]+) steps past\\); ([0-9]+) before its clocks' difference could place them, ([0-9]+) not finite, ([0-9]+) at a step no longer kept")
        message(FATAL_ERROR "the client did not say what it did with its words:\n${_text}")
    endif()
    set(_read "${CMAKE_MATCH_1}")
    set(_compared "${CMAKE_MATCH_2}")
    set(_held "${CMAKE_MATCH_3}")
    set(_past "${CMAKE_MATCH_4}")
    set(_unplaced "${CMAKE_MATCH_5}")
    set(_not_finite "${CMAKE_MATCH_6}")
    set(_not_kept "${CMAKE_MATCH_7}")
    if(NOT _text MATCHES "at a step no longer kept, ([0-9]+) dropped \\(([^)]*)\\), ([0-9]+) still held; ([0-9]+) put right from, at most ([0-9]+) read in a pass")
        message(FATAL_ERROR "the client did not say which of its words it dropped:\n${_text}")
    endif()
    set(_dropped "${CMAKE_MATCH_1}")
    set(_why "${CMAKE_MATCH_2}")
    set(_still "${CMAKE_MATCH_3}")
    set(_put_right "${CMAKE_MATCH_4}")
    set(_most "${CMAKE_MATCH_5}")
    string(CONCAT _summary "${_read} words read, ${_compared} compared at their own step "
           "(${_held} of them held until it was flown, at most ${_past} steps past); named: "
           "${_unplaced} before they could be placed, ${_not_finite} not finite, ${_dropped} "
           "dropped (${_why}), ${_still} still held; ${_not_kept} at a step no longer kept; "
           "${_put_right} put right from, at most ${_most} read in a pass")
    if(_most LESS 2 OR NOT _put_right LESS _read)
        message(FATAL_ERROR "no pass read more than one word, so every word was put right "
                            "from and the rule went untested: ${_summary}")
    endif()
    if(_held_built AND _held EQUAL 0)
        message(FATAL_ERROR "no word was placed past the steps flown, so the hold went "
                            "untested: ${_summary}")
    endif()
    if(_read LESS 20)
        message(FATAL_ERROR "only ${_read} words were read: ${_summary}")
    endif()
    if(NOT _not_kept EQUAL 0)
        message(FATAL_ERROR "${_not_kept} words were placed at a step flown and no longer "
                            "kept: ${_summary}")
    endif()
    if(_unplaced GREATER 25)
        message(FATAL_ERROR "${_unplaced} words could not be placed, more than a second's: "
                            "${_summary}")
    endif()
    if(_dropped GREATER _most_dropped)
        message(FATAL_ERROR "${_dropped} words were dropped, more than the ${_most_dropped} "
                            "stated: ${_summary}")
    endif()
    if(_still GREATER _most)
        message(FATAL_ERROR "${_still} words still held, more than a pass's: ${_summary}")
    endif()
    math(EXPR _named "${_unplaced} + ${_not_finite} + ${_dropped} + ${_still}")
    math(EXPR _accounted "${_compared} + ${_named}")
    if(NOT _accounted EQUAL _read)
        message(FATAL_ERROR "${_read} words read, and ${_compared} compared and ${_named} "
                            "named: ${_summary}")
    endif()
    set(${_out} "${_summary}" PARENT_SCOPE)
endfunction()

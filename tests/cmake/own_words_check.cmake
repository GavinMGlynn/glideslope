# own_words_check.cmake - every word a predicting client read on its own
# aircraft compared at its own step, or named: included by client_on_server
# (the client with the window, EVERY_WORD) and starved_prediction
# (glideslope_cli). Both say the same line (frontend/own_words.hpp).
#
#   check_own_words(TEXT HEARD REPLAYS MOST_DROPPED FORCED OUT)
#
# **The space is every word read**, and each is in exactly one of:
# - compared at its own step - at once, or held until its step was flown;
# - before its clocks' difference could place any. A word is placed by the
#   input it says the server last applied, as this client began that input
#   (sim::Prediction::hear_clock); until the server has applied one this
#   prediction flew, no word has a place - the words of the first frames
#   after it began, a round trip and a frame. Never one after a word was
#   placed. At most 28: twice the worst seen, 14, in a frame of 17 words on a
#   loaded machine (docs/status, 2026-10-24);
# - not finite, which measures nothing;
# - dropped with the prediction it was held for - at a switch, a take-back, a
#   loss, a start again, or held a second with its step never flown - at most
#   MOST_DROPPED, which the caller states;
# - still held when the client said it: at most a pass's words.
# A word placed at a step flown and no longer kept is in none of them, and
# fails the test; so does any word missing from the sum.
#
# **Counted twice, apart**: HEARD is the client's own count of the words it
# heard on its own aircraft, kept where it hears them and not by
# frontend::OwnWords, so a word missed before it reached them is seen; it
# must equal the words read. REPLAYS, if given, is the client's own count of
# its corrections, which must equal the words put right from.
#
# **Put right only from the newest**: every replay is from the newest word
# read in its pass, or - newer ones held for their steps - the newest whose
# step was flown; none from any other.
#
# **The situation is asserted built.** FORCED, if given, is how many passes
# the client flew ticks behind (`--ticks-behind`, more than two words' steps):
# at least ten, and at least that many words held until they were flown, so
# the hold was tested every time. Without it (glideslope_cli, starved by its
# own slowness), a pass read more than one word, so some were not put right
# from. OUT is set to a summary.

function(check_own_words _text _heard _replays _most_dropped _forced _out)
    # Three matches: CMake keeps nine groups at most.
    if(NOT _text MATCHES "own words: ([0-9]+) read, ([0-9]+) compared at their own step, ([0-9]+) held until it was flown \\(at most ([0-9]+) steps past\\); ([0-9]+) before its clocks' difference could place them \\(([0-9]+) after one was placed\\), ([0-9]+) not finite, ([0-9]+) at a step no longer kept")
        message(FATAL_ERROR "the client did not say what it did with its words:\n${_text}")
    endif()
    set(_read "${CMAKE_MATCH_1}")
    set(_compared "${CMAKE_MATCH_2}")
    set(_held "${CMAKE_MATCH_3}")
    set(_past "${CMAKE_MATCH_4}")
    set(_unplaced "${CMAKE_MATCH_5}")
    set(_unplaced_late "${CMAKE_MATCH_6}")
    set(_not_finite "${CMAKE_MATCH_7}")
    set(_not_kept "${CMAKE_MATCH_8}")
    if(NOT _text MATCHES "at a step no longer kept, ([0-9]+) dropped \\(([^)]*)\\), ([0-9]+) still held; ([0-9]+) put right from")
        message(FATAL_ERROR "the client did not say which of its words it dropped:\n${_text}")
    endif()
    set(_dropped "${CMAKE_MATCH_1}")
    set(_why "${CMAKE_MATCH_2}")
    set(_still "${CMAKE_MATCH_3}")
    set(_put_right "${CMAKE_MATCH_4}")
    if(NOT _text MATCHES "put right from \\(([0-9]+) the newest read, ([0-9]+) the newest flown with newer held, ([0-9]+) neither\\), at most ([0-9]+) read in a pass")
        message(FATAL_ERROR "the client did not say which words it put right from:\n${_text}")
    endif()
    set(_newest "${CMAKE_MATCH_1}")
    set(_newer_held "${CMAKE_MATCH_2}")
    set(_neither "${CMAKE_MATCH_3}")
    set(_most "${CMAKE_MATCH_4}")
    string(CONCAT _summary "${_read} words read (${_heard} heard), ${_compared} compared at "
           "their own step (${_held} of them held until it was flown, at most ${_past} steps "
           "past); named: ${_unplaced} before they could be placed, ${_not_finite} not finite, "
           "${_dropped} dropped (${_why}), ${_still} still held; ${_not_kept} at a step no "
           "longer kept; ${_put_right} put right from (${_newest} the newest read, "
           "${_newer_held} the newest flown with newer held), at most ${_most} read in a pass")
    if(NOT _read EQUAL _heard)
        message(FATAL_ERROR "the client heard ${_heard} words on its own and read ${_read}: "
                            "${_summary}\n${_text}")
    endif()
    if(NOT "${_replays}" STREQUAL "" AND NOT _put_right EQUAL _replays)
        message(FATAL_ERROR "${_replays} corrections, and ${_put_right} words put right from: "
                            "${_summary}\n${_text}")
    endif()
    math(EXPR _from_newest "${_newest} + ${_newer_held}")
    if(NOT _neither EQUAL 0 OR NOT _from_newest EQUAL _put_right)
        message(FATAL_ERROR "${_neither} corrections were not from the newest word: ${_summary}\n${_text}")
    endif()
    if(NOT "${_forced}" STREQUAL "")
        if(_forced LESS 10 OR _held LESS _forced)
            message(FATAL_ERROR "${_forced} passes flown ticks behind and ${_held} words held: "
                                "the hold was not tested every time - ${_summary}\n${_text}")
        endif()
        string(APPEND _summary "; ${_forced} passes flown ticks behind")
    elseif(_most LESS 2 OR NOT _put_right LESS _read)
        message(FATAL_ERROR "no pass read more than one word, so every word was put right "
                            "from and the rule went untested: ${_summary}\n${_text}")
    endif()
    if(_read LESS 20)
        message(FATAL_ERROR "only ${_read} words were read: ${_summary}\n${_text}")
    endif()
    if(NOT _not_kept EQUAL 0)
        message(FATAL_ERROR "${_not_kept} words were placed at a step flown and no longer "
                            "kept: ${_summary}\n${_text}")
    endif()
    if(NOT _unplaced_late EQUAL 0)
        message(FATAL_ERROR "${_unplaced_late} words could not be placed after one had been: "
                            "${_summary}\n${_text}")
    endif()
    if(_unplaced GREATER 28)
        message(FATAL_ERROR "${_unplaced} words could not be placed, more than 28: ${_summary}\n${_text}")
    endif()
    if(_dropped GREATER _most_dropped)
        message(FATAL_ERROR "${_dropped} words were dropped, more than the ${_most_dropped} "
                            "stated: ${_summary}\n${_text}")
    endif()
    if(_still GREATER _most)
        message(FATAL_ERROR "${_still} words still held, more than a pass's: ${_summary}\n${_text}")
    endif()
    math(EXPR _named "${_unplaced} + ${_not_finite} + ${_dropped} + ${_still}")
    math(EXPR _accounted "${_compared} + ${_named}")
    if(NOT _accounted EQUAL _read)
        message(FATAL_ERROR "${_read} words read, and ${_compared} compared and ${_named} "
                            "named: ${_summary}\n${_text}")
    endif()
    set(${_out} "${_summary}" PARENT_SCOPE)
endfunction()

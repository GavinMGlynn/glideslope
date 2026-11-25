# joined_words_check.cmake - no frame a predicting client drew had a joined
# word compared in its pass and not yet put right from or superseded; nine
# in ten of the words compared were joined; and every putting right was from
# the newest word.
#
#   include(joined_words_check.cmake)
#   check_joined_words("${said}" ${compared} out_summary)
#
# **Why not how many corrections.** glideslope_cli reads every word that has
# come, compares each against where it had flown, and puts its prediction
# right once a pass, from the newest (frontend/own_words.hpp; Gambetta,
# Valve). The older words of a pass are measured and superseded: what is
# drawn is the newest's. How many words a pass reads is how far apart the
# client's frames are against the server's 25 words a second - a debug
# build beside twice as many busy loops as cores drew a frame every 47 ms at
# the median, and read up to 78 words in one pass after a stall of the
# machine (docs/status, 2026-11-22). Until then the tests held the
# corrections once joined to nine in ten of the updates compared, which
# under load counted the client's frame rate and failed in every run.
#
# **What it guards**, and no more:
# - **No frame drawn with a joined word left over**: the client counts each
#   joined word compared as put right from (P), superseded by a newer word
#   it read with it (S), let go of with its prediction - mid-pass or while
#   putting right (L) - or still in the pass when a frame was drawn (D).
#   P + S + L + D = N, the joined words compared, holds by construction; the
#   check is D = 0. Seen red: putting right only every other pass.
# - **Nine in ten of the words compared were joined** - the clocks'
#   difference known - so the corrections' bound is held over the
#   prediction and not over nothing. Seen red: a word counted joined only
#   30 s after the prediction settled.
# - **Every putting right was from the newest word read**, or the newest
#   whose step was flown with newer held (frontend::OwnWords): none from
#   another. Seen red: the oldest of a pass offered instead.
function(check_joined_words said compared out)
    if(NOT said MATCHES "joined words: ([0-9]+) compared, ([0-9]+) put right from, ([0-9]+) superseded by a newer read with them, ([0-9]+) let go of, ([0-9]+) drawn without either")
        message(FATAL_ERROR "the client did not say what became of its joined words:\n${said}")
    endif()
    set(_words "${CMAKE_MATCH_1}")
    set(_put "${CMAKE_MATCH_2}")
    set(_superseded "${CMAKE_MATCH_3}")
    set(_let_go "${CMAKE_MATCH_4}")
    set(_left "${CMAKE_MATCH_5}")
    math(EXPR _least "${compared} * 9 / 10")
    if(_words LESS _least)
        message(FATAL_ERROR "only ${_words} words compared once joined, under ${_least} - nine in "
                            "ten of the ${compared} updates compared:\n${said}")
    endif()
    if(NOT _left EQUAL 0)
        message(FATAL_ERROR "of ${_words} joined words compared, ${_left} were in a frame drawn "
                            "before they were put right from or superseded:\n${said}")
    endif()
    if(NOT said MATCHES "([0-9]+) put right from \\(([0-9]+) the newest read, ([0-9]+) the newest flown with newer held, ([0-9]+) neither\\)")
        message(FATAL_ERROR "the client did not say which words it put right from:\n${said}")
    endif()
    math(EXPR _from_newest "${CMAKE_MATCH_2} + ${CMAKE_MATCH_3}")
    if(NOT CMAKE_MATCH_4 EQUAL 0 OR NOT _from_newest EQUAL CMAKE_MATCH_1)
        message(FATAL_ERROR "${CMAKE_MATCH_4} of ${CMAKE_MATCH_1} puttings right were not from "
                            "the newest word:\n${said}")
    endif()
    set(${out} "${_words} joined words compared: ${_put} put right from, ${_superseded} superseded, ${_let_go} let go of; every putting right from the newest" PARENT_SCOPE)
endfunction()

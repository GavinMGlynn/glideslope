# joined_words_check.cmake - every joined word a predicting client compared
# was put right from, or superseded by a newer word read with it that was.
#
#   include(joined_words_check.cmake)
#   check_joined_words("${said}" ${compared} out_summary)
#
# **What is held.** glideslope_cli reads every word that has come, compares
# each against where it had flown, and puts its prediction right once a
# pass, from the newest (frontend/own_words.hpp; Gambetta, Valve). The older
# words of a pass are measured and superseded: put right from one and then
# the newest before a frame is drawn, what is drawn is the newest's, so a
# superseded word costs the player nothing. How many words a pass reads is
# how far apart the client's frames are against the server's 25 words a
# second: a debug build beside twice as many busy loops as cores drew a
# frame every 47 ms at the median and read up to seven words a frame, and
# 59 at once as it joined, a 395 ms stall of the machine on top of the
# server catching up its start (docs/status, 2026-11-22). So
# what a test holds is not how many words were put right from, which is the
# client's frame rate, but that:
#
# - **nearly every word compared was joined** - nine in ten, the clocks'
#   difference known - so the corrections' bound is held over the
#   prediction and not over nothing; and
# - **every one of them was put right from, superseded by a newer word read
#   with it, or in a pass whose prediction was let go of**, and none was
#   still waiting when a frame was drawn.
#
# Until 2026-11-22 it held the corrections once joined to nine in ten of
# the updates compared, which under load counted the client's frame rate
# and failed in every run (1,684 to 2,122 of 3,000).
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
    math(EXPR _accounted "${_put} + ${_superseded} + ${_let_go}")
    if(NOT _accounted EQUAL _words OR NOT _left EQUAL 0)
        message(FATAL_ERROR "of ${_words} joined words compared, ${_put} were put right from, "
                            "${_superseded} superseded and ${_let_go} let go of - ${_left} were "
                            "in a frame drawn before they were put right from or superseded:\n${said}")
    endif()
    set(${out} "${_words} joined words compared: ${_put} put right from, ${_superseded} superseded, ${_let_go} let go of" PARENT_SCOPE)
endfunction()

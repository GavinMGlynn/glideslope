# machine_stalls_check.cmake - what a test that predicts holds a machine's
# stalls to, included by each: server_paced_prediction, server_take_over,
# server_stovl_landing, server_stovl_vertical_landing and client_on_server.
#
# A client counts apart the updates and corrections across a frame its machine
# mostly took (frontend::MachineStalls). That is an excuse, and it is capped,
# so that a machine that stalls all through fails rather than excusing the
# whole test:
#
# - **At most two stalls.** CI's Windows runs show one in a test when they
#   show any (run 39743359407: one in each of three shards, the one built);
#   two is that with one to spare.
# - **At most a quarter of what was compared counted apart.** One 3.5 s stall
#   - the longest CI has shown, and the one built - takes 113 of the paced
#   test's 600 updates (19%): its catching up (53 words) and the 60 after
#   (sim::offset_window 50 + sim::pace_window 10). Past a quarter, the test
#   measured the machine, not the prediction.
# - **Every stall back within a metre** by the last of the words after it
#   that were heard: its error over a metre on no later word. A stall whose
#   60 words were all heard is back within them.
#
#   check_machine_stalls(TEXT WHAT TOTAL OUT)
#
# TEXT is the client's output; WHAT is `updates` (glideslope_cli, which
# compares them) or `corrections` (the client with the window, which counts
# those); TOTAL is how many of WHAT there were in all; OUT is set to a
# summary naming every stall.

function(check_machine_stalls _text _what _total _out)
    if(NOT _text MATCHES "the machine's stalls: ([0-9]+) frames[^\n]*, ([0-9]+) updates compared and ([0-9]+) corrections")
        message(FATAL_ERROR "the client did not say its machine's stalls:\n${_text}")
    endif()
    set(_stalls "${CMAKE_MATCH_1}")
    if(_what STREQUAL "updates")
        set(_apart "${CMAKE_MATCH_2}")
    else()
        set(_apart "${CMAKE_MATCH_3}")
    endif()
    set(_summary "${_stalls} machine's stalls, ${_apart} of ${_total} ${_what} counted apart")
    math(EXPR _most_apart "${_total} / 4")
    if(_stalls GREATER 2 OR _apart GREATER _most_apart)
        message(FATAL_ERROR "the machine stalled too often for the prediction to be judged: "
                            "${_summary} - at most 2 stalls and a quarter (${_most_apart}) "
                            "allowed:\n${_text}")
    endif()
    # A stall's line has semicolons, CMake's list separator: commas instead.
    string(REPLACE ";" "," _plain "${_text}")
    string(REGEX MATCHALL "the machine's stall [0-9]+: [^\n]*" _each "${_plain}")
    foreach(_one IN LISTS _each)
        string(APPEND _summary "; ${_one}")
        if(NOT _one MATCHES "and ([0-9]+) after, .* back within 1 m from word ([0-9]+) after")
            message(FATAL_ERROR "a stall said without its recovery: ${_one}\n${_text}")
        endif()
        if(CMAKE_MATCH_1 GREATER 0 AND CMAKE_MATCH_2 GREATER CMAKE_MATCH_1)
            message(FATAL_ERROR "the prediction was not back within a metre after a stall: "
                                "${_one}\n${_text}")
        endif()
    endforeach()
    set(${_out} "${_summary}" PARENT_SCOPE)
endfunction()

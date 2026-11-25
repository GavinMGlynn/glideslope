# server_stovl_landing.cmake - a player's STOVL selector flown through a
# real server: the server's F-35B and the client's prediction fly the same
# transition law, and agree.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -P server_stovl_landing.cmake
#
# **Built, not hoped for.** A server with one AI and a plan that starts its
# player at 2,200 ft over Sydney airport heading west, over land; the
# command-line client joins it asking for the F-35B (`--aircraft f35b`) and
# predicts its own aircraft (`--predict`) through a relay that holds every
# datagram 100 ms each way. Its pilot (`--stovl-land`) flies the power the
# server holds for a player who has not touched anything for five seconds,
# and then selects STOVL mode and holds the stick full forward, the lever
# still: selected once the server is flying her inputs, as a pilot would,
# the server's transition law and the prediction's are seeded from the same
# controls (selected with the first input they were not, and their
# integrals stayed apart - PROJECT_STATUS.md, 2026-10-14). And selected
# once the client's prediction has settled, the clocks' difference known,
# which it says ("selected with input N, its prediction settled"). The law's
# height asked is seeded on each side from her height as each sees her; since
# protocol 13 the server tells it with her motion and the prediction takes it,
# so selected early it no longer holds what it was off by then (108 mm at the
# median under load before). And its wing-borne autopilot's pitch integral,
# seeded a step apart when the selector reaches the server a step off: 14 to
# 37 mm at the median here, 74 mm on CI (run 38692182329), before it was
# told; 0 to 2 mm after, selected settled or at the 150th input, idle or
# beside twice as many busy loops as cores (PROJECT_STATUS.md, 2026-10-16).
# Settled selection is kept: a pilot selects once flying.
#
# **What a server gives a player who asks for none is her model's own
# loading, 45,400 lb**,
# over the 39,837 lb she is flown into a hover at (`fcs/stovl-hover-most-lbs`):
# so on the server and in the prediction alike her STOVL mode holds her
# wing-borne at her conversion speed and the stick lets her down to her
# least conversion height (100 ft), and no further. Before that limit she
# was slowed to a hover she could not hold and was a wreck here, "hit the
# ground sinking at 4368 ft/min" - and flown raw, the stick full forward
# would have dived her in sooner. A player who asks for her `vertical_landing`
# loading is light enough to hover, and is converted, hovered and landed
# through a server in server_stovl_vertical_landing.cmake. The client stays until
# 3,000 updates are compared (`--until-compared`): the event, not a time.
#
# **What must hold**: no wreck heard; the client's prediction never
# converted, was let down no lower than 90 ft above the ground and ends at
# her 200 KCAS conversion speed within 10 kt; once joined - the clocks'
# difference known - no correction too large to hide (joining, at her 300
# kt, the prediction may be put right by more: 20 m on a debug build); and
# the prediction's error - the server's word against where the client had
# flown her to - under 60 mm at the median, within four steps' travel at her
# conversion speed for 95 updates in a hundred (3.4 m: a step is 0.86 m at
# 200 KCAS), and within four steps' travel at the 330 kt she dives to at
# worst (5.6 m: a step is 1.4 m there), over at least 3,000 updates; and her
# predicted pitch within half a degree of the server's at every update
# compared (0.154 at worst in 14 runs here, 0.190 on CI). Nine in ten of
# the words compared must be joined, and each put right from or superseded
# by a newer word read with it, so that the 20 m is held over the
# prediction and not over nothing (joined_words_check.cmake: how many are
# put right from is how far apart the client's frames are).
#
# **A named limit, the one server_paced_prediction.cmake names: the clocks'
# difference is known to a step, and on a busy machine some words are
# placed a step off, or two.** The difference is the least of the last fifty
# words'; a server held up unevenly by a busy machine, or by Windows' 15.6 ms
# sleeps, moves it, and the server's word is placed a step or two from where
# it was - that step's travel on that update, and the correction a player
# would see is that far, hidden by the blend. The median carries the
# prediction itself. Measured: on an idle machine the 95th percentile is 1 to
# 44 mm; with twice as many busy loops as cores beside it (2026-10-15) 0.868 m,
# the worst 1.505 m, the median 1 mm - CI's macOS 0.890 and 0.901 m, Windows'
# 1.22 and 1.28 m with the worst 2.99 and 3.56 m (runs 38517430777,
# 38531751565). The bound was 10 cm for 95 in a hundred, set from idle runs,
# and no busy runner met it: one update in twenty a step off is 0.86 m.
# Since 2026-11-22 the difference is held once settled, lowered only when
# ten of the fifty are below it (sim::ClockDifference): beside twice as many
# busy loops as cores the 95th percentile went to 1 to 3 mm in ten runs, from
# 0.87 m in six of ten. The bounds below are left at every platform's CI
# figures until Windows' two-step spread, its own item, is done.
# Lowest 778 to 1,013 ft above the ground, 200 KCAS at the end.
#
# **The bounds are CI's figures, every platform's, with headroom
# (2026-10-16).** The median was held to 1 cm and the 95th percentile to
# two steps (1.7 m) on 2026-10-16, from 14 runs here; CI then failed both.
# Every figure in a CI log since the law's state was told (a passing run
# prints none): the median 1, 1, 2 and 2 mm on Windows' release and clang,
# 11 mm on Ubuntu's linux-debug (run 38803123702); the 95th percentile
# 1.740, 1.745, 1.745 and 1.746 m on Windows (runs 38795838392,
# 38803123702 twice) - two steps off for one update in twenty, there every
# time - and 0.891 m on Ubuntu's debug. Before it, 0.889 to 1.374 m on
# macOS and Windows and 2.531 m on Ubuntu's debug (run 38517430777), the
# most any run has been. So the 95th percentile is held to four steps'
# travel, 3.4 m: a third over that 2.531, twice Windows' 1.75. And the
# median to 60 mm: about twice the most CI has shown, 28 mm on
# windows-clang (run 38842431267, its pitch 0.073 degrees), after 15 mm
# was set from an 11. **The median catches no planted bug now**: a
# prediction whose pitch integral is a degree off the server's flies to a
# median of 22 to 31 mm, inside what CI does unbroken. **The pitch check
# catches it**: her pitch 1.702 degrees off against CI's worst 0.190 and
# the half-degree bound - seen red, reverted, green (2026-10-16).
# **What no bound here catches** is a prediction
# flown on 0.8 of the stick the server flies: in STOVL mode the stick asks
# the height, the height is told, and it flew to a median of 2 mm and
# passed (2026-10-16, as on 2026-10-15). What the stick sets is held
# against the server's at every step in test_prediction.cpp ("a STOVL
# prediction sets with her stick what the server sets"), which that bug
# turns red; with it in, this passed again (the median 0 mm, 2026-10-22).
#
# RUN_SERIAL: a runner busy beside it would hold the server up further,
# unsteadily, as server_paced_prediction.cmake says of itself.
#
# **How far she has dived before her selector arrives is how long the join
# took**: untouched, the server's F-35B dives, and the law pulls her out
# from any moment of her first ten seconds (test_figures.cpp, "selected
# into STOVL mode at any moment"); on CI before that she went down to 25 ft
# and into the ground.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake): another
# build's run of this test on the same port waits for it.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/stovl.sqlite")
set(_heard "${WORK}/heard.txt")
set(_plan "${WORK}/stovl.plan")
file(REMOVE "${_store}" "${_heard}")
# The AI - a server with none gives nobody an aircraft - flies the plan from
# its start, north; the player starts two layers (2,000 ft) above it, west.
file(WRITE "${_plan}" "aircraft c172p\n"
                      "start -33.9461 151.1772 200 270 100\n"
                      "waypoint NORTH -33.5000 151.1772 1500 100\n")
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

# **Slipped** (`-DSLIPPED=ON`): the server runs nothing for 3.7 s at 60 s on
# its clock (`--test-stopped-once 60 3700`), past the 3.5 s it catches up
# (net::most_steps_caught_up), and lets the rest slip, for good - a real
# slip, built, and said (`--test-say-slipped`). Run by hand for its plan
# item (COMPLETION_PLAN, Phase 12a): not in ctest until it passes there -
# even idle its first correction after the slip, 19 to 20 m, can come before
# the slip is seen (docs/status, 2026-11-25).
set(_slipped_args "")
if(SLIPPED)
    set(_slipped_args --test-stopped-once 60 3700 --test-say-slipped)
endif()

math(EXPR _relay "${PORT} + 1")
port_lock_hold(${_relay})
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 900 --after 1
            --aircraft f35b --predict --stovl-land --until-compared 3000
            --heard "${_heard}"
    COMMAND "${SERVER}" --port ${PORT} --seconds 1000 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 30 --store "${_store}" --plan "${_plan}"
            ${_slipped_args}
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 1000
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
port_listened("${_out}" "${_err}")
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
if(_out MATCHES "is a wreck" OR _err MATCHES "is a wreck")
    message(FATAL_ERROR "an aircraft was a wreck:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _said MATCHES "stovl: converted at ([-0-9.]+) s, hovering at ([-0-9.]+) s, down at ([-0-9.]+) s; the server's word: hovering (yes|no), at rest (yes|no); lowest ([-0-9]+) ft above the ground, ([0-9]+) KCAS at the end")
    message(FATAL_ERROR "the client never said how its STOVL flight went:\n${_said}\n${_err}")
endif()
set(_converted "${CMAKE_MATCH_1}")
set(_lowest "${CMAKE_MATCH_6}")
set(_kcas "${CMAKE_MATCH_7}")
if(NOT _converted STREQUAL "-1.0")
    message(FATAL_ERROR "heavier than she can hover, she was converted at ${_converted} s:\n${_said}")
endif()
if(_lowest LESS 90)
    message(FATAL_ERROR "she was let down to ${_lowest} ft above the ground:\n${_said}")
endif()
if(_kcas LESS 190 OR _kcas GREATER 210)
    message(FATAL_ERROR "she ended at ${_kcas} KCAS, not her conversion speed:\n${_said}")
endif()
if(NOT _said MATCHES "stovl: selected with input ([0-9]+), its prediction settled")
    message(FATAL_ERROR "the client never selected STOVL mode, its prediction settled:\n${_said}")
endif()
# **Her predicted pitch within half a degree of the server's at every
# update compared** (COMPLETION_PLAN, Phase 9; 0.190 the most on CI): a law
# that has wandered in the prediction shows in her pitch before her position.
if(NOT _said MATCHES "stovl: predicted pitch within ([0-9]+)\\.([0-9][0-9][0-9]) degrees")
    message(FATAL_ERROR "the client did not say its pitch error:\n${_said}")
endif()
if(CMAKE_MATCH_1 GREATER 0 OR CMAKE_MATCH_2 GREATER 500)
    message(FATAL_ERROR "her predicted pitch was ${CMAKE_MATCH_1}.${CMAKE_MATCH_2} degrees off "
                        "the server's, over half a degree:\n${_said}")
endif()
set(_pitch "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}")
endif()
set(_compared "${CMAKE_MATCH_1}")
math(EXPR _worst_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
if(_compared LESS 3000)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
# **After a server's slip, and apart from it** (frontend::AfterASlip). A
# slip is seen in a word placed two steps or more from where it falls on the
# path flown, and it and the 60 words after it - the clocks' difference's
# window and the pace's - are counted apart, with the corrections once
# joined made while they come. That is the held difference's accepted cost
# (2026-11-22): a slip followed ten words late, and each move of the figure
# after it putting the round trip's words off again. **The rest are held as
# every word was** until a loaded machine's real slips went past it (6.7
# and 17.4 m, two runs in ten, 2026-11-22): four steps' travel at 330 kt
# (5.6 m), and no correction too large to hide. **The words after a slip are
# held to measured figures** (docs/status, 2026-11-25): beside twice as many
# busy loops as cores, a slip built of 24 to 26 steps, and the machine's own
# slips besides.
set(_worst_after_a_slip_mm 50000)
set(_worst_correction_after_a_slip_mm 60000)
set(_most_after_a_slip 250)
if(NOT _said MATCHES "after a slip: ([0-9]+) seen, ([0-9]+) updates compared after them, at most ([0-9]+) after one, placed at most ([0-9]+) steps off, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m; the rest: ([0-9]+) updates, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m;")
    message(FATAL_ERROR "the client did not say its words after a slip:\n${_said}")
endif()
set(_slips_seen "${CMAKE_MATCH_1}")
set(_after_slip "${CMAKE_MATCH_2}")
set(_longest_after "${CMAKE_MATCH_3}")
set(_most_off "${CMAKE_MATCH_4}")
math(EXPR _after_worst_mm "${CMAKE_MATCH_5} * 1000 + ${CMAKE_MATCH_6}")
set(_rest "${CMAKE_MATCH_7}")
math(EXPR _rest_worst_mm "${CMAKE_MATCH_8} * 1000 + ${CMAKE_MATCH_9}")
# (A CMake regular expression holds nine groups at most.)
if(NOT _said MATCHES "corrections once joined after them: ([0-9]+), the worst ([0-9]+)\\.([0-9][0-9][0-9]) m, ([0-9]+) too large to hide")
    message(FATAL_ERROR "the client did not say its corrections after a slip:\n${_said}")
endif()
set(_slip_corrections "${CMAKE_MATCH_1}")
math(EXPR _slip_correction_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
set(_slip_snapped "${CMAKE_MATCH_4}")
if(_rest_worst_mm GREATER 5600)
    message(FATAL_ERROR "the worst prediction error apart from a slip was ${_rest_worst_mm} mm, "
                        "over four steps' travel at 330 kt (5.6 m):\n${_said}")
endif()
if(_after_worst_mm GREATER _worst_after_a_slip_mm)
    message(FATAL_ERROR "the worst prediction error after a slip was ${_after_worst_mm} mm, "
                        "over ${_worst_after_a_slip_mm} mm:\n${_said}")
endif()
if(_slip_correction_mm GREATER _worst_correction_after_a_slip_mm)
    message(FATAL_ERROR "a correction once joined after a slip was ${_slip_correction_mm} mm, "
                        "over ${_worst_correction_after_a_slip_mm} mm:\n${_said}")
endif()
if(_longest_after GREATER _most_after_a_slip)
    message(FATAL_ERROR "${_longest_after} updates after one slip, over ${_most_after_a_slip}: "
                        "the clocks' difference was that long following it:\n${_said}")
endif()
if(NOT _said MATCHES "predicted, joined: ([0-9]+) corrections, the worst ([0-9.]+) m, ([0-9]+) too large to hide")
    message(FATAL_ERROR "the client did not say its corrections once joined:\n${_said}")
endif()
set(_joined "${CMAKE_MATCH_1}")
set(_joined_worst "${CMAKE_MATCH_2}")
math(EXPR _snapped_rest "${CMAKE_MATCH_3} - ${_slip_snapped}")
if(NOT _snapped_rest EQUAL 0)
    message(FATAL_ERROR "${_snapped_rest} corrections once joined, not after a slip, were too "
                        "large to hide:\n${_said}")
endif()
string(CONCAT _slip_said "${_slips_seen} slips seen, ${_after_slip} updates after them (at most "
               "${_longest_after} after one, placed at most ${_most_off} steps off), the "
               "worst ${_after_worst_mm} mm, ${_slip_corrections} corrections the worst "
               "${_slip_correction_mm} mm, ${_slip_snapped} too large to hide; the rest's "
               "worst ${_rest_worst_mm} mm over ${_rest}")
# **Slipped, the slip is built and seen**: the server said it let at least
# 20 steps go, and the client saw it.
if(SLIPPED)
    string(REGEX MATCHALL "let [0-9]+ steps? slip, [0-9]+ in all" _lets "${_err}")
    list(LENGTH _lets _n_lets)
    set(_let 0)
    if(_n_lets GREATER 0)
        list(GET _lets -1 _last_let)
        string(REGEX REPLACE ".*, ([0-9]+) in all" "\\1" _let "${_last_let}")
    endif()
    if(_let LESS 20)
        message(FATAL_ERROR "the server let ${_let} steps slip, under the 20 built:\n${_err}")
    endif()
    if(_slips_seen LESS 1)
        message(FATAL_ERROR "the server let ${_let} steps slip and the client saw no slip:\n${_said}")
    endif()
    string(APPEND _slip_said "; the server let ${_let} steps slip")
endif()
# **And the joined window is not empty**: nine in ten of the words compared
# were joined, and every one of them was put right from or superseded by a
# newer word read with it (joined_words_check.cmake).
include("${CMAKE_CURRENT_LIST_DIR}/joined_words_check.cmake")
check_joined_words("${_said}" "${_compared}" _joined_words)
if(NOT _said MATCHES "prediction error 95th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its 95th percentile:\n${_said}")
endif()
math(EXPR _p95_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_p95_mm GREATER 3400)
    message(FATAL_ERROR "the 95th percentile prediction error was ${_p95_mm} mm, over four steps' "
                        "travel at 200 KCAS (3.4 m):\n${_said}")
endif()
if(NOT _said MATCHES "prediction error median: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its median error:\n${_said}")
endif()
math(EXPR _median_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(_median_mm GREATER 60)
    message(FATAL_ERROR "the median prediction error was ${_median_mm} mm, over 60 mm:\n${_said}")
endif()
message(STATUS "held wing-borne: lowest ${_lowest} ft, ${_kcas} KCAS at the end; ${_compared} "
               "updates compared, the worst ${_worst_mm} mm, the 95th percentile ${_p95_mm} mm, "
               "the median ${_median_mm} mm; ${_joined} corrections once joined, the "
               "worst ${_joined_worst} m; ${_joined_words}; her pitch within ${_pitch} "
               "degrees of the server's")
message(STATUS "after a slip: ${_slip_said}")
# **The machine's stalls**, capped and each recovered from
# (machine_stalls_check.cmake): CI's Windows clang once went 5,214 ms between
# two of this client's frames and its worst went to 6.2 m (run 39701548318).
include("${CMAKE_CURRENT_LIST_DIR}/machine_stalls_check.cmake")
check_machine_stalls("${_said}" updates "${_compared}" _stalls)
message(STATUS "${_stalls}")

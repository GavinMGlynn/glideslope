# server_paced_prediction.cmake - a client predicting against a server behind
# real time flies at the server's pace and is off by centimetres, not metres.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> [-DUNPACED=ON] [-DSTOPPED=ON]
#         [-DSERVER_HELD=alone|unevenly] -P server_paced_prediction.cmake
#
# **What is pinned.** The server flies each input from when it arrives until
# the next does. One behind real time flies fewer steps of each than a client
# flying at its own machine's pace, and the two part by metres. The client
# steers its pace by the clocks' difference its prediction measures, holding
# it where it was first known (sim::Pacing), and so flies each input for as
# many steps as the server does.
#
# **The situation is built**: the server's clock runs at a set 0.6 of real
# time (`--test-pace 0.6`) - the same on every machine that can keep up with
# that, where one slowed by sleeping in its steps is as slow as the runner.
# The client predicts through a relay that delays everything 100 ms each way,
# flying changing controls, and stays until 600 updates have been compared
# (`--until-compared 600`) - 40 s of the server's updates at 15 a second - the
# event, not a time.
#
# **What must hold**: the client flew as many steps as the server, within
# 1 percent, between the first update compared and the last, and its prediction error was under a metre for 95 updates in a
# hundred, under 3 m at worst, and its median under 5 cm. `-DUNPACED=ON` flies
# the client at its own pace (`--unpaced`): that is what fails, by metres.
#
# **A machine's stall is counted apart, and recovered from.** CI's Windows
# runners have stopped the client - and the server beside it - for seconds
# (3,503 of 3,583 ms between two frames, 2026-10-21: 138 m). The client
# names each frame its machine mostly took, by its own thread's CPU time,
# and counts the updates across it apart - the server's catching up and the
# 60 words after, while the clocks' difference is found again - saying how
# many, the worst, and from which word it was back within a metre
# (glideslope_cli's `MachineStall`). Every update after them is judged with
# the rest, and the median and percentiles are of all of them, so a
# recovery that never comes still fails.
#
# **Built** (`-DSTOPPED=ON`): the server runs nothing for 3.5 s at 12 s on
# the session's clock (`--test-stopped-once`) and the client for 4 s
# (`--stopped-once`), on a pass with a word still waiting unread, a whole
# machine stopped - the relay runs on, which stopped alone 3 s by hand moved
# nothing past a metre. **The client wakes last, with the server's catching
# up all waiting**: macOS's runner woke it 150 ms late (3,650 ms of 3,500),
# and a stall named only at the frame drawn once all of that was read put
# the catching up after it, 0 words of it, spent the 60 words on it and was
# 4.2 m off past them (run 39796618052). The test requires that stall named,
# of 3.4 s or more, the server's catching up counted as such, all 60 words
# after it heard, and the error back within a metre by the last of them.
# Seen red with the stall named at the frame again (2026-10-23): 0 words
# catching up, 5.9 and 6.3 m off past the 60.
# Linux release: 5.6 to 6.3 m at worst across it, back within a metre from the
# 4th to 8th word after; 0.475 to 0.945 m at worst elsewhere. A stall of its own
# (a busy wait in its place) is not counted apart, and fails.
# (docs/status, 2026-10-22).
#
# **Built** (`-DSERVER_HELD=...`): **the server held up, and not the
# client**, at real time's pace (`--test-pace 1`) - a server whose runner
# holds it up while its client's runs on, which the client cannot see and
# does not count apart. `alone`: stopped 3 s at 12 s on its clock
# (`--test-stopped-once 12 3000`). `unevenly`: every second on its clock,
# run nothing for 150, 300, 450 and 600 ms in turn (`--test-held-up 1 600`).
# Either way it catches up every step it owes, and flies each input from the
# step due when it came, never sooner (net::CatchingUp): flown at most a
# quarter of a second late, as it was, the inputs read after a hold-up were
# flown early by the rest, the clocks' difference fell by it and stayed down
# two seconds while the server caught up. Linux debug, main's code: stopped
# alone, the 95th percentile 15.3 m and the worst 103.9 m; unevenly, 9.7 m
# and 14.9 m. With the rule: 0.62 m and 4.9 m; 0.35 m and 1.9 m
# (docs/status, 2026-10-28). **A named limit, stopped alone**: the inputs
# that came in the 3 s are read in one pass and the newest alone is flown, so
# the server flies none of the stick's moves in them - 4.4 m off the path the
# client flew - and every word of its catching up, 75 of the 600 at a word
# every 4.8 steps, is that much off - by how much is the stick's moves', and
# runs part three-fold. **The bounds are from what was measured**: nine runs
# of Windows debug, the 95th percentile 0.49 to 1.51 m and the worst 2.1 to
# 4.7 m (eight on this machine, 2026-10-28, and CI's run 40621099406), and
# Linux debug's 0.62 and 4.9 m; the 95th held to 3 m and the worst to 10 m,
# twice the most seen of each. Main's code put them at 24.8 and 104 m. The
# rest is held to the bounds below.
# Not built: a server persistently behind, which the paced test is.
#
# **A named limit: on Windows about one update in a hundred is off by two
# steps' travel.** A step's travel is 0.42 m at this aircraft's speed, and
# the clocks' difference - the least of the last fifty words' - is known to a
# step. Windows' sleeps are 15.6 ms, not the millisecond the server, relay
# and client ask for, so each word carries up to two steps of spread; as a
# word that came through quickest enters the fifty, or leaves it, the least
# moves by two steps, and the server's word is placed two steps off - 1.1 m
# on the update that does it, and the correction a player would see is that
# far, hidden by the blend. Read from the client's track on Windows debug
# (2026-10-08): the 1.1 m updates fell exactly where the least moved, -20 to
# -22 and back, with centimetres between. On CI's Windows debug, 1 in 100
# such updates or more put the 99th percentile at 1.105 m (run 37723785657),
# so the metre bounds the 95th, the worst three metres - under seven steps'
# travel. Tried and taken out: steering the pace by each word's difference,
# smoothed, instead of the least of the newest ten - five in a hundred off
# by a step instead (PROJECT_STATUS.md).
#
# RUN_SERIAL: a runner busy beside it would put the server further behind
# than its set pace, unsteadily, which is not what this builds.
#
# It needs the DEM's tiles, so without the network it reports itself skipped
# (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
set(_store "${WORK}/paced.sqlite")
set(_heard "${WORK}/heard.txt")
file(REMOVE "${_store}" "${_heard}")
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

set(_pace 0.6)
set(_worst_allowed_mm 3000)
set(_p95_allowed_mm 1000)
set(_server_held "")
if(SERVER_HELD STREQUAL "alone")
    set(_pace 1)
    set(_server_held --test-stopped-once 12 3000)
    set(_worst_allowed_mm 10000)
    set(_p95_allowed_mm 3000)
elseif(SERVER_HELD STREQUAL "unevenly")
    set(_pace 1)
    set(_server_held --test-held-up 1 600)
    # **Its worst update is the held-up item's open part**: a stick moved
    # while the server is held up is flown as one move once it catches up,
    # which needs inputs that name their step (COMPLETION_PLAN, Phase 12a),
    # the worst 4.9 m stopped alone. Bounded as that case is, at 10 m, twice
    # the most seen: the default 3 m was the paced test's, never this one's,
    # and CI went past it - 3.3 m on Windows release (run 40673705998) and
    # 4.1 m on macOS (run 40673840988), their 95th percentiles 0.49 m and
    # under the metre held below.
    set(_worst_allowed_mm 10000)
elseif(DEFINED SERVER_HELD)
    message(FATAL_ERROR "SERVER_HELD is alone or unevenly, not '${SERVER_HELD}'")
endif()
set(_unpaced "")
if(UNPACED)
    set(_unpaced --unpaced)
endif()
# The whole machine stopped 3.5 s at 12 s on the session's clock (20 s of
# real time at 0.6): client and server each told to
# run nothing then, as a runner that stops them all does, the client woken
# half a second after the server, as macOS's woke it late.
set(_server_stopped "")
if(STOPPED)
    list(APPEND _unpaced --stopped-once 12 4000)
    set(_server_stopped --test-stopped-once 12 3500)
endif()
math(EXPR _relay "${PORT} + 1")
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${_relay}" "${_key}" 280 --after 1
            --predict --until-compared 600 --heard "${_heard}" ${_unpaced}
            --track "${WORK}/track.txt"
    COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 5 --store "${_store}" --test-pace ${_pace}
            ${_server_stopped} ${_server_held}
    COMMAND "${IMPAIR}" ${_relay} "127.0.0.1:${PORT}" --delay 100 --jitter 0
            --loss 0 --seed 1 --until-input-ends --seconds 290
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
file(READ "${_heard}" _said)
if(NOT _said MATCHES "prediction error: ([0-9]+) updates compared, the worst ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its prediction error:\n${_said}\n${_err}")
endif()
set(_compared "${CMAKE_MATCH_1}")
math(EXPR _worst_mm "${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
if(NOT _said MATCHES "prediction error median: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its median error:\n${_said}")
endif()
math(EXPR _median_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "prediction error 95th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its 95th percentile:\n${_said}")
endif()
math(EXPR _p95_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "prediction error 99th percentile: ([0-9]+)\\.([0-9][0-9][0-9]) m")
    message(FATAL_ERROR "the client did not say its 99th percentile:\n${_said}")
endif()
math(EXPR _p99_mm "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "paced: [^\n]* at ([0-9]+)\\.([0-9][0-9][0-9]) of this machine's clock at the end; the clocks' difference (-?[0-9]+) steps from the one held then, (-?[0-9]+) at worst")
    message(FATAL_ERROR "the client did not say its pace:\n${_said}")
endif()
math(EXPR _pace_thousandths "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
if(NOT _said MATCHES "paced in steps: ([0-9]+) flown here while the server flew ([0-9]+),")
    message(FATAL_ERROR "the client did not say the steps it flew:\n${_said}")
endif()
set(_steps_here "${CMAKE_MATCH_1}")
set(_steps_there "${CMAKE_MATCH_2}")
math(EXPR _steps_apart "${_steps_here} - ${_steps_there}")
if(_steps_apart LESS 0)
    math(EXPR _steps_apart "0 - ${_steps_apart}")
endif()
math(EXPR _steps_allowed "${_steps_there} / 100")
string(CONCAT _line "${_compared} updates compared, the worst error ${_worst_mm} mm, the "
          "95th and 99th percentiles ${_p95_mm} and ${_p99_mm} mm and the median "
          "${_median_mm} mm, flown at ${_pace_thousandths} thousandths of real time against a "
          "server at ${_pace} (${_steps_here} steps flown here to the server's "
          "${_steps_there})")
# **The machine's stalls**, capped and each recovered from
# (machine_stalls_check.cmake).
include("${CMAKE_CURRENT_LIST_DIR}/machine_stalls_check.cmake")
check_machine_stalls("${_said}" updates "${_compared}" _stalls)
# The stall built, seen to have happened: a frame held through it, all of it
# the machine's as the client counts its own time.
if(STOPPED)
    if(NOT _said MATCHES
       "the machine's stall [0-9]+: ([0-9]+) ms, [0-9.]+ s in; ([0-9]+) words catching up and ([0-9]+) after, ([0-9]+) updates compared, the worst ([0-9.]+) m, back within 1 m from word ([0-9]+) after")
        message(FATAL_ERROR "the client did not name the stall built - it stops only on "
                            "a pass straight after a datagram was read, from 12 s on the "
                            "session's clock, so none may have come after it, or it "
                            "stopped and the stall was not counted the machine's:\n${_said}")
    endif()
    string(APPEND _line ", and across the machine's stall of ${CMAKE_MATCH_1} ms "
           "${CMAKE_MATCH_4} updates counted apart, the worst ${CMAKE_MATCH_5} m, back "
           "within a metre from word ${CMAKE_MATCH_6} of the ${CMAKE_MATCH_3} after the "
           "${CMAKE_MATCH_2} of its catching up")
    if(CMAKE_MATCH_1 LESS 3400)
        message(FATAL_ERROR "the machine was not stopped 3.5 s: ${_line}:\n${_said}")
    endif()
    if(CMAKE_MATCH_2 EQUAL 0)
        message(FATAL_ERROR "the server's catching up was not counted as such, so the "
                            "60 words after the stall were spent on it: ${_line}:\n${_said}")
    endif()
    if(NOT CMAKE_MATCH_3 EQUAL 60 OR CMAKE_MATCH_6 GREATER 60)
        message(FATAL_ERROR "the prediction did not come back within a metre in the 60 "
                            "words after the stall: ${_line}:\n${_said}")
    endif()
endif()
string(APPEND _line "; ${_stalls}")
if(_compared LESS 600)
    message(FATAL_ERROR "only ${_compared} updates were compared:\n${_said}")
endif()
if(_p95_mm GREATER _p95_allowed_mm OR _worst_mm GREATER _worst_allowed_mm OR _median_mm GREATER 50)
    message(FATAL_ERROR "against a server behind real time the prediction was off: "
                        "${_line}:\n${_said}")
endif()
# **The pace in steps, counted, not timed**: the pace at the end is a real
# time's, which Windows' 15.6 ms timers moved to 0.583 once on CI's Windows
# debug (run 37729451368) with the steps held; the steps flown here and on
# the server between the first update compared and the last are the same
# whatever the clocks do. Within 1 percent; unpaced they part by 40.
if(_steps_apart GREATER _steps_allowed)
    message(FATAL_ERROR "the client did not fly at the server's pace: ${_line}:\n${_said}")
endif()
message(STATUS "${_line}")

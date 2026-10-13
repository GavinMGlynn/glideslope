# measured_landing.cmake - a measured landing distance is what the
# measurement makes.
#
#   cmake -DPROGRAM=<glideslope_cli> -DAIRCRAFT=<id> -DFIGURES=<figures xml>
#         -P measured_landing.cmake
#
# An aircraft that publishes no landing distance carries the runway her
# model used, landed by the approach autopilot (`glideslope_cli land`), as
# `<landing_distance ft="..." basis="measured">`. This flies that landing
# again and fails unless she stopped on the runway, within the figure and
# no more than a tenth short of it: a model changed so that she rolls
# further than her figure, or a figure left far over what she now needs,
# both turn it red. The figure is written 2 per cent over what she used,
# rounded up to 100 ft, for the drift between machines.

if(NOT DEFINED PROGRAM OR NOT DEFINED AIRCRAFT OR NOT DEFINED FIGURES)
    message(FATAL_ERROR "measured_landing.cmake needs PROGRAM, AIRCRAFT and FIGURES")
endif()

file(READ "${FIGURES}" _figures)
if(NOT _figures MATCHES "<landing_distance ft=\"([0-9]+)\" basis=\"measured\"")
    message(FATAL_ERROR "${FIGURES} gives no measured <landing_distance>")
endif()
set(_ft "${CMAKE_MATCH_1}")
# Metres, to the metre: ft * 0.3048, in whole numbers.
math(EXPR _figure_m "${_ft} * 3048 / 10000")
math(EXPR _least_m "${_figure_m} * 9 / 10")

execute_process(COMMAND "${PROGRAM}" land "${AIRCRAFT}"
                RESULT_VARIABLE _exit
                OUTPUT_VARIABLE _out
                ERROR_VARIABLE  _err)
string(REPLACE "\r\n" "\n" _out "${_out}")
if(NOT _exit EQUAL 0 OR NOT _out MATCHES "stopped ([0-9]+) m past the threshold")
    message(FATAL_ERROR "${AIRCRAFT} did not land and stop on the runway (exit ${_exit})\n"
                        "--- stdout ---\n${_out}--- stderr ---\n${_err}")
endif()
set(_stopped_m "${CMAKE_MATCH_1}")
message(STATUS "${AIRCRAFT}: stopped ${_stopped_m} m past the threshold; her figure, "
               "${_ft} ft, is ${_figure_m} m")
if(_stopped_m GREATER _figure_m OR _stopped_m LESS _least_m)
    message(FATAL_ERROR "${AIRCRAFT} stopped ${_stopped_m} m past the threshold, not from "
                        "${_least_m} to ${_figure_m} m: her measured ${_ft} ft is not what "
                        "the measurement makes now")
endif()

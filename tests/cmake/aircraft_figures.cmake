# aircraft_figures.cmake - the figures glideslope_cli prints for an aircraft must
# be the ones in its model file.
#
#   cmake -DPROGRAM=<glideslope_cli> -DDATA=<data dir> -DMODEL=<name>
#         -P aircraft_figures.cmake
#
# The expected numbers are read from the model's XML here, independently of
# JSBSim, so the test is not JSBSim agreeing with itself. Each element's unit is
# required to be the one the CLI reports in, so a model written in metres fails
# loudly rather than comparing feet with metres.

set(_xml "${DATA}/jsbsim/aircraft/${MODEL}/${MODEL}.xml")
if(NOT EXISTS "${_xml}")
    message(FATAL_ERROR "no model file at ${_xml}")
endif()
file(READ "${_xml}" _text)

set(_failures "")
set(_checked 0)

function(expect_figure element unit label)
    if(NOT _text MATCHES "<${element}[ \t]+unit=\"([A-Za-z0-9]+)\"[ \t]*>[ \t\r\n]*([-0-9.]+)[ \t\r\n]*</${element}>")
        set(_failures "${_failures}\n  ${element}: not found in ${_xml}" PARENT_SCOPE)
        return()
    endif()
    set(_unit "${CMAKE_MATCH_1}")
    set(_want "${CMAKE_MATCH_2}")
    if(NOT _unit STREQUAL "${unit}")
        set(_failures "${_failures}\n  ${element}: in ${_unit}, the CLI reports ${unit}" PARENT_SCOPE)
        return()
    endif()
    if(NOT _out MATCHES "\n${label}[ ]+([-0-9.]+)")
        set(_failures "${_failures}\n  '${label}' is not in the CLI's output" PARENT_SCOPE)
        return()
    endif()
    set(_got "${CMAKE_MATCH_1}")
    # The CLI prints one decimal place; the file may have none or several.
    string(REGEX REPLACE "^([-0-9]+)$" "\\1.0" _want_padded "${_want}")
    string(REGEX MATCH "^-?[0-9]+\\.[0-9]" _want_rounded "${_want_padded}0")
    if(NOT _got STREQUAL _want_rounded)
        set(_failures "${_failures}\n  ${label}: the CLI says ${_got}, the file says ${_want}" PARENT_SCOPE)
    endif()
    math(EXPR _n "${_checked} + 1")
    set(_checked ${_n} PARENT_SCOPE)
endfunction()

execute_process(COMMAND "${PROGRAM}" aircraft "${MODEL}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
string(REPLACE "\r\n" "\n" _out "\n${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope_cli aircraft ${MODEL} exited ${_rc}\n${_out}${_err}")
endif()

expect_figure(wingarea  FT2 "wing area")
expect_figure(wingspan  FT  "wingspan")
expect_figure(chord     FT  "chord")
expect_figure(emptywt   LBS "empty weight")

string(REGEX MATCHALL "<engine[ \t]+file=" _engines "${_text}")
list(LENGTH _engines _want_engines)
if(NOT _out MATCHES "\nengines[ ]+([0-9]+)")
    list(APPEND _failures "\n  'engines' is not in the CLI's output")
elseif(NOT CMAKE_MATCH_1 EQUAL _want_engines)
    list(APPEND _failures "\n  engines: the CLI says ${CMAKE_MATCH_1}, the file has ${_want_engines}")
else()
    math(EXPR _checked "${_checked} + 1")
endif()

message(STATUS "checked ${_checked} of 5 figures for ${MODEL}")
if(_failures OR NOT _checked EQUAL 5)
    message(FATAL_ERROR "glideslope_cli's figures for ${MODEL} are wrong:${_failures}\n${_out}")
endif()

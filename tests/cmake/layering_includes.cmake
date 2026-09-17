# layering_includes.cmake - walk every include the simulation may not have.
#
#   cmake -DGLIDESLOPE_ROOT=<source dir> -DWORK=<scratch dir>
#         -DCASES=<forbidden|allowed> -P layering_includes.cmake
#
# forbidden  each case writes one small tree whose src/sim/ holds one violation
#            on line 3, and requires the check to refuse it naming the file,
#            the line and the include. The cases come from the lists in
#            cmake/Layering.cmake, so an entry added there is walked here
#            without anybody remembering to, and the count is checked:
#              - every forbidden SDL header, as <SDL3/...>
#              - every forbidden layer, as "layer/x.hpp", "../layer/x.hpp"
#                and <layer/x.hpp>
#              - every source extension the check scans, with one violation
#              - a violation in a subdirectory, one written "#  include", one
#                indented, and one after a line with an unclosed '[' and a ';'
# allowed    one tree full of lines that look close to violations and are not,
#            which the check must accept

include("${GLIDESLOPE_ROOT}/cmake/Layering.cmake")
set(_runner "${GLIDESLOPE_ROOT}/tests/cmake/run_layering.cmake")
set(_failures "")
set(_walked 0)

# The file's text is read from _content rather than passed as an argument:
# C++ is full of ';', and an argument list would split on every one of them.
function(layering_case label relpath include_line expect)
    set(_tree "${WORK}/${_walked}")
    file(REMOVE_RECURSE "${_tree}")
    file(WRITE "${_tree}/src/sim/${relpath}" "${_content}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DGLIDESLOPE_ROOT=${GLIDESLOPE_ROOT}"
                "-DROOT=${_tree}" -P "${_runner}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_all "${_out}${_err}")
    set(_problem "")
    if(expect STREQUAL "REFUSE")
        if(_rc EQUAL 0)
            set(_problem "was accepted, should have been refused")
        else()
            # The message wraps long lines, so compare with whitespace squeezed.
            string(REGEX REPLACE "[ \t\r\n]+" " " _flat "${_all}")
            foreach(_needle "src/sim/${relpath}:3:" "${include_line}")
                string(REGEX REPLACE "[ \t\r\n]+" " " _needle "${_needle}")
                string(FIND "${_flat}" "${_needle}" _at)
                if(_at EQUAL -1)
                    set(_problem "refused, but the message does not name '${_needle}'")
                endif()
            endforeach()
        endif()
    elseif(NOT _rc EQUAL 0)
        set(_problem "was refused, should have been accepted")
    endif()
    if(_problem)
        set(_failures "${_failures}\n  ${label}: ${_problem}\n----\n${_all}----" PARENT_SCOPE)
    endif()
    math(EXPR _n "${_walked} + 1")
    set(_walked ${_n} PARENT_SCOPE)
endfunction()

# A violation on line 3 of an otherwise ordinary file.
function(violation label relpath include_line)
    set(_content "// probe\n#include <cmath>\n${include_line}\n")
    layering_case("${label}" "${relpath}" "${include_line}" REFUSE)
    set(_failures "${_failures}" PARENT_SCOPE)
    set(_walked ${_walked} PARENT_SCOPE)
endfunction()

if(CASES STREQUAL "forbidden")
    set(_sdl ${GLIDESLOPE_SIM_FORBIDDEN_SDL_UMBRELLA} ${GLIDESLOPE_SIM_FORBIDDEN_SDL_VIDEO}
             ${GLIDESLOPE_SIM_FORBIDDEN_SDL_INPUT} ${GLIDESLOPE_SIM_FORBIDDEN_SDL_AUDIO})
    list(LENGTH _sdl _n_sdl)
    list(LENGTH GLIDESLOPE_SIM_FORBIDDEN_LAYERS _n_layers)
    list(LENGTH GLIDESLOPE_SIM_SOURCE_EXTENSIONS _n_ext)
    math(EXPR _expected "${_n_sdl} + ${_n_layers} * 3 + ${_n_ext} + 3")

    foreach(_h IN LISTS _sdl)
        violation("SDL ${_h}" "probe.cpp" "#include <SDL3/${_h}>")
    endforeach()
    foreach(_layer IN LISTS GLIDESLOPE_SIM_FORBIDDEN_LAYERS)
        violation("${_layer}/" "probe.cpp" "#include \"${_layer}/x.hpp\"")
        violation("../${_layer}/" "probe.cpp" "#include \"../${_layer}/x.hpp\"")
        violation("<${_layer}/>" "probe.cpp" "#include <${_layer}/x.hpp>")
    endforeach()
    foreach(_ext IN LISTS GLIDESLOPE_SIM_SOURCE_EXTENSIONS)
        violation(".${_ext}" "probe.${_ext}" "#include <SDL3/SDL_video.h>")
    endforeach()
    violation("subdirectory" "deep/er/probe.cpp" "#include \"gfx/renderer.hpp\"")
    violation("#  include" "probe.cpp" "#  include <SDL3/SDL_audio.h>")
    string(CONCAT _content
        "auto f = [&](int x) { return x; }; int a[\n"
        "3]; // a bracket left open across a line\n"
        "#include <SDL3/SDL_gamepad.h>\n")
    layering_case("after an unclosed [" "probe.cpp" "#include <SDL3/SDL_gamepad.h>" REFUSE)

elseif(CASES STREQUAL "allowed")
    set(_expected 1)
    string(CONCAT _content
        "#include <SDL3/SDL_stdinc.h>\n"
        "#include <SDL3/SDL_atomic.h>\n"
        "#include <SDL3/SDL_timer.h>\n"
        "#include \"sim/version.hpp\"\n"
        "#include \"world/terrain.hpp\"\n"
        "#include \"gfxtools/x.hpp\"\n"
        "#include \"uikit.hpp\"\n"
        "#include \"platforms/x.hpp\"\n"
        "#include <cmath>\n"
        "// #include <SDL3/SDL_video.h>\n"
        "/* #include \"gfx/renderer.hpp\" */\n"
        "const char* text = \"#include <SDL3/SDL_audio.h>\";\n"
        "int a[3]; auto f = [&](int x) { return x; };\n")
    layering_case("near misses" "probe.cpp" "" ACCEPT)

else()
    message(FATAL_ERROR "layering_includes.cmake: CASES must be forbidden or allowed")
endif()

message(STATUS "walked ${_walked} of ${_expected} cases")
if(NOT _walked EQUAL _expected)
    set(_failures "${_failures}\n  walked ${_walked} cases, expected ${_expected}")
endif()
if(_failures)
    message(FATAL_ERROR "the include check is wrong:${_failures}")
endif()

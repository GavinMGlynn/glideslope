# copilot_walls.cmake - the three things that keep a language model's copilot
# from driving the aircraft (cmake/Copilot.cmake), each walked.
#
#   cmake -DGLIDESLOPE_ROOT=<source dir> -DWORK=<scratch> -DSTAGE=<the copilot's stage>
#         -DINCLUDES=<the copilot target's include directories, |-separated>
#         -DCXX=<compiler> -DCXX_STYLE=<gnu|msvc> [-DNM=<nm>] -P copilot_walls.cmake
#
# 1. **The source rules**: every way an include could reach past the staged
#    copy, and every file the copilot's build would not compile, refused at
#    configure time naming the file and the line; and a tree of lines that
#    only look like them, accepted.
# 2. **What it can see**: compiled with the include directories the copilot's
#    target is built with - which must be the stage alone -
#      - every header under src/ that is not staged fails to include;
#      - every class, struct and enum the simulation's headers declare, other
#        than the plan's, fails to be named, with every staged header included;
#      - the ways a name can be spelled past a reading of the source - a
#        macro pasting it together, a raw string, digit separators, a name
#        split over lines - fail too, since the compiler is what reads them;
#      - and a file naming everything the copilot may name compiles, so that
#        the failures are for the reason given.
# 3. **Its symbols** (where there is an nm): objects that declare a function
#    of the simulation's by hand, call one by its mangled name, or define
#    something in another part's namespace are refused, and one that uses
#    only what is allowed is accepted.
#
# Each walk counts its cases against the size of what it walks - headers and
# declarations from the tree itself - and fails when they differ.

cmake_minimum_required(VERSION 3.28)
include("${GLIDESLOPE_ROOT}/cmake/Copilot.cmake")
set(_failures "")
set(_walked 0)
set(_expected 0)

macro(failed what)
    string(APPEND _failures "\n  ${what}")
endmacro()

# ---- 1. the source rules --------------------------------------------------
# `_content` holds the file, with @SEMI@ for ';', which an argument list would
# split on; the violation is on line 3.
function(source_case label relpath expect)
    set(_tree "${WORK}/sources/${label}")
    string(MAKE_C_IDENTIFIER "${label}" _id)
    set(_tree "${WORK}/sources/${_id}")
    file(REMOVE_RECURSE "${_tree}")
    string(REPLACE "@SEMI@" ";" _text "${_content}")
    file(WRITE "${_tree}/src/copilot/${relpath}" "${_text}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DGLIDESLOPE_ROOT=${GLIDESLOPE_ROOT}" "-DROOT=${_tree}"
                -P "${GLIDESLOPE_ROOT}/tests/cmake/run_copilot_sources.cmake"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    string(REGEX REPLACE "[ \t\r\n]+" " " _flat "${_out}${_err}")
    if(expect STREQUAL "REFUSE")
        if(_rc EQUAL 0)
            set(_failures "${_failures}\n  source rule, ${label}: accepted" PARENT_SCOPE)
        elseif(NOT _flat MATCHES "src/copilot/${relpath}")
            set(_failures "${_failures}\n  source rule, ${label}: refused without naming the file: ${_flat}" PARENT_SCOPE)
        endif()
    elseif(NOT _rc EQUAL 0)
        set(_failures "${_failures}\n  source rule, ${label}: refused: ${_flat}" PARENT_SCOPE)
    endif()
    math(EXPR _n "${_walked} + 1")
    set(_walked ${_n} PARENT_SCOPE)
endfunction()

set(_source_cases
    "a digraph|probe.cpp|%:include \"sim/aircraft.hpp\""
    "a trigraph|probe.cpp|??=include \"sim/aircraft.hpp\""
    "include_next|probe.cpp|#include_next <sim/aircraft.hpp>"
    "import|probe.cpp|#import \"sim/aircraft.hpp\""
    "a macro include|probe.cpp|#include HEADER"
    "an absolute path|probe.cpp|#include \"/home/x/src/sim/aircraft.hpp\""
    "an absolute angle path|probe.cpp|#include </src/sim/aircraft.hpp>"
    "a drive letter|probe.cpp|#include \"C:/src/sim/aircraft.hpp\""
    "dot dot|probe.cpp|#include \"../sim/aircraft.hpp\""
    "dot dot inside|probe.cpp|#include \"copilot/../../src/sim/aircraft.hpp\""
    "a backslash|probe.cpp|#include \"sim\\aircraft.hpp\""
    "an indented include of dot dot|probe.cpp|  #  include \"../sim/aircraft.hpp\""
    "a .tpp file|probe.tpp|int x@SEMI@"
    "a .cppm file|probe.cppm|int x@SEMI@"
    "a .h file|probe.h|int x@SEMI@"
    "a .inc file|probe.inc|int x@SEMI@"
    "a file with no extension|probe|int x@SEMI@")
list(LENGTH _source_cases _n)
math(EXPR _expected "${_expected} + ${_n} + 2")
# A line splice, written here rather than in the list above, where its
# backslash would escape the list's own separator.
string(CONCAT _content "// a probe\nint y = 0@SEMI@\n" "int x = 1@SEMI@ \\" "\n")
source_case("a line splice" "probe.cpp" REFUSE)
foreach(_case IN LISTS _source_cases)
    string(REPLACE "|" ";" _parts "${_case}")
    list(GET _parts 0 _label)
    list(GET _parts 1 _path)
    list(GET _parts 2 _line)
    string(CONCAT _content "// a probe\nint y = 0@SEMI@\n" "${_line}" "\n")
    source_case("${_label}" "${_path}" REFUSE)
endforeach()
string(CONCAT _content
    "#include \"copilot/provider.hpp\"\n"
    "#include \"sim/plan.hpp\"\n"
    "#include <string>\n"
    "// a comment that mentions ../sim/aircraft.hpp and /abs/path\n"
    "const char* s = \"#include \\\"/abs/sim/aircraft.hpp\\\"\"@SEMI@\n"
    "int x = 1 // a comment\n@SEMI@\n")
source_case("near misses" "probe.cpp" ACCEPT)

# ---- 2. what it can see ---------------------------------------------------
# The target's own include directories, as its build has them - its own and
# every linked target's - which must be the stage and nothing else.
string(REPLACE "|" ";" _includes "${INCLUDES}")
list(REMOVE_DUPLICATES _includes)
if(NOT _includes STREQUAL "${STAGE}")
    failed("the copilot is built with the include directories ${_includes}, not its stage "
           "alone (${STAGE})")
endif()
set(_syntax "")
if(CXX_STYLE STREQUAL "msvc")
    list(APPEND _syntax /nologo /Zs /std:c++20 /EHsc)
    foreach(_i IN LISTS _includes)
        list(APPEND _syntax /I "${_i}")
    endforeach()
else()
    list(APPEND _syntax -fsyntax-only -std=c++20)
    foreach(_i IN LISTS _includes)
        list(APPEND _syntax -I "${_i}")
    endforeach()
endif()
# Compiles `_content` as a copilot source would be; `_compiled` says whether it did.
function(compiles label)
    string(MAKE_C_IDENTIFIER "${label}" _id)
    set(_file "${WORK}/see/${_id}.cpp")
    string(REPLACE "@SEMI@" ";" _text "${_content}")
    file(WRITE "${_file}" "${_text}")
    execute_process(COMMAND "${CXX}" ${_syntax} "${_file}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(_rc EQUAL 0)
        set(_compiled TRUE PARENT_SCOPE)
    else()
        set(_compiled FALSE PARENT_SCOPE)
    endif()
    set(_said "${_out}${_err}" PARENT_SCOPE)
    math(EXPR _n "${_walked} + 1")
    set(_walked ${_n} PARENT_SCOPE)
endfunction()

set(_all_visible "")
foreach(_h IN LISTS GLIDESLOPE_COPILOT_VISIBLE)
    string(APPEND _all_visible "#include \"${_h}\"\n")
endforeach()

# Everything the copilot may name, used: this must compile.
string(CONCAT _content "${_all_visible}"
    "#include \"copilot/planner.hpp\"\n"
    "glideslope::sim::FlightPlan p = glideslope::sim::parse_flight_plan(\"\")@SEMI@\n"
    "glideslope::sim::AutopilotModes m@SEMI@ glideslope::sim::Runway r@SEMI@\n"
    "glideslope::world::Json j = glideslope::world::parse_json(\"1\")@SEMI@\n"
    "glideslope::platform::HttpRequest q@SEMI@\n"
    "double d = glideslope::sim::least_orbit_radius_m(90) + glideslope::sim::distance_m(1, 2, 3, 4)@SEMI@\n")
compiles("everything allowed")
math(EXPR _expected "${_expected} + 1")
if(NOT _compiled)
    failed("what the copilot may name did not compile, so nothing below means anything:\n${_said}")
endif()

# Every header under src/ that is not staged.
file(GLOB_RECURSE _headers RELATIVE "${GLIDESLOPE_ROOT}/src" "${GLIDESLOPE_ROOT}/src/*.hpp"
     "${GLIDESLOPE_ROOT}/src/*.h")
set(_unstaged 0)
foreach(_h IN LISTS _headers)
    if(_h IN_LIST GLIDESLOPE_COPILOT_VISIBLE OR _h MATCHES "^copilot/")
        continue()
    endif()
    math(EXPR _unstaged "${_unstaged} + 1")
    set(_content "#include \"${_h}\"\nint x@SEMI@\n")
    compiles("include ${_h}")
    if(_compiled)
        failed("${_h}, which is not staged, was included")
    endif()
endforeach()
if(_unstaged LESS 50)
    failed("only ${_unstaged} headers under src/ were walked")
endif()
math(EXPR _expected "${_expected} + ${_unstaged}")

# Every class, struct and enum in the simulation's headers, other than the plan's.
file(GLOB _sim_headers "${GLIDESLOPE_ROOT}/src/sim/*.hpp")
set(_declared "")
foreach(_h IN LISTS _sim_headers)
    if(_h MATCHES "/plan\\.hpp$")
        continue()
    endif()
    file(STRINGS "${_h}" _decls REGEX "^[ \t]*(class|struct|enum class|enum)[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*[:{]")
    foreach(_d IN LISTS _decls)
        string(REGEX REPLACE "^[ \t]*(class|struct|enum class|enum)[ \t]+([A-Za-z_][A-Za-z0-9_]*).*" "\\2" _name "${_d}")
        if(NOT "sim::${_name}" IN_LIST GLIDESLOPE_COPILOT_NAMES AND NOT _name IN_LIST _declared)
            list(APPEND _declared "${_name}")
        endif()
    endforeach()
endforeach()
list(LENGTH _declared _n_declared)
if(_n_declared LESS 20)
    failed("only ${_n_declared} names found in the simulation's headers: ${_declared}")
endif()
foreach(_name IN LISTS _declared)
    string(CONCAT _content "${_all_visible}" "glideslope::sim::${_name}* p = nullptr@SEMI@\n")
    compiles("name sim ${_name}")
    if(_compiled)
        failed("sim::${_name} was named")
    endif()
endforeach()
math(EXPR _expected "${_expected} + ${_n_declared}")

# The ways past a reading of the source that the review found, each as a
# control position, sim::Controls, which is what must not be reached.
set(_spellings
    "#define CAT(a, b) a##b\nglideslope::CAT(si, m)::CAT(Contr, ols) c@SEMI@\n"
    "auto a = R\"x(\")x\"@SEMI@ glideslope::sim::Controls c@SEMI@ auto b = \"\"@SEMI@\n"
    "int n = 1'000@SEMI@ glideslope::sim::Controls c@SEMI@ char q = 'x'@SEMI@\n"
    "auto s = R\"(\n/*\n)\"@SEMI@\nglideslope::sim::Controls c@SEMI@\n"
    "namespace s\n= glideslope::sim@SEMI@\ns::Prediction p@SEMI@\n"
    "glideslope::sim\n::Controls c@SEMI@\n"
    "using namespace glideslope::sim@SEMI@\nControls c@SEMI@\n"
    "auto c = glideslope::platform::Control::elevator@SEMI@\n"
    "glideslope::net::InputFrame f@SEMI@\n")
list(LENGTH _spellings _n_spellings)
set(_k 0)
foreach(_spelling IN LISTS _spellings)
    math(EXPR _k "${_k} + 1")
    string(CONCAT _content "${_all_visible}" "${_spelling}")
    compiles("spelling ${_k}")
    if(_compiled)
        failed("spelling ${_k} reached a control:\n${_spelling}")
    endif()
endforeach()
math(EXPR _expected "${_expected} + ${_n_spellings}")

# ---- 3. its symbols -------------------------------------------------------
if(NM AND NOT CXX_STYLE STREQUAL "msvc")
    list(JOIN GLIDESLOPE_COPILOT_NAMES "|" _names)
    function(symbols_case label expect)
        string(MAKE_C_IDENTIFIER "${label}" _id)
        set(_file "${WORK}/symbols/${_id}.cpp")
        string(REPLACE "@SEMI@" ";" _text "${_content}")
        file(WRITE "${_file}" "${_text}")
        execute_process(COMMAND "${CXX}" -std=c++20 -c -I "${STAGE}" "${_file}" -o "${_file}.o"
            RESULT_VARIABLE _rc ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            set(_failures "${_failures}\n  symbols, ${label}: did not compile: ${_err}" PARENT_SCOPE)
        else()
            execute_process(COMMAND "${CMAKE_COMMAND}" "-DNM=${NM}" "-DARCHIVE=${_file}.o"
                    "-DNAMES=${_names}" -P "${GLIDESLOPE_ROOT}/cmake/copilot_symbols.cmake"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
            if(expect STREQUAL "REFUSE" AND _rc EQUAL 0)
                set(_failures "${_failures}\n  symbols, ${label}: accepted" PARENT_SCOPE)
            elseif(expect STREQUAL "ACCEPT" AND NOT _rc EQUAL 0)
                set(_failures "${_failures}\n  symbols, ${label}: refused: ${_out}${_err}" PARENT_SCOPE)
            endif()
        endif()
        math(EXPR _n "${_walked} + 1")
        set(_walked ${_n} PARENT_SCOPE)
    endfunction()
    set(_content
        "namespace glideslope::sim { struct Aircraft@SEMI@ struct Controls { double elevator@SEMI@ }@SEMI@\n"
        "void steer(Aircraft&, const Controls&)@SEMI@ }\n"
        "void f(glideslope::sim::Aircraft& a) { glideslope::sim::steer(a, {1.0})@SEMI@ }\n")
    symbols_case("a simulation function declared by hand" REFUSE)
    set(_content
        "extern \"C\" void _ZN10glideslope3sim8Aircraft12set_controlsERKNS0_8ControlsE(void*, const void*)@SEMI@\n"
        "void f(void* a, const void* c) { _ZN10glideslope3sim8Aircraft12set_controlsERKNS0_8ControlsE(a, c)@SEMI@ }\n")
    symbols_case("one called by its mangled name" REFUSE)
    set(_content "namespace glideslope::sim { double throttle() { return 1.0@SEMI@ } }\n")
    symbols_case("something defined in the simulation's namespace" REFUSE)
    set(_content "namespace glideslope::net { int frame() { return 1@SEMI@ } }\n")
    symbols_case("something defined in the network's namespace" REFUSE)
    set(_content
        "#include \"sim/plan.hpp\"\n"
        "namespace glideslope::copilot { double d() { return glideslope::sim::least_orbit_radius_m(90) "
        "+ static_cast<double>(glideslope::sim::parse_flight_plan(\"\").waypoints.size())@SEMI@ } }\n")
    symbols_case("only what is allowed" ACCEPT)
    math(EXPR _expected "${_expected} + 5")
else()
    message(STATUS "no nm with this compiler: the symbol cases are walked on the platforms that have one")
endif()

message(STATUS "walked ${_walked} of ${_expected} cases")
if(NOT _walked EQUAL _expected)
    failed("walked ${_walked} cases, expected ${_expected}")
endif()
if(_failures)
    message(FATAL_ERROR "the copilot's walls are wrong:${_failures}")
endif()

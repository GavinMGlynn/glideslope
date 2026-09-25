# copilot_names.cmake - walk every way the copilot could reach a control, and
# the lines that only look like one.
#
#   cmake -DGLIDESLOPE_ROOT=<source dir> -DWORK=<scratch dir>
#         -DCASES=<forbidden|allowed> -P copilot_names.cmake
#
# forbidden  each case writes one small tree whose src/copilot/ holds one
#            violation on line 3, and requires cmake/Copilot.cmake to refuse
#            it naming the file and the line. The cases come from the lists
#            in Copilot.cmake and from the simulation's own headers, so a name
#            added to either is walked here without anybody remembering to,
#            and the count is checked:
#              - every forbidden name, qualified `glideslope::sim::` and bare;
#              - every class, struct and enum a header in src/sim/ declares
#                that is not on the allowed list, as `sim::` it;
#              - every header of src/sim/ not allowed, included;
#              - every way of hiding a name's namespace: using namespace,
#                opening the namespace, and renaming it;
#              - every source extension the check scans;
#              - a violation after a block comment that ended, after a string
#                holding "//", and in a subdirectory.
# allowed    one tree whose lines only look like violations - names in
#            comments and strings, allowed names, longer words holding a
#            forbidden one - which the check must accept.

cmake_minimum_required(VERSION 3.28)
include("${GLIDESLOPE_ROOT}/cmake/Copilot.cmake")
set(_runner "${GLIDESLOPE_ROOT}/tests/cmake/run_copilot_check.cmake")
set(_failures "")
set(_walked 0)

# The file's text comes from _content: C++ is full of ';', which an argument
# list would split on.
function(copilot_case label relpath expect)
    set(_tree "${WORK}/${_walked}")
    file(REMOVE_RECURSE "${_tree}")
    # ';' is written @SEMI@ above, where a list would split on it.
    string(REPLACE "@SEMI@" ";" _text "${_content}")
    file(WRITE "${_tree}/src/copilot/${relpath}" "${_text}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DGLIDESLOPE_ROOT=${GLIDESLOPE_ROOT}" "-DROOT=${_tree}"
                -P "${_runner}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_all "${_out}${_err}")
    set(_problem "")
    if(expect STREQUAL "REFUSE")
        string(REGEX REPLACE "[ \t\r\n]+" " " _flat "${_all}")
        if(_rc EQUAL 0)
            set(_problem "was accepted, should have been refused")
        else()
            string(FIND "${_flat}" "src/copilot/${relpath}:3:" _at)
            if(_at EQUAL -1)
                set(_problem "refused, but not naming src/copilot/${relpath}:3")
            endif()
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

# One violation, on line 3.
function(violation label relpath line)
    string(CONCAT _content "#include \"sim/navigator.hpp\"\n// a comment\n" "${line}" "\n")
    copilot_case("${label}" "${relpath}" REFUSE)
    set(_failures "${_failures}" PARENT_SCOPE)
    set(_walked ${_walked} PARENT_SCOPE)
endfunction()

if(CASES STREQUAL "forbidden")
    set(_expected 0)
    foreach(_name IN LISTS GLIDESLOPE_COPILOT_FORBIDDEN)
        violation("glideslope::sim::${_name}" "probe.cpp" "glideslope::sim::${_name}* p = nullptr@SEMI@")
        violation("${_name}, bare" "probe.cpp" "auto p = ${_name}{}@SEMI@")
        math(EXPR _expected "${_expected} + 2")
    endforeach()

    # Everything the simulation's headers declare that is not a plan or a mode.
    file(GLOB _sim_headers "${GLIDESLOPE_ROOT}/src/sim/*.hpp")
    set(_declared "")
    foreach(_h IN LISTS _sim_headers)
        file(STRINGS "${_h}" _decls REGEX "^(class|struct|enum class|enum)[ \t]+[A-Za-z_][A-Za-z0-9_]*")
        foreach(_d IN LISTS _decls)
            string(REGEX REPLACE "^(class|struct|enum class|enum)[ \t]+([A-Za-z_][A-Za-z0-9_]*).*" "\\2" _name "${_d}")
            if(NOT _name IN_LIST GLIDESLOPE_COPILOT_SIM_ALLOWED AND NOT _name IN_LIST _declared)
                list(APPEND _declared "${_name}")
            endif()
        endforeach()
    endforeach()
    list(LENGTH _declared _n_declared)
    if(_n_declared LESS 20)
        message(FATAL_ERROR "only ${_n_declared} names found in src/sim/'s headers: ${_declared}")
    endif()
    foreach(_name IN LISTS _declared)
        violation("sim::${_name}" "probe.cpp" "void f(const sim::${_name}& x)@SEMI@")
        math(EXPR _expected "${_expected} + 1")
    endforeach()

    foreach(_h IN LISTS _sim_headers)
        get_filename_component(_file "${_h}" NAME)
        if(NOT _file IN_LIST GLIDESLOPE_COPILOT_SIM_HEADERS)
            violation("sim/${_file}" "probe.cpp" "#include \"sim/${_file}\"")
            math(EXPR _expected "${_expected} + 1")
        endif()
    endforeach()

    foreach(_hiding
            "using namespace glideslope::sim@SEMI@"
            "using namespace glideslope@SEMI@"
            "using namespace sim@SEMI@"
            "namespace glideslope::sim {"
            "namespace sim {"
            "namespace s = glideslope::sim@SEMI@"
            "namespace g = glideslope@SEMI@")
        violation("${_hiding}" "probe.cpp" "${_hiding}")
        math(EXPR _expected "${_expected} + 1")
    endforeach()

    foreach(_ext IN LISTS GLIDESLOPE_COPILOT_SOURCE_EXTENSIONS)
        violation(".${_ext}" "probe.${_ext}" "sim::Controls c@SEMI@")
        math(EXPR _expected "${_expected} + 1")
    endforeach()
    violation("subdirectory" "deep/er/probe.cpp" "sim::Aircraft* a = nullptr@SEMI@")
    violation("after a string holding //" "probe.cpp"
              "const char* u = \"https://x\"@SEMI@ sim::Controls c@SEMI@")
    string(CONCAT _content "/* a block comment\n   that ends here */\n"
                  "int x@SEMI@ /* and one */ sim::Controller* c@SEMI@\n")
    copilot_case("after block comments" "probe.cpp" REFUSE)
    math(EXPR _expected "${_expected} + 3")

elseif(CASES STREQUAL "allowed")
    set(_expected 1)
    string(CONCAT _content
        "#include \"sim/navigator.hpp\"\n"
        "#include \"sim/lander.hpp\"\n"
        "#include \"sim/autopilot.hpp\"\n"
        "#include \"world/runways.hpp\"\n"
        "// Controls, sim::Aircraft and to_ai are named in comments.\n"
        "/* as is sim::Controller,\n"
        "   over two lines: set_controls */\n"
        "const char* said = \"sim::Controls and Aircraft, in a string\"@SEMI@\n"
        "const char q = '\"'@SEMI@ int after_a_quote_character = 0@SEMI@\n"
        "sim::FlightPlan plan = sim::parse_flight_plan(text)@SEMI@\n"
        "std::vector<sim::Waypoint> w@SEMI@ sim::Runway r@SEMI@ sim::AutopilotModes m@SEMI@\n"
        "double d = sim::distance_m(1, 2, 3, 4) + sim::bearing_deg(1, 2, 3, 4)@SEMI@\n"
        "try {} catch (const sim::FlightPlanError&) {}\n"
        "std::string aircraft@SEMI@ int ControlsShown = 0@SEMI@ int my_Controls = 0@SEMI@\n"
        "int to_ai_ = 0@SEMI@ bool AircraftClass = true@SEMI@\n")
    copilot_case("near misses" "probe.cpp" ACCEPT)

else()
    message(FATAL_ERROR "copilot_names.cmake: CASES must be forbidden or allowed")
endif()

message(STATUS "walked ${_walked} of ${_expected} cases")
if(NOT _walked EQUAL _expected)
    set(_failures "${_failures}\n  walked ${_walked} cases, expected ${_expected}")
endif()
if(_failures)
    message(FATAL_ERROR "the copilot's names check is wrong:${_failures}")
endif()

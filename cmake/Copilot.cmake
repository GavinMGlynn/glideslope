# Copilot.cmake - a language model never drives a control surface, checked at
# configure time rather than remembered (REQUIREMENTS.md section 5: "The LLM
# plans; the controllers fly").
#
# **What the copilot may name of the simulation is a short list**: a flight
# plan and what it is made of, a runway, and the autopilot's modes - what a
# pilot in command of an autopilot hands it. Nothing else. So `src/copilot/`
# can make a plan and ask for modes, and cannot make, set or even name a
# control position, the aircraft, its controller or the autopilot that flies
# it, however it came by the header that declares them.
#
#   glideslope_check_copilot(<root>)
#
# refuses the configure, naming the file, the line and the reason, for any
# line of code in `src/copilot/` - comments and string literals are not code,
# and are passed over - that:
#
#   - names `sim::X` for an X not in GLIDESLOPE_COPILOT_SIM_ALLOWED;
#   - names one of GLIDESLOPE_COPILOT_FORBIDDEN, however qualified: the
#     control positions and whatever holds or applies them;
#   - opens the simulation's namespace, or brings it in or renames it, which
#     would let a name be used without `sim::` in front of it;
#   - includes a header of `sim/` not in GLIDESLOPE_COPILOT_SIM_HEADERS.
#
# It runs in script mode as well, so tests/cmake/copilot_names.cmake can walk
# every refusal without a project around it.

# The simulation's names the copilot may use: the plan, its parts, and the
# modes an autopilot is asked for.
set(GLIDESLOPE_COPILOT_SIM_ALLOWED
    FlightPlan FlightPlanError Waypoint parse_flight_plan least_orbit_radius_m distance_m bearing_deg
    Runway AutopilotModes)
# Names it may not use however it writes them: every control position, and
# everything that holds, sets or applies one.
set(GLIDESLOPE_COPILOT_FORBIDDEN
    Controls set_controls Controller Aircraft Autopilot Navigator Departure Lander
    set_property set_pilot to_pilot to_ai to_ai_flying to_ai_take_off to_ai_approach
    TestPilot)
# The headers of sim/ it may include.
set(GLIDESLOPE_COPILOT_SIM_HEADERS
    navigator.hpp lander.hpp autopilot.hpp)
set(GLIDESLOPE_COPILOT_SOURCE_EXTENSIONS
    h hh hpp hxx inl ipp c cc cpp cxx mm)

function(glideslope_check_copilot root)
    set(_globs "")
    foreach(_ext IN LISTS GLIDESLOPE_COPILOT_SOURCE_EXTENSIONS)
        list(APPEND _globs "${root}/src/copilot/*.${_ext}")
    endforeach()
    file(GLOB_RECURSE _sources ${_globs})
    list(SORT _sources)
    list(JOIN GLIDESLOPE_COPILOT_FORBIDDEN "|" _forbidden)
    list(JOIN GLIDESLOPE_COPILOT_SIM_HEADERS "|" _headers)
    string(REPLACE "." "\\." _headers "${_headers}")

    set(_violations "")
    foreach(_f IN LISTS _sources)
        file(RELATIVE_PATH _rel "${root}" "${_f}")
        # Line by line, as Layering.cmake reads: CMake's list characters are
        # swapped out first, and put back in what is reported.
        file(READ "${_f}" _text)
        string(REPLACE ";" "@GS_SEMI@" _text "${_text}")
        string(REPLACE "[" "@GS_OPEN@" _text "${_text}")
        string(REPLACE "]" "@GS_CLOSE@" _text "${_text}")
        string(REPLACE "\r" "" _text "${_text}")
        string(REPLACE "\n" ";" _lines "${_text}")

        set(_n 0)
        set(_in_block FALSE)
        foreach(_line IN LISTS _lines)
            math(EXPR _n "${_n} + 1")
            set(_code "${_line}")
            # Inside a block comment until it ends.
            if(_in_block)
                string(FIND "${_code}" "*/" _end)
                if(_end EQUAL -1)
                    continue()
                endif()
                math(EXPR _end "${_end} + 2")
                string(SUBSTRING "${_code}" ${_end} -1 _code)
                set(_in_block FALSE)
            endif()
            # **Code only**: string and character literals, line comments and
            # block comments taken out, whichever begins first - a "//" in a
            # string is the string's, a quote in a comment the comment's.
            string(REGEX REPLACE
                "\"([^\"\\\\]|\\\\.)*\"|'([^'\\\\]|\\\\.)*'|//.*$|/\\*([^*]|\\*+[^*/])*\\*+/"
                " " _code "${_code}")
            string(FIND "${_code}" "/*" _open)
            if(NOT _open EQUAL -1)
                string(SUBSTRING "${_code}" 0 ${_open} _code)
                set(_in_block TRUE)
            endif()

            set(_why "")
            if(_line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"](\\.\\./)*sim/([^>\"]+)[>\"]")
                if(NOT CMAKE_MATCH_2 MATCHES "^(${_headers})$")
                    set(_why "includes sim/${CMAKE_MATCH_2}, which the copilot may not")
                endif()
            elseif(_code MATCHES "(^|[^A-Za-z0-9_])(${_forbidden})([^A-Za-z0-9_]|$)")
                set(_why "names ${CMAKE_MATCH_2}, which drives the aircraft")
            elseif(_code MATCHES "namespace[ \t]+([A-Za-z_][A-Za-z0-9_]*[ \t]*=|glideslope::sim|sim([^A-Za-z0-9_]|$))")
                set(_why "opens, brings in or renames a namespace, which would hide what is named")
            elseif(_code MATCHES "using[ \t]+namespace")
                set(_why "brings a namespace in, which would hide what is named")
            else()
                string(REGEX MATCHALL "sim[ \t]*::[ \t]*[A-Za-z_][A-Za-z0-9_]*" _named "${_code}")
                foreach(_one IN LISTS _named)
                    string(REGEX REPLACE "^sim[ \t]*::[ \t]*" "" _name "${_one}")
                    if(NOT _name IN_LIST GLIDESLOPE_COPILOT_SIM_ALLOWED)
                        set(_why "names sim::${_name}, which is not a plan or a mode")
                        break()
                    endif()
                endforeach()
            endif()

            if(_why)
                string(REPLACE "@GS_SEMI@" ";" _shown "${_line}")
                string(REPLACE "@GS_OPEN@" "[" _shown "${_shown}")
                string(REPLACE "@GS_CLOSE@" "]" _shown "${_shown}")
                string(STRIP "${_shown}" _shown)
                string(APPEND _violations "  ${_rel}:${_n}: ${_shown}    <- ${_why}\n")
            endif()
        endforeach()
    endforeach()

    if(_violations)
        message(FATAL_ERROR
            "src/copilot/ reaches past a plan and the autopilot's modes:\n"
            "${_violations}"
            "A language model's copilot may name only a flight plan, its parts, a "
            "runway and the autopilot's modes. See cmake/Copilot.cmake.")
    endif()
    list(LENGTH _sources _count)
    message(STATUS "glideslope: src/copilot/ names nothing that drives the aircraft (${_count} files)")
endfunction()

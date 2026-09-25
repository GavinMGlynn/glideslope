# Copilot.cmake - a language model never drives a control surface, and the
# compiler and linker say so, not a reading of the source (REQUIREMENTS.md
# section 5: "The LLM plans; the controllers fly").
#
# **What the copilot can see is a short list of headers**, and what it
# defines and calls a short list of names: a flight plan and its parts, a
# runway, the autopilot's modes, JSON, the runways file and an HTTP request.
# None of those headers includes the aircraft, its controls or anything that
# moves them (sim/plan.hpp is built for this). Three things hold it:
#
#   1. **It is compiled from a staged copy** (glideslope_stage_copilot): its
#      own sources, beside only the headers in GLIDESLOPE_COPILOT_VISIBLE,
#      and with no other include directory. A control is not declared in
#      anything it can include, so naming one does not compile - however it
#      is spelled, by macro, splice, raw string or anything else, because the
#      compiler is what reads it.
#   2. **Its includes are plain** (checked at configure time): a literal path,
#      not absolute and without "..", and no line splice or digraph that could
#      make one. Those are the only ways out of the staged copy.
#   3. **Its symbols are checked as it is built** (glideslope_check_copilot_
#      symbols, where the toolchain has `nm`): every name of glideslope's in
#      what it defines or calls must be the copilot's own or on
#      GLIDESLOPE_COPILOT_NAMES. A function of the simulation's declared by
#      hand - even by its mangled name - is refused there, and so is anything
#      the copilot defines in another part's namespace.
#
# It links only glideslope_plan (the plan, JSON and the runways) and
# glideslope_platform (HTTP), and takes neither's include directory.

# The headers the copilot may include, beside its own.
set(GLIDESLOPE_COPILOT_VISIBLE
    sim/plan.hpp world/json.hpp world/runways.hpp platform/http.hpp)

# The names of glideslope's its compiled code may define or call, by part:
# everything in those headers, and nothing else.
set(GLIDESLOPE_COPILOT_NAMES
    sim::FlightPlan sim::FlightPlanError sim::Waypoint sim::Runway sim::AutopilotModes
    sim::parse_flight_plan sim::least_orbit_radius_m sim::distance_m sim::bearing_deg
    sim::most_bank_deg
    world::Json world::JsonError world::parse_json world::write_json
    world::RunwayEnd world::RunwayError world::read_runways world::runways_at world::as_runway
    platform::HttpRequest platform::HttpResponse platform::HttpError platform::http_get
    platform::http_post platform::refuse_unsafe_headers)

# Refuses the configure for anything in `root`/src/copilot/ that is not a
# .cpp or .hpp, or has an include that could reach past the staged copy.
function(glideslope_check_copilot_sources root)
    file(GLOB_RECURSE _files LIST_DIRECTORIES false "${root}/src/copilot/*")
    list(SORT _files)
    set(_violations "")
    foreach(_f IN LISTS _files)
        file(RELATIVE_PATH _rel "${root}" "${_f}")
        if(NOT _f MATCHES "\\.(cpp|hpp)$")
            string(APPEND _violations "  ${_rel}    <- not a .cpp or .hpp, and so not compiled as the copilot is\n")
            continue()
        endif()
        file(READ "${_f}" _text)
        # A backslash too: before the ';' a line becomes, it would escape it,
        # and a spliced line would join the next unseen.
        string(REPLACE "\\" "@GS_BS@" _text "${_text}")
        string(REPLACE ";" "@GS_SEMI@" _text "${_text}")
        string(REPLACE "[" "@GS_OPEN@" _text "${_text}")
        string(REPLACE "]" "@GS_CLOSE@" _text "${_text}")
        string(REPLACE "\r" "" _text "${_text}")
        string(REPLACE "\n" ";" _lines "${_text}")
        set(_n 0)
        foreach(_line IN LISTS _lines)
            math(EXPR _n "${_n} + 1")
            set(_why "")
            if(_line MATCHES "@GS_BS@$")
                set(_why "a line splice, which could join an include together")
            elseif(_line MATCHES "%:|\\?\\?=")
                set(_why "a digraph or trigraph for #, which could spell an include")
            elseif(_line MATCHES "^[ \t]*#[ \t]*(include_next|import)")
                set(_why "#${CMAKE_MATCH_1}, which the copilot does not use")
            elseif(_line MATCHES "^[ \t]*#[ \t]*include")
                if(NOT _line MATCHES "^[ \t]*#[ \t]*include[ \t]*([<\"])([^>\"]*)[>\"]")
                    set(_why "an include that is not a literal path")
                elseif(CMAKE_MATCH_2 MATCHES "^/|^@GS_BS@|^[A-Za-z]:|\\.\\.|@GS_BS@")
                    set(_why "an absolute path, a \"..\" or a backslash, which could reach past the staged copy")
                endif()
            endif()
            if(_why)
                string(REPLACE "@GS_SEMI@" ";" _shown "${_line}")
                string(REPLACE "@GS_BS@" "\\" _shown "${_shown}")
                string(REPLACE "@GS_OPEN@" "[" _shown "${_shown}")
                string(REPLACE "@GS_CLOSE@" "]" _shown "${_shown}")
                string(STRIP "${_shown}" _shown)
                string(APPEND _violations "  ${_rel}:${_n}: ${_shown}    <- ${_why}\n")
            endif()
        endforeach()
    endforeach()
    if(_violations)
        message(FATAL_ERROR
            "src/copilot/ could reach past what it may see:\n${_violations}"
            "The copilot is compiled from a staged copy with only the headers in "
            "GLIDESLOPE_COPILOT_VISIBLE. See cmake/Copilot.cmake.")
    endif()
endfunction()

# Copies the copilot's sources and the headers it may see into `stage`, and
# sets `out` to the staged .cpp files, to be compiled with `stage` as their
# only include directory. configure_file makes each source a dependency of
# the configure, so an edit is staged again before the next build.
function(glideslope_stage_copilot root stage out)
    glideslope_check_copilot_sources("${root}")
    file(REMOVE_RECURSE "${stage}")
    foreach(_h IN LISTS GLIDESLOPE_COPILOT_VISIBLE)
        configure_file("${root}/src/${_h}" "${stage}/${_h}" COPYONLY)
    endforeach()
    file(GLOB_RECURSE _own RELATIVE "${root}/src" "${root}/src/copilot/*.cpp" "${root}/src/copilot/*.hpp")
    set(_sources "")
    foreach(_f IN LISTS _own)
        configure_file("${root}/src/${_f}" "${stage}/${_f}" COPYONLY)
        if(_f MATCHES "\\.cpp$")
            list(APPEND _sources "${stage}/${_f}")
        endif()
    endforeach()
    set(${out} "${_sources}" PARENT_SCOPE)
endfunction()

# After `target` is built, its symbols are checked (cmake/copilot_symbols.cmake)
# with the toolchain's nm. MSVC has none - its CI jobs build the same sources
# the others check - and says so when configured.
function(glideslope_check_copilot_symbols target)
    if(NOT CMAKE_NM OR MSVC)
        message(STATUS "glideslope: no nm here, so the copilot's symbols are checked on the "
                       "other platforms' builds")
        return()
    endif()
    list(JOIN GLIDESLOPE_COPILOT_NAMES "|" _names)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} "-DNM=${CMAKE_NM}" "-DARCHIVE=$<TARGET_FILE:${target}>"
                "-DNAMES=${_names}" -P "${PROJECT_SOURCE_DIR}/cmake/copilot_symbols.cmake"
        COMMENT "Checking the copilot names nothing that drives the aircraft"
        VERBATIM)
endfunction()

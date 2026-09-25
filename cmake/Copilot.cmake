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
#   2. **What it includes is what it is shown**: after it is built, every
#      staged file is preprocessed again for the files it opened, and each
#      must be in the stage or outside the project's source and build trees -
#      whatever the spelling that opened it. Its headers are compiled each on
#      its own there too, not only as the programs that use them see them.
#      And, at configure time, its sources may hold no preprocessor
#      conditional or __has_include, so that what the programs including a
#      copilot header compile is what the stage compiled; nor anything but a
#      .cpp or .hpp, a line splice, a digraph or a trigraph.
#   3. **Its symbols are checked as it is built** (glideslope_check_copilot_
#      symbols, with `nm`, or MSVC's `dumpbin`): every name of glideslope's
#      in what it defines or calls must be the copilot's own or on
#      GLIDESLOPE_COPILOT_NAMES, and none may be JSBSim's. A function of the
#      simulation's declared by hand - even by its mangled name - is refused
#      there, and so is anything the copilot defines in another part's
#      namespace. A toolchain with neither refuses the configure.
#
# **What it does not stop**: code set on getting round it on purpose - a
# pointer to the aircraft handed in by a caller and cast, say - which review
# is for. It stops the copilot growing a way to a control by accident.
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
    # A file added or taken away is seen by the next build, not the next
    # configure - inside a project; a script has no build to tell.
    set(_depends "")
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        set(_depends CONFIGURE_DEPENDS)
    endif()
    file(GLOB_RECURSE _files LIST_DIRECTORIES false ${_depends} "${root}/src/copilot/*")
    list(SORT _files)
    set(_violations "")
    # A byte-order mark, a form feed and a vertical tab, which CMake's
    # strings cannot spell.
    string(ASCII 239 187 191 _bom)
    string(ASCII 12 _ff)
    string(ASCII 11 _vt)
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
            # What a line holds after a byte-order mark, form feeds and
            # comments before its first token, which the preprocessor skips.
            set(_code "${_line}")
            string(REPLACE "${_bom}" "" _code "${_code}")
            set(_skipped "^([ \t${_ff}${_vt}]|/\\*([^*]|\\*+[^*/])*\\*+/)+")
            if(_code MATCHES "${_skipped}")
                string(REGEX REPLACE "${_skipped}" "" _code "${_code}")
            endif()
            if(_line MATCHES "@GS_BS@$")
                set(_why "a line splice, which could join an include together")
            elseif(_line MATCHES "%:|\\?\\?=")
                set(_why "a digraph or trigraph for #, which could spell an include")
            elseif(_code MATCHES "__has_include")
                set(_why "__has_include, which would let a header see more where it is included than where it is staged")
            elseif(_code MATCHES "^#[ \t]*(if|ifdef|ifndef|elif|elifdef|elifndef|else)([^A-Za-z0-9_]|$)")
                set(_why "a preprocessor conditional, which could compile one way in the stage and another where it is included")
            elseif(_code MATCHES "^#[ \t]*pragma[ \t]+(include_alias|push_macro|pop_macro)")
                set(_why "#pragma ${CMAKE_MATCH_1}, which could open or unmake what the stage does not show")
            elseif(_code MATCHES "^#[ \t]*(include_next|import)")
                set(_why "#${CMAKE_MATCH_1}, which the copilot does not use")
            elseif(_code MATCHES "^#[ \t]*include")
                if(NOT _code MATCHES "^#[ \t]*include[ \t]*([<\"])([^>\"]*)[>\"]")
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
    file(GLOB_RECURSE _own RELATIVE "${root}/src" CONFIGURE_DEPENDS "${root}/src/copilot/*.cpp"
         "${root}/src/copilot/*.hpp")
    set(_sources "")
    foreach(_f IN LISTS _own)
        configure_file("${root}/src/${_f}" "${stage}/${_f}" COPYONLY)
        if(_f MATCHES "\\.cpp$")
            list(APPEND _sources "${stage}/${_f}")
        else()
            # **Each header compiled on its own in the stage**: the programs
            # that use one compile it seeing all of src/, so what it holds
            # must compile where it sees only the stage.
            string(MAKE_C_IDENTIFIER "${_f}" _unit)
            file(WRITE "${stage}/units/${_unit}.cpp" "#include \"${_f}\"\n")
            list(APPEND _sources "${stage}/units/${_unit}.cpp")
        endif()
    endforeach()
    set(${out} "${_sources}" PARENT_SCOPE)
endfunction()

# After `target` is built, its symbols are checked (cmake/copilot_symbols.cmake)
# with the toolchain's nm. MSVC has none - its CI jobs build the same sources
# the others check - and says so when configured.
# The tool that lists an archive's symbols with their names readable: nm -C,
# or on MSVC's toolchain dumpbin /SYMBOLS, which gives each its undecorated
# name. Sets `tool` and `style`; a toolchain with neither refuses the
# configure.
function(glideslope_symbol_lister tool style)
    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        get_filename_component(_linker_dir "${CMAKE_LINKER}" DIRECTORY)
        get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_program(GLIDESLOPE_DUMPBIN dumpbin HINTS "${_linker_dir}" "${_compiler_dir}")
        if(GLIDESLOPE_DUMPBIN)
            set(${tool} "${GLIDESLOPE_DUMPBIN}" PARENT_SCOPE)
            set(${style} dumpbin PARENT_SCOPE)
            return()
        endif()
        if(CMAKE_NM MATCHES "llvm-nm")
            set(${tool} "${CMAKE_NM}" PARENT_SCOPE)
            set(${style} nm PARENT_SCOPE)
            return()
        endif()
    elseif(CMAKE_NM)
        set(${tool} "${CMAKE_NM}" PARENT_SCOPE)
        set(${style} nm PARENT_SCOPE)
        return()
    endif()
    message(FATAL_ERROR "glideslope: no nm or dumpbin with this toolchain, so the copilot's "
                        "symbols could not be checked. See cmake/Copilot.cmake.")
endfunction()

function(glideslope_check_copilot_symbols target stage)
    glideslope_symbol_lister(_tool _style)
    list(JOIN GLIDESLOPE_COPILOT_NAMES "|" _names)
    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(_cxx_style msvc)
    else()
        set(_cxx_style gnu)
    endif()
    get_target_property(_sources ${target} SOURCES)
    list(JOIN _sources "|" _sources)
    # **The checks' settings in a file**, not on their command lines: the
    # lists hold '|', and Windows' cmd, which runs a build step there, takes
    # it for a pipe however it is quoted.
    set(_settings "${PROJECT_BINARY_DIR}/copilot-check-settings.cmake")
    file(WRITE "${_settings}"
        "set(NM [==[${_tool}]==])\n"
        "set(NM_STYLE [==[${_style}]==])\n"
        "set(NAMES [==[${_names}]==])\n"
        "set(CXX [==[${CMAKE_CXX_COMPILER}]==])\n"
        "set(CXX_STYLE [==[${_cxx_style}]==])\n"
        "set(STAGE [==[${stage}]==])\n"
        "set(SOURCES [==[${_sources}]==])\n"
        "set(SOURCE_DIR [==[${PROJECT_SOURCE_DIR}]==])\n"
        "set(BINARY_DIR [==[${PROJECT_BINARY_DIR}]==])\n")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} "-DSETTINGS=${_settings}" "-DARCHIVE=$<TARGET_FILE:${target}>"
                -P "${PROJECT_SOURCE_DIR}/cmake/copilot_symbols.cmake"
        COMMAND ${CMAKE_COMMAND} "-DSETTINGS=${_settings}"
                -P "${PROJECT_SOURCE_DIR}/cmake/copilot_includes.cmake"
        COMMENT "Checking the copilot opens and names nothing that drives the aircraft"
        VERBATIM)
endfunction()

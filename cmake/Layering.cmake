# Layering.cmake - the simulation links no presentation, checked at configure
# time rather than remembered.
#
# **`src/sim/` is the simulation, and it does not know it is being looked at.**
# No window, renderer, input device or audio stream, and nothing from gfx/, ui/,
# platform/ or a frontend. That is what lets glideslope_cli fly an aircraft with
# no window, lets a test run a flight with no video driver, and lets the server
# fly every aircraft in a session without SDL's video subsystem.
#
# Two checks, because there are two ways to break it:
#
#   glideslope_check_layering(<root>)   an #include in src/sim/ that reaches
#                                       SDL's video, input or audio headers, or
#                                       a presentation layer
#   glideslope_check_sim_links(<target>) the simulation target linking SDL,
#                                       directly or through anything it links
#
# The first runs in script mode as well, so tests/cmake/layering_includes.cmake
# can walk every forbidden include without a project around it.

# ---------------------------------------------------------------------------
# What src/sim/ may not include.
#
# SDL headers by what they are for, from include/SDL3 at release-3.4.16. The
# rest of SDL - threads, atomics, timers, logging - is not presentation and is
# not listed, though the link check below still keeps SDL out of the simulation
# target itself.
# ---------------------------------------------------------------------------
set(GLIDESLOPE_SIM_FORBIDDEN_SDL_UMBRELLA
    SDL.h SDL_main.h SDL_main_impl.h)
set(GLIDESLOPE_SIM_FORBIDDEN_SDL_VIDEO
    SDL_video.h SDL_render.h SDL_gpu.h SDL_surface.h SDL_pixels.h
    SDL_blendmode.h SDL_vulkan.h SDL_metal.h SDL_egl.h SDL_opengl.h
    SDL_opengl_glext.h SDL_opengles.h SDL_opengles2.h SDL_opengles2_gl2.h
    SDL_opengles2_gl2ext.h SDL_opengles2_gl2platform.h
    SDL_opengles2_khrplatform.h SDL_messagebox.h SDL_dialog.h SDL_tray.h
    SDL_clipboard.h SDL_camera.h SDL_system.h)
set(GLIDESLOPE_SIM_FORBIDDEN_SDL_INPUT
    SDL_events.h SDL_keyboard.h SDL_keycode.h SDL_scancode.h SDL_mouse.h
    SDL_touch.h SDL_pen.h SDL_gamepad.h SDL_joystick.h SDL_haptic.h
    SDL_sensor.h SDL_hidapi.h)
set(GLIDESLOPE_SIM_FORBIDDEN_SDL_AUDIO
    SDL_audio.h)
set(GLIDESLOPE_SIM_FORBIDDEN_LAYERS
    gfx ui platform frontend)
set(GLIDESLOPE_SIM_SOURCE_EXTENSIONS
    h hh hpp hxx inl ipp c cc cpp cxx)

function(glideslope_check_layering root)
    set(_sdl
        ${GLIDESLOPE_SIM_FORBIDDEN_SDL_UMBRELLA} ${GLIDESLOPE_SIM_FORBIDDEN_SDL_VIDEO}
        ${GLIDESLOPE_SIM_FORBIDDEN_SDL_INPUT} ${GLIDESLOPE_SIM_FORBIDDEN_SDL_AUDIO})
    list(JOIN GLIDESLOPE_SIM_FORBIDDEN_LAYERS "|" _layers)

    set(_globs "")
    foreach(_ext IN LISTS GLIDESLOPE_SIM_SOURCE_EXTENSIONS)
        list(APPEND _globs "${root}/src/sim/*.${_ext}")
    endforeach()
    file(GLOB_RECURSE _sources ${_globs})
    list(SORT _sources)

    set(_violations "")
    foreach(_f IN LISTS _sources)
        file(RELATIVE_PATH _rel "${root}" "${_f}")

        # Read line by line with line numbers. C++ is full of the characters a
        # CMake list treats specially - ';' separates, '[' and ']' stop it
        # separating - so they are swapped out before the text becomes a list,
        # and the reported line is put back the way it was written.
        file(READ "${_f}" _text)
        string(REPLACE ";" "@GS_SEMI@" _text "${_text}")
        string(REPLACE "[" "@GS_OPEN@" _text "${_text}")
        string(REPLACE "]" "@GS_CLOSE@" _text "${_text}")
        string(REPLACE "\r" "" _text "${_text}")
        string(REPLACE "\n" ";" _lines "${_text}")

        set(_n 0)
        foreach(_line IN LISTS _lines)
            math(EXPR _n "${_n} + 1")
            if(NOT _line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
                continue()
            endif()
            set(_path "${CMAKE_MATCH_1}")
            # "../gfx/x.hpp" reaches the same layer as "gfx/x.hpp".
            string(REGEX REPLACE "^(\\.\\.?/)+" "" _path "${_path}")

            set(_why "")
            if(_path MATCHES "^(${_layers})/")
                set(_why "reaches into ${CMAKE_MATCH_1}/")
            elseif(_path MATCHES "^(SDL3/)?(SDL[A-Za-z0-9_]*\\.h)$")
                list(FIND _sdl "${CMAKE_MATCH_2}" _at)
                if(NOT _at EQUAL -1)
                    set(_why "includes SDL presentation (${CMAKE_MATCH_2})")
                endif()
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
            "src/sim/ has broken the presentation boundary:\n"
            "${_violations}"
            "The simulation may not include SDL's video, input or audio headers, "
            "nor gfx/, ui/, platform/ or a frontend. See cmake/Layering.cmake.")
    endif()
    list(LENGTH _sources _count)
    message(STATUS "glideslope: src/sim/ includes no presentation (${_count} files)")
endfunction()

# ---------------------------------------------------------------------------
# The simulation target links no SDL.
#
# SDL is one library: linking any of it links the video, input and audio code
# with it, and glideslope_cli would stop being proof of anything. Walked through
# every target the simulation links, so SDL arriving second-hand is caught too.
# Call it deferred, once every target_link_libraries has been seen.
# ---------------------------------------------------------------------------
function(_glideslope_sdl_in_links target chain out)
    set(_found "")
    foreach(_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_libs ${target} ${_prop})
        if(NOT _libs)
            continue()
        endif()
        foreach(_lib IN LISTS _libs)
            # A static library records its dependencies as $<LINK_ONLY:...>.
            string(REGEX REPLACE "^\\$<LINK_ONLY:(.*)>$" "\\1" _lib "${_lib}")
            set(_name "${_lib}")
            if(TARGET "${_lib}")
                get_target_property(_aliased "${_lib}" ALIASED_TARGET)
                if(_aliased)
                    set(_name "${_lib} (${_aliased})")
                endif()
            endif()
            get_filename_component(_leaf "${_lib}" NAME)
            if(_name MATCHES "(^|[ (])SDL3" OR _leaf MATCHES "^(lib)?SDL3")
                list(APPEND _found "${chain} -> ${_lib}")
            elseif(TARGET "${_lib}")
                list(FIND _seen "${_lib}" _already)
                if(_already EQUAL -1)
                    list(APPEND _seen "${_lib}")
                    _glideslope_sdl_in_links("${_lib}" "${chain} -> ${_lib}" _deeper)
                    list(APPEND _found ${_deeper})
                endif()
            endif()
        endforeach()
    endforeach()
    set(${out} "${_found}" PARENT_SCOPE)
    set(_seen "${_seen}" PARENT_SCOPE)
endfunction()

function(glideslope_check_sim_links target)
    set(_seen "")
    _glideslope_sdl_in_links(${target} ${target} _found)
    # A static library lists a dependency twice - once to link, once as
    # $<LINK_ONLY:...> for whoever links it - so the same route can be found
    # twice.
    list(REMOVE_DUPLICATES _found)
    if(_found)
        list(JOIN _found "\n  " _report)
        message(FATAL_ERROR
            "${target} links SDL, which brings video, input and audio with it:\n"
            "  ${_report}\n"
            "The simulation links no presentation. See cmake/Layering.cmake.")
    endif()
    message(STATUS "glideslope: ${target} links no SDL")
endfunction()

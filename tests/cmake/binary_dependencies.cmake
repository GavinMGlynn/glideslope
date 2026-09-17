# binary_dependencies.cmake - read what a built program actually links, from the
# program itself, and refuse anything presentational.
#
#   cmake -DPROGRAM=<path> -DSYSTEM=<Linux|Darwin|Windows> -P binary_dependencies.cmake
#
# The configure-time checks look at what the build *says* it links. This looks
# at what the linker *did*: the dynamic dependencies recorded in the binary, read
# with readelf on Linux, otool on macOS and dumpbin on Windows. A presentation
# library that arrived some other way - a flag, a static library's own
# dependencies - shows up here.
#
# The lists name each platform's windowing, graphics, input and audio libraries,
# and SDL itself.

set(_linux_presentation
    SDL3 libX11 libXext libXrandr libXi libXcursor libXfixes libXss libXtst
    libxkbcommon libwayland libdecor libGL libEGL libGLX libOpenGL libvulkan
    libdrm libgbm libasound libpulse libpipewire libjack libsndio libudev libusb)
set(_darwin_presentation
    SDL3 Cocoa AppKit Metal MetalKit QuartzCore CoreVideo CoreAudio AudioToolbox
    AVFoundation CoreHaptics GameController ForceFeedback Carbon IOKit
    UniformTypeIdentifiers)
set(_windows_presentation
    SDL3 user32 gdi32 d3d11 d3d12 dxgi d3dcompiler vulkan-1 opengl32 winmm
    dinput8 xinput1_4 imm32 hid setupapi dwmapi uxtheme shcore)

if(SYSTEM STREQUAL "Linux")
    set(_tool readelf -d)
    set(_line_regex "\\(NEEDED\\)[^[]*\\[([^]]+)\\]")
    set(_presentation ${_linux_presentation})
elseif(SYSTEM STREQUAL "Darwin")
    set(_tool otool -L)
    set(_line_regex "^[ \t]+([^ \t]+) \\(compatibility")
    set(_presentation ${_darwin_presentation})
elseif(SYSTEM STREQUAL "Windows")
    set(_tool dumpbin /dependents)
    set(_line_regex "^[ \t]+([A-Za-z0-9_.-]+\\.[dD][lL][lL])[ \t]*$")
    set(_presentation ${_windows_presentation})
else()
    message(FATAL_ERROR "binary_dependencies.cmake: no reader for ${SYSTEM}")
endif()

execute_process(COMMAND ${_tool} "${PROGRAM}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "could not read ${PROGRAM}'s dependencies with '${_tool}' "
        "(exit ${_rc}):\n${_out}${_err}")
endif()

string(REPLACE "\r" "" _out "${_out}")
string(REPLACE "\n" ";" _lines "${_out}")
set(_deps "")
foreach(_line IN LISTS _lines)
    if(_line MATCHES "${_line_regex}")
        list(APPEND _deps "${CMAKE_MATCH_1}")
    endif()
endforeach()

# A reader that found nothing is a reader that stopped working, not a clean
# program: every program here links at least the C runtime.
list(LENGTH _deps _n_deps)
if(_n_deps EQUAL 0)
    message(FATAL_ERROR
        "found no dependencies in ${PROGRAM} - the '${_tool}' output was not "
        "understood:\n${_out}")
endif()

set(_bad "")
foreach(_dep IN LISTS _deps)
    get_filename_component(_leaf "${_dep}" NAME)
    string(TOLOWER "${_leaf}" _leaf_lower)
    foreach(_lib IN LISTS _presentation)
        string(TOLOWER "${_lib}" _lib_lower)
        string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1" _lib_re "${_lib_lower}")
        if(_leaf_lower MATCHES "^${_lib_re}([.-]|$)")
            list(APPEND _bad "${_dep} (${_lib})")
        endif()
    endforeach()
endforeach()

list(LENGTH _presentation _n_pres)
list(JOIN _deps ", " _dep_list)
message(STATUS "${PROGRAM}: ${_n_deps} dependencies checked against "
               "${_n_pres} presentation libraries: ${_dep_list}")
if(_bad)
    list(JOIN _bad "\n  " _report)
    message(FATAL_ERROR "${PROGRAM} links presentation:\n  ${_report}")
endif()

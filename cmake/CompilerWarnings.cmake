# CompilerWarnings.cmake - the warning set, applied to first-party targets only.
#
# Vendored code in ext/ is deliberately untouched: warnings in code this project
# cannot fix are warnings people learn to scroll past. When a dependency's
# headers arrive, they are included as SYSTEM so that their warnings stay theirs.

# Warnings are errors, in **every** build type - not debug and CI alone. A
# warning that is only fatal somewhere else is a warning that gets committed
# here and discovered by someone else's build.
#   cmake --preset linux-debug -DGLIDESLOPE_WERROR=OFF
# The option lives here rather than in CMakeLists.txt so that anything including
# this file - tests/warnings/ does - gets the same default the project does.
option(GLIDESLOPE_WERROR "Treat compiler warnings as errors" ON)

function(glideslope_warnings target)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # MSVC, and clang-cl, which takes MSVC's flags.
        #
        # /utf-8 because the sources are UTF-8 and comments here use it; without
        # it MSVC reads them in the machine's code page and says so with C4819,
        # which /WX makes fatal on a machine set to a different locale.
        target_compile_options(${target} PRIVATE /W4 /utf-8)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # clang-cl's /W4 is -Wall -Wextra and no more, so the conversions
            # are named explicitly to keep it in step with Clang elsewhere.
            target_compile_options(${target} PRIVATE
                -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast)
        endif()
        if(GLIDESLOPE_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        # -Wconversion and -Wsign-conversion are the pair that matter most. The
        # world is double precision and the renderer is float relative to the
        # camera, so the floating origin is one long double-to-float boundary,
        # and a silent narrowing there is jitter nobody can trace.
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow
            -Wconversion -Wsign-conversion -Wdouble-promotion
            -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual
            -Wimplicit-fallthrough)
        if(GLIDESLOPE_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

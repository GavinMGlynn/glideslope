# Sanitizers.cmake - address and undefined-behaviour sanitizers, for the debug
# presets on Linux and macOS.
#
# **`-fno-sanitize-recover=all` is the line that makes this worth having.**
# Without it the undefined-behaviour sanitizer prints its report and lets the
# program carry on, so a test that hits undefined behaviour still exits zero and
# ctest shows it green. With it, the first report ends the process and the test
# fails where the problem is.
#
# windows-debug does not sanitize. MSVC has no undefined-behaviour sanitizer,
# and its address sanitizer needs its runtime DLLs found at run time, which is
# setup nothing here has paid for yet.

function(glideslope_sanitize target)
    if(GLIDESLOPE_ASAN AND NOT MSVC)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
endfunction()

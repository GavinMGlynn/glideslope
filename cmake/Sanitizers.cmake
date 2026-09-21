# Sanitizers.cmake - address and undefined-behaviour sanitizers, for the debug
# presets on Linux and macOS.
#
# **`-fno-sanitize-recover=all` is the line that makes this worth having.**
# Without it the undefined-behaviour sanitizer prints its report and lets the
# program carry on, so a test that hits undefined behaviour still exits zero and
# ctest shows it green. With it, the first report ends the process and the test
# fails where the problem is.
#
# **Sanitized code is built at -O1**, as the address sanitizer's documentation
# advises. Unoptimised, it ran the tests 25 to 40 times slower than a release
# build - the F-22's supercruise flight took 875 s where release takes 24 -
# and Ubuntu's debug job took two hours. -O1 keeps every check; what it costs
# is a debugger that sometimes finds a variable optimised away. With it, GCC 14
# warns that std::regex's own std::function "may be used uninitialized" - in
# libstdc++, where it is not, and only when sanitized code is optimised - so
# that warning is off in sanitized GCC builds; the release builds, optimised
# and unsanitized, still give it.
#
# windows-debug does not sanitize. MSVC has no undefined-behaviour sanitizer,
# and its address sanitizer needs its runtime DLLs found at run time, which is
# setup nothing here has paid for yet.

# **One check turned off, for vendored code only.**
#
# Cesium Native reads quantized-mesh terrain where it lies, with
#
#     return *reinterpret_cast<const T*>(data.data() + offset);
#
# at an arbitrary byte offset - CesiumQuantizedMeshTerrain's
# QuantizedMeshLoader.cpp, readValue. Quantized mesh has unaligned fields by
# design, so that is undefined behaviour on every terrain tile Cesium ion
# serves, and -fno-sanitize-recover=all ends the program at the first one. It
# is their code, not this project's, and the fix is theirs to make - a memcpy;
# an issue is drafted for it in docs/cesium-issues.md. Until they do, the
# alignment check alone is off for their targets, and every other check, and
# all first-party code, is untouched.
#
# Take this away when Cesium Native is bumped past a release that fixes it: if
# it is fixed, nothing changes; if it is not, the sanitized build says so
# again.
function(glideslope_allow_misaligned target)
    if(GLIDESLOPE_ASAN AND NOT MSVC)
        target_compile_options(${target} PRIVATE -fno-sanitize=alignment)
    endif()
endfunction()

function(glideslope_sanitize target)
    if(GLIDESLOPE_ASAN AND NOT MSVC)
        target_compile_options(${target} PRIVATE
            -O1 -fsanitize=address,undefined -fno-sanitize-recover=all
            -fno-omit-frame-pointer
            $<$<OR:$<COMPILE_LANG_AND_ID:CXX,GNU>,$<COMPILE_LANG_AND_ID:C,GNU>>:-Wno-maybe-uninitialized>)
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
endfunction()

# ci_clang_cl_show_includes.cmake - clang-cl reports its headers as cl.exe does,
# so that ccache can cache its compiles. CI's Windows builds only, through
# CMAKE_PROJECT_INCLUDE on the configure's command line (.github/workflows/
# ci.yml); a developer's build never reads it.
#
# **Why**: CMake 4 has clang-cl write a gcc-style dependency file, through
# `-clang:-MD -clang:-MT... -clang:-MF...` (Platform/Windows-Clang.cmake), and
# ccache refuses every `/clang:` option as unsupported: every one of the
# windows-clang build's 792 compiles was uncacheable. cl.exe's /showIncludes,
# which clang-cl also takes and ccache understands, is what MSVC's rules use.
#
# Included after every project(), since each one can enable a language anew.

foreach(_lang C CXX)
    if(CMAKE_${_lang}_COMPILER_ID STREQUAL "Clang"
       AND CMAKE_${_lang}_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(CMAKE_DEPFILE_FLAGS_${_lang} "/showIncludes")
        set(CMAKE_${_lang}_DEPFILE_FORMAT msvc)
    endif()
endforeach()
unset(_lang)

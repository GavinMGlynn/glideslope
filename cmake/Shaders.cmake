# Shaders.cmake - the shader compiler, and a rule to compile shaders with it.
#
# **One source per shader, three backends.** SDL_GPU takes a shader in the form
# its backend runs: SPIR-V for Vulkan, DXBC or DXIL for Direct3D 12, MSL for
# Metal. Shaders are written once, in GLSL, and glideslope_shaderc - a build
# tool, built here and never shipped - turns each into all three:
#
#   - glslang compiles the GLSL to SPIR-V;
#   - SPIRV-Cross translates the SPIR-V to MSL and to HLSL, placing every
#     resource where SDL_GPU's documented layout says it must be;
#   - on Windows, the system's D3DCompile compiles that HLSL to DXBC. Direct3D 12
#     exists only on Windows, so nowhere else needs it.
#
# The result is a C++ source per shader, compiled into the program, holding the
# three forms and the resource counts SDL_GPU asks for, read from the SPIR-V
# rather than written by hand. A shader that breaks SDL_GPU's layout fails the
# build, not the first frame.
#
# DXC, the compiler for DXIL, is not used: it is LLVM-sized, and SDL_GPU accepts
# Shader Model 5.1 DXBC on Direct3D 12.
#
# glslang and SPIRV-Cross are pinned submodules. Neither is linked into
# anything shipped, so no package carries their licences. Neither is sanitized,
# and nor is the compiler: it is a build step, and a leak report from glslang
# would fail the build without saying anything about glideslope.

foreach(_dep glslang spirv-cross)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/${_dep}/CMakeLists.txt")
        message(FATAL_ERROR
            "ext/${_dep} is empty - the submodules are not checked out.\n"
            "  git submodule update --init --depth 1 ext/${_dep}")
    endif()
endforeach()

block()
    # Their option() calls honour these rather than resetting them.
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    # Only the libraries: no optimizer (it needs SPIRV-Tools), no HLSL front end
    # (the source language is GLSL), no binaries, tests, install rules or
    # precompiled headers.
    set(ENABLE_OPT OFF)
    set(ENABLE_HLSL OFF)
    set(ENABLE_GLSLANG_BINARIES OFF)
    set(ENABLE_PCH OFF)
    set(BUILD_EXTERNAL OFF)
    set(GLSLANG_TESTS OFF)
    set(GLSLANG_ENABLE_INSTALL OFF)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ext/glslang"
                     "${CMAKE_CURRENT_BINARY_DIR}/ext/glslang" EXCLUDE_FROM_ALL)

    # The C++ API for the three translators it is asked for, and nothing else.
    set(SPIRV_CROSS_CLI OFF)
    set(SPIRV_CROSS_ENABLE_TESTS OFF)
    set(SPIRV_CROSS_ENABLE_CPP OFF)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF)
    set(SPIRV_CROSS_ENABLE_C_API OFF)
    set(SPIRV_CROSS_ENABLE_UTIL OFF)
    set(SPIRV_CROSS_SKIP_INSTALL ON)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ext/spirv-cross"
                     "${CMAKE_CURRENT_BINARY_DIR}/ext/spirv-cross" EXCLUDE_FROM_ALL)
endblock()

foreach(_t glslang SPIRV glslang-default-resource-limits
           spirv-cross-core spirv-cross-glsl spirv-cross-msl spirv-cross-hlsl)
    set_target_properties(${_t} PROPERTIES SYSTEM TRUE)
endforeach()

add_executable(glideslope_shaderc tools/shaderc/main.cpp)
glideslope_warnings(glideslope_shaderc)
target_link_libraries(glideslope_shaderc PRIVATE
    glslang::glslang glslang::glslang-default-resource-limits
    spirv-cross-msl spirv-cross-hlsl)
if(WIN32)
    target_compile_definitions(glideslope_shaderc PRIVATE GLIDESLOPE_SHADERC_DXBC=1)
    target_link_libraries(glideslope_shaderc PRIVATE d3dcompiler)
endif()

# glideslope_shaders(<target> <shader>...)
#
# Compiles each shader - a path relative to the current source directory, named
# <name>.vert or <name>.frag - into <target>, and generates shaders.hpp, which
# declares every one as `glideslope::gfx::shaders::<name>_<stage>`, a
# gfx::ShaderCode. <target> gets the directory shaders.hpp is in on its include
# path.
function(glideslope_shaders target)
    set(_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders/${target}")
    set(_declarations "")
    set(_sources "")
    foreach(_shader IN LISTS ARGN)
        get_filename_component(_name "${_shader}" NAME_WE)
        get_filename_component(_ext "${_shader}" LAST_EXT)
        if(_ext STREQUAL ".vert")
            set(_stage vertex)
        elseif(_ext STREQUAL ".frag")
            set(_stage fragment)
        else()
            message(FATAL_ERROR "${_shader}: a shader is named .vert or .frag")
        endif()
        set(_symbol "${_name}_${_stage}")
        set(_out "${_dir}/${_symbol}.cpp")
        add_custom_command(
            OUTPUT "${_out}"
            COMMAND glideslope_shaderc --stage ${_stage} --name ${_symbol}
                    --in "${CMAKE_CURRENT_SOURCE_DIR}/${_shader}" --out "${_out}"
            DEPENDS glideslope_shaderc "${CMAKE_CURRENT_SOURCE_DIR}/${_shader}"
            COMMENT "Compiling shader ${_shader}"
            VERBATIM)
        list(APPEND _sources "${_out}")
        string(APPEND _declarations "extern const ShaderCode ${_symbol};\n")
    endforeach()
    file(CONFIGURE OUTPUT "${_dir}/shaders.hpp" CONTENT
"#pragma once\n\n// Generated by glideslope_shaders() in cmake/Shaders.cmake.\n\n#include \"gfx/shader_code.hpp\"\n\nnamespace glideslope::gfx::shaders {\n\n${_declarations}\n} // namespace glideslope::gfx::shaders\n")
    target_sources(${target} PRIVATE ${_sources})
    target_include_directories(${target} PRIVATE "${_dir}")
endfunction()

message(STATUS "glideslope: shaders compiled by glslang and SPIRV-Cross from ext/")

#pragma once

// One shader in every form SDL_GPU's backends take, made at build time by
// glideslope_shaderc from a GLSL source - see cmake/Shaders.cmake.

#include <cstdint>
#include <span>
#include <string_view>

namespace glideslope::gfx {

enum class ShaderStage { vertex, fragment };

struct ShaderCode {
    std::string_view name;
    ShaderStage stage = ShaderStage::vertex;

    std::span<const std::uint8_t> spirv; // Vulkan; entry point "main"
    std::string_view msl;                // Metal source
    std::string_view msl_entry_point;    // SPIRV-Cross renames "main"
    std::span<const std::uint8_t> dxbc;  // Direct3D 12; empty off Windows

    // What SDL_GPUShaderCreateInfo asks for, read from the SPIR-V.
    std::uint32_t samplers = 0;
    std::uint32_t storage_textures = 0;
    std::uint32_t storage_buffers = 0;
    std::uint32_t uniform_buffers = 0;
};

} // namespace glideslope::gfx

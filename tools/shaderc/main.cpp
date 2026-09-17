// glideslope_shaderc - compiles one GLSL shader into every form SDL_GPU takes.
//
//   glideslope_shaderc --stage vertex|fragment --name NAME --in FILE --out FILE.cpp
//
// Writes a C++ source defining `glideslope::gfx::shaders::NAME`, a
// gfx::ShaderCode holding the shader as SPIR-V (glslang), MSL (SPIRV-Cross),
// and on Windows DXBC (SPIRV-Cross to HLSL, then D3DCompile), with the resource
// counts SDL_GPU asks for read from the SPIR-V. See cmake/Shaders.cmake.
//
// **SDL_GPU's resource layout is checked, not trusted.** SDL_CreateGPUShader
// documents where each backend expects each resource; a shader that puts one
// elsewhere compiles and then draws nothing, or the wrong thing. The layout is
// written once, here, against SDL's documentation, and a shader that breaks it
// fails the build with the resource named.
//
// The GLSL is written to SDL's SPIR-V layout:
//
//   vertex:   set 0 - sampled textures, then storage textures, then storage
//                     buffers, bound 0, 1, 2... in that order
//             set 1 - uniform buffers, bound 0, 1, 2...
//   fragment: set 2 and set 3, the same
//
// and every other backend's placement follows from the set and binding.

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>

#ifdef GLIDESLOPE_SHADERC_DXBC
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3dcompiler.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Arguments {
    bool vertex = true;
    std::string name;
    std::string in;
    std::string out;
};

[[noreturn]] void fail(const std::string& what) {
    throw std::runtime_error(what);
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        fail("cannot read " + path);
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::vector<std::uint32_t> compile_spirv(const Arguments& a,
                                         const std::string& source) {
    const EShLanguage stage = a.vertex ? EShLangVertex : EShLangFragment;
    std::vector<std::uint32_t> spirv;
    glslang::InitializeProcess();
    {
        glslang::TShader shader(stage);
        const char* text = source.c_str();
        const char* name = a.in.c_str();
        shader.setStringsWithLengthsAndNames(&text, nullptr, &name, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan,
                           100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
        const auto messages =
            static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
        if (!shader.parse(GetDefaultResources(), 450, false, messages)) {
            const std::string log = shader.getInfoLog();
            glslang::FinalizeProcess();
            fail(log);
        }
        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(messages)) {
            const std::string log = program.getInfoLog();
            glslang::FinalizeProcess();
            fail(log);
        }
        std::vector<unsigned int> words;
        spv::SpvBuildLogger logger;
        glslang::SpvOptions options;
        glslang::GlslangToSpv(*program.getIntermediate(stage), words, &logger,
                              &options);
        const std::string messages_out = logger.getAllMessages();
        if (!messages_out.empty()) {
            std::fputs(messages_out.c_str(), stderr);
        }
        spirv.assign(words.begin(), words.end());
    }
    glslang::FinalizeProcess();
    return spirv;
}

// Where SDL_GPU wants each resource, per backend, derived from its SPIR-V set
// and binding; see the comment at the top.
struct Layout {
    std::uint32_t samplers = 0;
    std::uint32_t storage_textures = 0;
    std::uint32_t storage_buffers = 0;
    std::uint32_t uniform_buffers = 0;

    struct Resource {
        enum class Kind {
            sampled_texture,
            storage_texture,
            storage_buffer,
            uniform_buffer
        };
        Kind kind;
        std::string name;
        std::uint32_t set;
        std::uint32_t binding;
    };
    std::vector<Resource> resources;
};

Layout read_layout(const Arguments& a, const std::vector<std::uint32_t>& spirv) {
    using Kind = Layout::Resource::Kind;
    const spirv_cross::Compiler compiler(spirv);
    const spirv_cross::ShaderResources r = compiler.get_shader_resources();

    const auto refuse = [&](const auto& list, const char* what) {
        if (!list.empty()) {
            fail(a.in + ": " + list.front().name + " is a " + what +
                 ", which SDL_GPU's layout has no place for");
        }
    };
    refuse(r.separate_images, "separate texture (use a combined sampler2D)");
    refuse(r.separate_samplers, "separate sampler (use a combined sampler2D)");
    refuse(r.push_constant_buffers, "push constant block (use a uniform buffer)");
    refuse(r.subpass_inputs, "subpass input");
    refuse(r.atomic_counters, "atomic counter");
    refuse(r.acceleration_structures, "acceleration structure");

    const std::uint32_t resource_set = a.vertex ? 0 : 2;
    const std::uint32_t uniform_set = a.vertex ? 1 : 3;
    const char* stage = a.vertex ? "a vertex" : "a fragment";

    Layout layout;
    const auto take = [&](const auto& list, Kind kind, std::uint32_t set) {
        std::vector<Layout::Resource> found;
        for (const auto& res : list) {
            found.push_back(
                {kind, res.name,
                 compiler.get_decoration(res.id, spv::DecorationDescriptorSet),
                 compiler.get_decoration(res.id, spv::DecorationBinding)});
            if (found.back().set != set) {
                fail(a.in + ": " + res.name + " is in set " +
                     std::to_string(found.back().set) + "; in " + stage +
                     " shader SDL_GPU wants it in set " + std::to_string(set));
            }
        }
        std::sort(found.begin(), found.end(),
                  [](const auto& x, const auto& y) { return x.binding < y.binding; });
        return found;
    };
    const auto sampled = take(r.sampled_images, Kind::sampled_texture, resource_set);
    const auto storage_tex =
        take(r.storage_images, Kind::storage_texture, resource_set);
    const auto storage_buf =
        take(r.storage_buffers, Kind::storage_buffer, resource_set);
    const auto uniforms = take(r.uniform_buffers, Kind::uniform_buffer, uniform_set);

    // Bindings run 0, 1, 2... through sampled textures, storage textures and
    // storage buffers in that order, and separately through uniform buffers.
    std::uint32_t next = 0;
    for (const auto* group : {&sampled, &storage_tex, &storage_buf}) {
        for (const auto& res : *group) {
            if (res.binding != next) {
                fail(a.in + ": " + res.name + " is bound at " +
                     std::to_string(res.binding) + "; SDL_GPU's layout puts it at " +
                     std::to_string(next) +
                     " (sampled textures, then storage textures, then storage buffers, "
                     "from 0)");
            }
            ++next;
            layout.resources.push_back(res);
        }
    }
    next = 0;
    for (const auto& res : uniforms) {
        if (res.binding != next) {
            fail(a.in + ": uniform buffer " + res.name + " is bound at " +
                 std::to_string(res.binding) + "; SDL_GPU's layout puts it at " +
                 std::to_string(next));
        }
        ++next;
        layout.resources.push_back(res);
    }
    layout.samplers = static_cast<std::uint32_t>(sampled.size());
    layout.storage_textures = static_cast<std::uint32_t>(storage_tex.size());
    layout.storage_buffers = static_cast<std::uint32_t>(storage_buf.size());
    layout.uniform_buffers = static_cast<std::uint32_t>(uniforms.size());
    return layout;
}

spv::ExecutionModel model(const Arguments& a) {
    return a.vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
}

// MSL: [[texture]] sampled textures then storage textures, [[sampler]] matching
// the sampled textures, [[buffer]] uniform buffers then storage buffers.
std::string translate_msl(const Arguments& a, const std::vector<std::uint32_t>& spirv,
                          const Layout& layout, std::string& entry_point) {
    using Kind = Layout::Resource::Kind;
    spirv_cross::CompilerMSL msl(spirv);
    spirv_cross::CompilerMSL::Options options;
    options.platform = spirv_cross::CompilerMSL::Options::macOS;
    msl.set_msl_options(options);
    for (const auto& res : layout.resources) {
        spirv_cross::MSLResourceBinding b;
        b.stage = model(a);
        b.desc_set = res.set;
        b.binding = res.binding;
        b.count = 1;
        switch (res.kind) {
        case Kind::sampled_texture:
        case Kind::storage_texture:
            b.msl_texture = res.binding;
            b.msl_sampler = res.binding;
            break;
        case Kind::storage_buffer:
            b.msl_buffer = layout.uniform_buffers +
                           (res.binding - layout.samplers - layout.storage_textures);
            break;
        case Kind::uniform_buffer: b.msl_buffer = res.binding; break;
        }
        msl.add_msl_resource_binding(b);
    }
    std::string source = msl.compile();
    entry_point = msl.get_cleansed_entry_point_name("main", model(a));
    return source;
}

// HLSL: t, s and u registers in the resource set's space, b registers in the
// uniform set's, each at the SPIR-V binding.
std::string translate_hlsl(const Arguments& a, const std::vector<std::uint32_t>& spirv,
                           const Layout& layout) {
    spirv_cross::CompilerHLSL hlsl(spirv);
    spirv_cross::CompilerHLSL::Options options;
    options.shader_model = 51;
    options.nonwritable_uav_texture_as_srv = true;
    hlsl.set_hlsl_options(options);
    for (const auto& res : layout.resources) {
        spirv_cross::HLSLResourceBinding b;
        b.stage = model(a);
        b.desc_set = res.set;
        b.binding = res.binding;
        for (auto* reg : {&b.cbv, &b.uav, &b.srv, &b.sampler}) {
            reg->register_space = res.set;
            reg->register_binding = res.binding;
        }
        hlsl.add_hlsl_resource_binding(b);
    }
    return hlsl.compile();
}

#ifdef GLIDESLOPE_SHADERC_DXBC
std::vector<std::uint8_t> compile_dxbc(const Arguments& a, const std::string& hlsl) {
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT result =
        D3DCompile(hlsl.data(), hlsl.size(), a.in.c_str(), nullptr, nullptr, "main",
                   a.vertex ? "vs_5_1" : "ps_5_1",
                   D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                   &code, &errors);
    std::string log;
    if (errors != nullptr) {
        log.assign(static_cast<const char*>(errors->GetBufferPointer()),
                   errors->GetBufferSize());
        errors->Release();
    }
    if (FAILED(result) || code == nullptr) {
        if (code != nullptr) {
            code->Release();
        }
        fail(a.in + ": D3DCompile refused the HLSL SPIRV-Cross made of it:\n" + log +
             "\n" + hlsl);
    }
    const auto* bytes = static_cast<const std::uint8_t*>(code->GetBufferPointer());
    std::vector<std::uint8_t> dxbc(bytes, bytes + code->GetBufferSize());
    code->Release();
    return dxbc;
}
#endif

void write_bytes(std::ostream& out, const char* type, const char* name,
                 const std::vector<std::uint8_t>& bytes) {
    out << "constinit const " << type << " " << name << "[] = {";
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out << (i % 16 == 0 ? "\n    " : " ") << static_cast<unsigned>(bytes[i]) << ",";
    }
    out << "\n};\n\n";
}

void write_source(const Arguments& a, const std::vector<std::uint32_t>& spirv,
                  const std::string& msl, const std::string& msl_entry_point,
                  const std::vector<std::uint8_t>& dxbc, const Layout& layout) {
    std::vector<std::uint8_t> spirv_bytes;
    for (const std::uint32_t word : spirv) {
        for (int shift = 0; shift < 32; shift += 8) {
            spirv_bytes.push_back(static_cast<std::uint8_t>(word >> shift));
        }
    }
    // MSL made from GLSL is ASCII; held as integers so it needs no escaping and
    // meets no compiler's string literal limit.
    std::vector<std::uint8_t> msl_bytes(msl.begin(), msl.end());
    if (std::any_of(msl_bytes.begin(), msl_bytes.end(),
                    [](std::uint8_t c) { return c >= 128; })) {
        fail(a.in + ": the MSL SPIRV-Cross made is not ASCII");
    }

    std::ostringstream out;
    out << "// Generated by glideslope_shaderc from " << a.in << ". Do not edit.\n\n"
        << "#include \"shaders.hpp\"\n\n"
        << "namespace glideslope::gfx::shaders {\n\nnamespace {\n\n";
    write_bytes(out, "std::uint8_t", "spirv", spirv_bytes);
    write_bytes(out, "char", "msl", msl_bytes);
    if (!dxbc.empty()) {
        write_bytes(out, "std::uint8_t", "dxbc", dxbc);
    }
    out << "} // namespace\n\n"
        << "constinit const ShaderCode " << a.name << "{\n"
        << "    \"" << a.name << "\",\n"
        << "    ShaderStage::" << (a.vertex ? "vertex" : "fragment") << ",\n"
        << "    spirv,\n"
        << "    std::string_view(msl, sizeof msl),\n"
        << "    \"" << msl_entry_point << "\",\n"
        << "    " << (dxbc.empty() ? "{}" : "dxbc") << ",\n"
        << "    " << layout.samplers << ",\n"
        << "    " << layout.storage_textures << ",\n"
        << "    " << layout.storage_buffers << ",\n"
        << "    " << layout.uniform_buffers << ",\n"
        << "};\n\n"
        << "} // namespace glideslope::gfx::shaders\n";

    // Written only when it changes, so a rebuilt compiler does not rebuild
    // everything that includes the shaders.
    const std::string text = out.str();
    {
        std::ifstream existing(a.out, std::ios::binary);
        if (existing) {
            std::ostringstream old;
            old << existing.rdbuf();
            if (old.str() == text) {
                return;
            }
        }
    }
    std::ofstream file(a.out, std::ios::binary | std::ios::trunc);
    file << text;
    if (!file) {
        fail("cannot write " + a.out);
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    Arguments a;
    bool stage_given = false;
    for (std::size_t i = 0; i + 1 < args.size(); i += 2) {
        const std::string value(args[i + 1]);
        if (args[i] == "--stage" && (value == "vertex" || value == "fragment")) {
            a.vertex = value == "vertex";
            stage_given = true;
        } else if (args[i] == "--name") {
            a.name = value;
        } else if (args[i] == "--in") {
            a.in = value;
        } else if (args[i] == "--out") {
            a.out = value;
        } else {
            a.name.clear();
            break;
        }
    }
    if (args.size() % 2 != 0 || !stage_given || a.name.empty() || a.in.empty() ||
        a.out.empty()) {
        std::fputs("usage: glideslope_shaderc --stage vertex|fragment --name NAME "
                   "--in FILE --out FILE.cpp\n",
                   stderr);
        return 2;
    }

    try {
        const std::vector<std::uint32_t> spirv = compile_spirv(a, read_file(a.in));
        const Layout layout = read_layout(a, spirv);
        std::string msl_entry_point;
        const std::string msl = translate_msl(a, spirv, layout, msl_entry_point);
        const std::string hlsl = translate_hlsl(a, spirv, layout);
        std::vector<std::uint8_t> dxbc;
#ifdef GLIDESLOPE_SHADERC_DXBC
        dxbc = compile_dxbc(a, hlsl);
#endif
        write_source(a, spirv, msl, msl_entry_point, dxbc, layout);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glideslope_shaderc: %s\n", e.what());
        return 1;
    }
    return 0;
}

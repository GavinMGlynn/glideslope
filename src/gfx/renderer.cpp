#include "gfx/renderer.hpp"

#include "gfx/shader_code.hpp"
#include "shaders.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace glideslope::gfx {

namespace {

constexpr SDL_GPUTextureFormat colour_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
// Float, because reversed depth only pays for itself in a float buffer.
constexpr SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

std::runtime_error sdl_error(const std::string& what) {
    return std::runtime_error(what + ": " + SDL_GetError());
}

// The shader in the form this device's backend takes.
SDL_GPUShader* make_shader(SDL_GPUDevice* device, const ShaderCode& code) {
    SDL_GPUShaderCreateInfo info{};
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    const std::string entry_point(code.msl_entry_point);
    if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = code.spirv.data();
        info.code_size = code.spirv.size();
        info.entrypoint = "main";
    } else if ((formats & SDL_GPU_SHADERFORMAT_DXBC) != 0 && !code.dxbc.empty()) {
        info.format = SDL_GPU_SHADERFORMAT_DXBC;
        info.code = code.dxbc.data();
        info.code_size = code.dxbc.size();
        info.entrypoint = "main";
    } else if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        // Uint8 is how SDL takes shader code of every kind, text included.
        info.code = reinterpret_cast<const Uint8*>(code.msl.data());
        info.code_size = code.msl.size();
        info.entrypoint = entry_point.c_str();
    } else {
        throw std::runtime_error("shader " + std::string(code.name) +
                                 " has no form this GPU driver takes");
    }
    info.stage = code.stage == ShaderStage::vertex ? SDL_GPU_SHADERSTAGE_VERTEX
                                                   : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = code.samplers;
    info.num_storage_textures = code.storage_textures;
    info.num_storage_buffers = code.storage_buffers;
    info.num_uniform_buffers = code.uniform_buffers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    if (shader == nullptr) {
        throw sdl_error("shader " + std::string(code.name) + " would not load");
    }
    return shader;
}

SDL_GPUTexture* make_texture(SDL_GPUDevice* device, SDL_GPUTextureFormat format,
                             SDL_GPUTextureUsageFlags usage, int width, int height) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    return SDL_CreateGPUTexture(device, &info);
}

// The mesh pipeline, with the depth test for the world, or without it for what
// is drawn over the world.
SDL_GPUGraphicsPipeline* make_mesh_pipeline(SDL_GPUDevice* device, bool depth_test) {
    SDL_GPUShader* vertex = make_shader(device, shaders::mesh_vertex);
    SDL_GPUShader* fragment = nullptr;
    try {
        fragment = make_shader(device, shaders::mesh_fragment);
    } catch (...) {
        SDL_ReleaseGPUShader(device, vertex);
        throw;
    }

    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.pitch = sizeof(Vertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    SDL_GPUVertexAttribute attributes[2]{};
    attributes[0].location = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attributes[0].offset = offsetof(Vertex, position);
    attributes[1].location = 1;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[1].offset = offsetof(Vertex, colour);

    SDL_GPUColorTargetDescription colour{};
    colour.format = colour_format;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 2;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.enable_depth_clip = true;
    info.depth_stencil_state.enable_depth_test = depth_test;
    info.depth_stencil_state.enable_depth_write = depth_test;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER; // reversed
    info.target_info.color_target_descriptions = &colour;
    info.target_info.num_color_targets = 1;
    info.target_info.depth_stencil_format = depth_format;
    info.target_info.has_depth_stencil_target = true;

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, vertex);
    SDL_ReleaseGPUShader(device, fragment);
    if (pipeline == nullptr) {
        throw sdl_error("the mesh pipeline would not build");
    }
    return pipeline;
}

} // namespace

Renderer::Renderer(const std::string& driver, SDL_Window* window, int width, int height)
    : window_(window), width_(width), height_(height) {
    device_ =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC |
                                SDL_GPU_SHADERFORMAT_MSL,
                            false, driver.empty() ? nullptr : driver.c_str());
    if (device_ == nullptr) {
        throw sdl_error("no GPU device" +
                        (driver.empty() ? std::string() : " for " + driver));
    }
    try {
        if (window_ != nullptr && !SDL_ClaimWindowForGPUDevice(device_, window_)) {
            window_ = nullptr;
            throw sdl_error("the GPU device would not take the window");
        }
        target_ = make_texture(device_, colour_format,
                               SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                   SDL_GPU_TEXTUREUSAGE_SAMPLER,
                               width_, height_);
        if (target_ == nullptr) {
            throw sdl_error("no render target");
        }
        if (!SDL_GPUTextureSupportsFormat(device_, depth_format, SDL_GPU_TEXTURETYPE_2D,
                                          SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
            throw std::runtime_error("the GPU device has no 32-bit float depth buffer");
        }
        depth_ =
            make_texture(device_, depth_format,
                         SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, width_, height_);
        if (depth_ == nullptr) {
            throw sdl_error("no depth buffer");
        }
        mesh_pipeline_ = make_mesh_pipeline(device_, true);
        overlay_pipeline_ = make_mesh_pipeline(device_, false);
    } catch (...) {
        release();
        throw;
    }
}

Renderer::~Renderer() {
    release();
}

void Renderer::release() {
    for (const GpuMesh& mesh : meshes_) {
        SDL_ReleaseGPUBuffer(device_, mesh.vertices);
        SDL_ReleaseGPUBuffer(device_, mesh.indices);
    }
    meshes_.clear();
    SDL_ReleaseGPUBuffer(device_, overlay_.vertices);
    SDL_ReleaseGPUBuffer(device_, overlay_.indices);
    if (overlay_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device_, overlay_pipeline_);
    }
    if (mesh_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device_, mesh_pipeline_);
    }
    if (depth_ != nullptr) {
        SDL_ReleaseGPUTexture(device_, depth_);
    }
    if (target_ != nullptr) {
        SDL_ReleaseGPUTexture(device_, target_);
    }
    if (window_ != nullptr) {
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
    }
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
}

std::string Renderer::driver() const {
    return SDL_GetGPUDeviceDriver(device_);
}

MeshId Renderer::add_mesh(const Mesh& mesh) {
    GpuMesh gpu;
    upload(gpu, mesh, false);
    meshes_.push_back(gpu);
    return meshes_.size() - 1;
}

// Uploads `mesh` into `gpu`, making its buffers, or with `reuse` making them
// only when those it has are too small - the overlay, which changes every
// frame, keeps its buffers and is written over.
void Renderer::upload(GpuMesh& gpu, const Mesh& mesh, bool reuse) {
    const auto vertex_bytes =
        static_cast<Uint32>(mesh.vertices.size() * sizeof(Vertex));
    const auto index_bytes =
        static_cast<Uint32>(mesh.indices.size() * sizeof(std::uint32_t));
    if (vertex_bytes == 0 || index_bytes == 0) {
        throw std::runtime_error("a mesh needs vertices and indices");
    }
    gpu.index_count = static_cast<std::uint32_t>(mesh.indices.size());

    const bool grow = !reuse || vertex_bytes > overlay_vertex_capacity_ ||
                      index_bytes > overlay_index_capacity_;
    if (grow) {
        SDL_ReleaseGPUBuffer(device_, gpu.vertices);
        SDL_ReleaseGPUBuffer(device_, gpu.indices);
        gpu.vertices = nullptr;
        gpu.indices = nullptr;
        Uint32 vertex_size = vertex_bytes;
        Uint32 index_size = index_bytes;
        if (reuse) {
            // Room to grow into, so a HUD whose text changes length does not
            // make new buffers every frame.
            vertex_size = std::max<Uint32>(vertex_bytes * 2, 1u << 16);
            index_size = std::max<Uint32>(index_bytes * 2, 1u << 14);
            overlay_vertex_capacity_ = vertex_size;
            overlay_index_capacity_ = index_size;
        }
        SDL_GPUBufferCreateInfo buffer_info{};
        buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        buffer_info.size = vertex_size;
        gpu.vertices = SDL_CreateGPUBuffer(device_, &buffer_info);
        buffer_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        buffer_info.size = index_size;
        gpu.indices = SDL_CreateGPUBuffer(device_, &buffer_info);
        if (gpu.vertices == nullptr || gpu.indices == nullptr) {
            const auto error = sdl_error("no buffers for a mesh");
            SDL_ReleaseGPUBuffer(device_, gpu.vertices);
            SDL_ReleaseGPUBuffer(device_, gpu.indices);
            gpu = {};
            if (reuse) {
                overlay_vertex_capacity_ = overlay_index_capacity_ = 0;
            }
            throw error;
        }
    }

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = vertex_bytes + index_bytes;
    SDL_GPUTransferBuffer* transfer =
        SDL_CreateGPUTransferBuffer(device_, &transfer_info);
    void* mapped = transfer == nullptr
                       ? nullptr
                       : SDL_MapGPUTransferBuffer(device_, transfer, false);
    if (mapped == nullptr) {
        const auto error = sdl_error("the mesh could not be uploaded");
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        throw error;
    }
    std::memcpy(mapped, mesh.vertices.data(), vertex_bytes);
    std::memcpy(static_cast<char*>(mapped) + vertex_bytes, mesh.indices.data(),
                index_bytes);
    SDL_UnmapGPUTransferBuffer(device_, transfer);

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTransferBufferLocation from{transfer, 0};
    SDL_GPUBufferRegion to{gpu.vertices, 0, vertex_bytes};
    SDL_UploadToGPUBuffer(copy, &from, &to, false);
    from.offset = vertex_bytes;
    to = {gpu.indices, 0, index_bytes};
    SDL_UploadToGPUBuffer(copy, &from, &to, false);
    SDL_EndGPUCopyPass(copy);
    const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device_, transfer);
    if (!submitted) {
        throw sdl_error("the mesh upload was not submitted");
    }
}

void Renderer::render(const Camera& camera, std::span<const Draw> draws,
                      const Mesh* overlay) {
    if (overlay != nullptr && !overlay->indices.empty()) {
        upload(overlay_, *overlay, true);
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    if (commands == nullptr) {
        throw sdl_error("no command buffer");
    }

    SDL_GPUColorTargetInfo target{};
    target.texture = target_;
    target.clear_color = {sky.r, sky.g, sky.b, sky.a};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = depth_;
    depth.clear_depth = 0.0f; // infinitely far, in reversed depth
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, &depth);
    if (!draws.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, mesh_pipeline_);
        const Mat4f clip_from_camera = projection(
            camera.vertical_fov_rad,
            static_cast<double>(width_) / static_cast<double>(height_), camera.near_m);
        for (const Draw& draw : draws) {
            const GpuMesh& mesh = meshes_.at(draw.mesh);
            SDL_GPUBufferBinding vertices{mesh.vertices, 0};
            SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
            SDL_GPUBufferBinding indices{mesh.indices, 0};
            SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            const Mat4f clip_from_local =
                clip_from_camera * camera_from_local(camera, draw.placement);
            SDL_PushGPUVertexUniformData(commands, 0, clip_from_local.m.data(),
                                         sizeof clip_from_local.m);
            SDL_DrawGPUIndexedPrimitives(pass, mesh.index_count, 1, 0, 0, 0);
        }
    }
    if (overlay != nullptr && !overlay->indices.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, overlay_pipeline_);
        SDL_GPUBufferBinding vertices{overlay_.vertices, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
        SDL_GPUBufferBinding indices{overlay_.indices, 0};
        SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        Mat4f identity;
        identity.m[0] = identity.m[5] = identity.m[10] = identity.m[15] = 1.0f;
        SDL_PushGPUVertexUniformData(commands, 0, identity.m.data(), sizeof identity.m);
        SDL_DrawGPUIndexedPrimitives(pass, overlay_.index_count, 1, 0, 0, 0);
    }
    SDL_EndGPURenderPass(pass);

    if (window_ != nullptr) {
        SDL_GPUTexture* swapchain = nullptr;
        Uint32 w = 0;
        Uint32 h = 0;
        if (SDL_WaitAndAcquireGPUSwapchainTexture(commands, window_, &swapchain, &w,
                                                  &h) &&
            swapchain != nullptr) {
            SDL_GPUBlitInfo blit{};
            blit.source.texture = target_;
            blit.source.w = static_cast<Uint32>(width_);
            blit.source.h = static_cast<Uint32>(height_);
            blit.destination.texture = swapchain;
            blit.destination.w = w;
            blit.destination.h = h;
            blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
            blit.filter = SDL_GPU_FILTER_LINEAR;
            SDL_BlitGPUTexture(commands, &blit);
            ++presented_;
        }
    }
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        throw sdl_error("the frame was not submitted");
    }
}

Frame Renderer::capture() {
    const Uint32 size = static_cast<Uint32>(width_) * static_cast<Uint32>(height_) * 4u;
    SDL_GPUTransferBufferCreateInfo buffer_info{};
    buffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    buffer_info.size = size;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device_, &buffer_info);
    if (buffer == nullptr) {
        throw sdl_error("no download buffer");
    }

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion region{};
    region.texture = target_;
    region.w = static_cast<Uint32>(width_);
    region.h = static_cast<Uint32>(height_);
    region.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = buffer;
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence == nullptr) {
        SDL_ReleaseGPUTransferBuffer(device_, buffer);
        throw sdl_error("the read-back was not submitted");
    }
    SDL_WaitForGPUFences(device_, true, &fence, 1);
    SDL_ReleaseGPUFence(device_, fence);

    Frame frame;
    frame.width = width_;
    frame.height = height_;
    frame.rgba.resize(size);
    const void* mapped = SDL_MapGPUTransferBuffer(device_, buffer, false);
    if (mapped == nullptr) {
        SDL_ReleaseGPUTransferBuffer(device_, buffer);
        throw sdl_error("the read-back could not be mapped");
    }
    std::memcpy(frame.rgba.data(), mapped, size);
    SDL_UnmapGPUTransferBuffer(device_, buffer);
    SDL_ReleaseGPUTransferBuffer(device_, buffer);
    return frame;
}

void save_bmp(const Frame& frame, const std::string& path) {
    // SDL takes the pixels as writable but does not write to them when saving.
    std::vector<std::uint8_t> pixels = frame.rgba;
    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(frame.width, frame.height, SDL_PIXELFORMAT_RGBA32,
                              pixels.data(), frame.width * 4);
    if (surface == nullptr) {
        throw sdl_error("no surface for the frame");
    }
    const bool saved = SDL_SaveBMP(surface, path.c_str());
    SDL_DestroySurface(surface);
    if (!saved) {
        throw sdl_error("could not write " + path);
    }
}

} // namespace glideslope::gfx

#include "gfx/renderer.hpp"

#include <SDL3/SDL.h>

#include <cstring>
#include <stdexcept>

namespace glideslope::gfx {

namespace {

std::runtime_error sdl_error(const std::string& what) {
    return std::runtime_error(what + ": " + SDL_GetError());
}

} // namespace

Renderer::Renderer(const std::string& driver, SDL_Window* window, int width, int height)
    : window_(window), width_(width), height_(height) {
    device_ =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                SDL_GPU_SHADERFORMAT_MSL,
                            false, driver.empty() ? nullptr : driver.c_str());
    if (device_ == nullptr) {
        throw sdl_error("no GPU device" +
                        (driver.empty() ? std::string() : " for " + driver));
    }
    if (window_ != nullptr && !SDL_ClaimWindowForGPUDevice(device_, window_)) {
        const auto error = sdl_error("the GPU device would not take the window");
        SDL_DestroyGPUDevice(device_);
        throw error;
    }

    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = static_cast<Uint32>(width_);
    info.height = static_cast<Uint32>(height_);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    target_ = SDL_CreateGPUTexture(device_, &info);
    if (target_ == nullptr) {
        const auto error = sdl_error("no render target");
        if (window_ != nullptr) {
            SDL_ReleaseWindowFromGPUDevice(device_, window_);
        }
        SDL_DestroyGPUDevice(device_);
        throw error;
    }
}

Renderer::~Renderer() {
    SDL_ReleaseGPUTexture(device_, target_);
    if (window_ != nullptr) {
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
    }
    SDL_DestroyGPUDevice(device_);
}

std::string Renderer::driver() const {
    return SDL_GetGPUDeviceDriver(device_);
}

void Renderer::render() {
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    if (commands == nullptr) {
        throw sdl_error("no command buffer");
    }

    SDL_GPUColorTargetInfo target{};
    target.texture = target_;
    target.clear_color = {sky.r, sky.g, sky.b, sky.a};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_EndGPURenderPass(SDL_BeginGPURenderPass(commands, &target, 1, nullptr));

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

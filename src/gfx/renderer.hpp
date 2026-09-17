#pragma once

// The renderer: one SDL_GPU device, over Vulkan, Direct3D 12 or Metal.
//
// It draws meshes placed on the Earth into an offscreen colour target with a
// float depth buffer, over the sky's clear colour, blits that to the window if
// there is one, and can read a frame back off the GPU. Positions are
// camera-relative and depth is reversed; see gfx/scene.hpp.

#include "gfx/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct SDL_GPUBuffer;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUTexture;
struct SDL_Window;

namespace glideslope::gfx {

struct Frame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // top row first, four bytes a pixel
};

// The colour the sky is cleared to, before anything is drawn.
struct Colour {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};
inline constexpr Colour sky{0.45f, 0.65f, 0.90f, 1.0f};

using MeshId = std::size_t;

struct Draw {
    MeshId mesh = 0;
    Placement placement;
};

class Renderer {
public:
    // Creates the GPU device. `driver` names SDL's GPU driver - "vulkan",
    // "direct3d12", "metal" - or is empty to let SDL choose. `window` may be null
    // to render headless. Throws std::runtime_error, with SDL's reason, if no
    // device can be made.
    Renderer(const std::string& driver, SDL_Window* window, int width, int height);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // The driver SDL chose.
    std::string driver() const;

    // Uploads a mesh to the GPU, to be drawn by the id returned.
    MeshId add_mesh(const Mesh& mesh);

    // Draws one frame, from `camera`, into the offscreen target, and to the
    // window if there is one. With no draws it is the sky alone.
    void render(const Camera& camera, std::span<const Draw> draws);

    // The last frame rendered, read back from the GPU.
    Frame capture();

    // How many frames have reached the window's swapchain. A window that is
    // hidden or minimised hands out no swapchain image, so this can trail the
    // frames rendered.
    long presented() const {
        return presented_;
    }

private:
    struct GpuMesh {
        SDL_GPUBuffer* vertices = nullptr;
        SDL_GPUBuffer* indices = nullptr;
        std::uint32_t index_count = 0;
    };

    void release();

    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUTexture* target_ = nullptr;
    SDL_GPUTexture* depth_ = nullptr;
    SDL_GPUGraphicsPipeline* mesh_pipeline_ = nullptr;
    std::vector<GpuMesh> meshes_;
    int width_ = 0;
    int height_ = 0;
    long presented_ = 0;
};

// Writes `frame` as a 32-bit BMP. Throws std::runtime_error on failure.
void save_bmp(const Frame& frame, const std::string& path);

} // namespace glideslope::gfx

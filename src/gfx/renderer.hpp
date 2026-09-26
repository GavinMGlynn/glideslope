#pragma once

// The renderer: one SDL_GPU device, over Vulkan, Direct3D 12 or Metal.
//
// It draws meshes placed on the Earth into an offscreen colour target with a
// float depth buffer, over the sky's clear colour, blits that to the window if
// there is one, and can read a frame back off the GPU. Positions are
// camera-relative and depth is reversed; see gfx/scene.hpp.

#include "gfx/scene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

struct SDL_GPUBuffer;
struct SDL_GPUDevice;
struct SDL_GPUFence;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
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

// The air a frame is seen through. What is seen at a distance d through air of
// visibility V keeps exp(-ln 20 d / V) of its contrast with the haze's colour -
// Koschmieder's law, with visibility where 5% is left. Below `top_m` the
// visibility is `below_m`, above it `above_m`; heights are above `station`, an
// east-north-up frame, with the Earth's curvature. Clear air, the default, has
// no haze.
struct Haze {
    Colour colour{0.72f, 0.75f, 0.80f, 1.0f};
    double below_m = std::numeric_limits<double>::infinity();
    double above_m = std::numeric_limits<double>::infinity();
    double top_m = 0.0;
    Placement station;
};

using MeshId = std::size_t;
using TextureId = std::size_t;

// No texture: the mesh's vertex colours alone.
inline constexpr TextureId no_texture = static_cast<TextureId>(-1);

struct Draw {
    MeshId mesh = 0;
    Placement placement;
    TextureId texture = no_texture;
    // The mesh's texture coordinates to the texture's: scale u and v, then add.
    std::array<float, 4> uv_transform{1.0f, 1.0f, 0.0f, 0.0f};
    // Blended over what is behind it by its alpha, after everything opaque,
    // in the order given, and hiding nothing behind it.
    bool translucent = false;
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

    // Frees a mesh's GPU buffers. Its id may be given to a mesh added later, and
    // drawing it before then throws.
    void remove_mesh(MeshId id);

    // Uploads an image - `width` by `height` pixels, four bytes each, the top
    // row first - as a texture with mipmaps, sampled linearly and clamped at
    // its edges.
    TextureId add_texture(int width, int height, const std::uint8_t* rgba);

    // Frees a texture. Its id may be given to one added later, and drawing
    // with it before then throws.
    void remove_texture(TextureId id);

    // How many textures are on the GPU.
    std::size_t texture_count() const {
        return textures_.size() - free_textures_.size();
    }

    // How many meshes are on the GPU.
    std::size_t mesh_count() const {
        return meshes_.size() - free_meshes_.size();
    }

    // Draws one frame, from `camera`, into the offscreen target, and to the
    // window if there is one: over `background`, the opaque draws, then the
    // translucent ones, through `haze`. With no draws it is the background
    // alone. `overlay`, if given, is in clip space and drawn over everything,
    // with no depth test and no haze: the HUD.
    void render(const Camera& camera, std::span<const Draw> draws,
                const Mesh* overlay = nullptr, const Haze& haze = {},
                const Colour& background = sky);

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
    void upload(GpuMesh& gpu, const Mesh& mesh, bool reuse);
    SDL_GPUTexture* upload_texture(int width, int height, const std::uint8_t* rgba);

    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUTexture* target_ = nullptr;
    SDL_GPUTexture* depth_ = nullptr;
    SDL_GPUGraphicsPipeline* mesh_pipeline_ = nullptr;
    SDL_GPUGraphicsPipeline* translucent_pipeline_ = nullptr;
    SDL_GPUGraphicsPipeline* overlay_pipeline_ = nullptr;
    std::vector<GpuMesh> meshes_;
    std::vector<MeshId> free_meshes_;
    std::vector<SDL_GPUTexture*> textures_;
    std::vector<TextureId> free_textures_;
    SDL_GPUTexture* white_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;
    GpuMesh overlay_;
    std::uint32_t overlay_vertex_capacity_ = 0; // bytes
    std::uint32_t overlay_index_capacity_ = 0;
    int width_ = 0;
    int height_ = 0;
    long presented_ = 0;
    // **Headless, a frame waits for the one two before it.** With a window,
    // acquiring the swapchain's image waits for the GPU to catch up; with
    // none, nothing does, and SDL keeps every frame submitted - its command
    // buffer, its uniforms, its uploads - until the GPU has drawn it. Mesa's
    // lavapipe draws on the CPU, slower than the frames come, so they queued
    // without end: 0.8 MiB a frame, and the process past 1.6 GB and dead by
    // frame 2,000. Each headless frame's fence is kept here, and the frame
    // after next waits on it, as a swapchain of two would.
    std::array<SDL_GPUFence*, 2> in_flight_{};
    std::size_t next_in_flight_ = 0;
};

// Writes `frame` as a 32-bit BMP. Throws std::runtime_error on failure.
void save_bmp(const Frame& frame, const std::string& path);

} // namespace glideslope::gfx

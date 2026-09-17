#pragma once

// The renderer: one SDL_GPU device, over Vulkan, Direct3D 12 or Metal.
//
// Today it clears its colour target to the sky and can read a frame back off
// the GPU; terrain, aircraft and the HUD draw into the same target as they
// arrive.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct SDL_GPUDevice;
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

    // Draws one frame into the offscreen target, and to the window if there is
    // one.
    void render();

    // The last frame rendered, read back from the GPU.
    Frame capture();

private:
    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUTexture* target_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

// Writes `frame` as a 32-bit BMP. Throws std::runtime_error on failure.
void save_bmp(const Frame& frame, const std::string& path);

} // namespace glideslope::gfx

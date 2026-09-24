#include "frontend/server/window.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace glideslope::server {

namespace {

// SDL's debug font is 8 pixels square; drawn at twice that it can be read
// across a room.
constexpr float scale = 2.0f;
constexpr float glyph = 8.0f;
constexpr float line_height = 11.0f; // in the font's own pixels, before scaling
constexpr float margin = 6.0f;
// The drop buttons sit to the right of the slot rows, which are 44 characters.
constexpr float button_column = 48.0f;
constexpr int width_px = 1120;
constexpr int height_px = 720;

} // namespace

std::unique_ptr<Window> Window::open(std::string& why) {
#if defined(__linux__)
    // **A desktop's display, or none.** Given no X11 or Wayland display, SDL
    // goes on to drive the console's graphics directly (kmsdrm), through Mesa,
    // which on a server fails somewhere deep and leaks on the way; a window on
    // a server's console is not what --window is for either. Only the two a
    // desktop has are tried, so that no display is a clean refusal.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        why = std::string("SDL's video would not start: ") + SDL_GetError();
        return nullptr;
    }
    std::unique_ptr<Window> w(new Window());
    w->window_ = SDL_CreateWindow("glideslope server", width_px, height_px,
                                  SDL_WINDOW_RESIZABLE);
    if (w->window_ == nullptr) {
        why = std::string("no window could be made: ") + SDL_GetError();
        return nullptr;
    }
    w->renderer_ = SDL_CreateRenderer(w->window_, nullptr);
    if (w->renderer_ == nullptr) {
        why = std::string("no renderer could be made: ") + SDL_GetError();
        return nullptr;
    }
    SDL_SetRenderScale(w->renderer_, scale, scale);
    return w;
}

Window::~Window() {
    if (frame_ != nullptr) {
        SDL_DestroySurface(frame_);
    }
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool Window::pump() {
    bool open = true;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            open = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                // Into the renderer's own coordinates, which the scale has
                // made the font's pixels.
                float x = e.button.x;
                float y = e.button.y;
                SDL_RenderCoordinatesFromWindow(renderer_, e.button.x, e.button.y, &x, &y);
                clicked_ = true;
                click_x_ = x;
                click_y_ = y;
            }
            break;
        default:
            break;
        }
    }
    return open;
}

std::optional<std::string> Window::draw(const Dashboard& d) {
    std::optional<std::string> pressed;
    if (keeping_) {
        drawn_.clear();
    }
    SDL_SetRenderDrawColor(renderer_, 18, 22, 30, 255);
    SDL_RenderClear(renderer_);

    const std::vector<std::string> lines = dashboard_lines(d);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const float y = margin + static_cast<float>(i) * line_height;
        SDL_SetRenderDrawColor(renderer_, 220, 226, 235, 255);
        SDL_RenderDebugText(renderer_, margin, y, lines[i].c_str());
        if (keeping_) {
            drawn_.push_back(lines[i]);
        }
    }

    // **A drop button beside every slot somebody is in.**
    for (std::size_t i = 0; i < d.slots.size(); ++i) {
        const DashboardSlot& s = d.slots[i];
        if (s.address.empty()) {
            continue;
        }
        const std::string label = "drop " + std::to_string(s.slot);
        const float y = margin + static_cast<float>(dashboard_slot_line(i)) * line_height;
        const SDL_FRect box{margin + button_column * glyph - 2.0f, y - 2.0f,
                            static_cast<float>(label.size()) * glyph + 4.0f, glyph + 3.0f};
        SDL_SetRenderDrawColor(renderer_, 150, 45, 45, 255);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer_, box.x + 2.0f, y, label.c_str());
        if (keeping_) {
            drawn_.push_back("[" + label + "]");
        }
        const bool clicked = clicked_ && click_x_ >= box.x && click_x_ < box.x + box.w &&
                             click_y_ >= box.y && click_y_ < box.y + box.h;
        // **A press by a test takes the same path a click does**: the first
        // time the button is drawn, it is as if it had been clicked.
        const bool asked = !pending_press_.empty() && pending_press_ == label;
        if ((clicked || asked) && !pressed) {
            pressed = s.address;
            if (asked) {
                pending_press_.clear();
            }
        }
    }
    clicked_ = false;

    if (keeping_frame_) {
        SDL_Surface* now = SDL_RenderReadPixels(renderer_, nullptr);
        if (now != nullptr) {
            if (frame_ != nullptr) {
                SDL_DestroySurface(frame_);
            }
            frame_ = now;
        }
    }
    SDL_RenderPresent(renderer_);
    return pressed;
}

int Window::frame_width() const {
    return frame_ != nullptr ? frame_->w : 0;
}

int Window::frame_height() const {
    return frame_ != nullptr ? frame_->h : 0;
}

bool Window::shot(const std::string& path) const {
    return frame_ != nullptr && SDL_SaveBMP(frame_, path.c_str());
}

} // namespace glideslope::server

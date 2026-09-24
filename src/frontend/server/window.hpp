#pragma once

// The server's window, when `--window` asks for one.
//
// **A choice made at start, never a default.** The server is meant to run on
// a machine nobody looks at - a cloud host with no screen - so it runs in the
// terminal unless told otherwise, and nothing here is touched: SDL's video is
// started only when a window is asked for, and SDL loads a display library
// only then, so a server without `--window` runs where none is installed.
//
// **It draws the dashboard's lines** (dashboard.hpp) with SDL's renderer and
// its built-in 8x8 font - no font file, no widget library - and a drop button
// beside each slot that has somebody in it. Closing the window stops the
// server.
//
// **Three test flags reach into it**, as gearstick's server window has them:
// `--window-dump` keeps every piece of text the window draws and prints it at
// the end, `--window-shot FILE` writes its last frame as a BMP, and
// `--window-press LABEL` presses a button the first time it is drawn, through
// the same path a click takes.

#include "frontend/server/dashboard.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Surface;

namespace glideslope::server {

class Window {
public:
    // Opens it, or says why it could not: no display, or SDL's video or a
    // window or a renderer that would not start.
    static std::unique_ptr<Window> open(std::string& why);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Takes the window's events. False once somebody has closed it, which is
    // the operator saying stop.
    bool pump();

    // Draws the dashboard, and says which connection's drop button was
    // pressed since the last frame - by its address - if one was.
    std::optional<std::string> draw(const Dashboard& d);

    // For tests: keep what is drawn, press a button by its label, keep the
    // last frame.
    void keep_text() { keeping_ = true; }
    void press(const std::string& label) { pending_press_ = label; }
    void keep_frame() { keeping_frame_ = true; }

    // What the last frame drew, a line of text at a time, and its buttons as
    // `[label]`.
    const std::vector<std::string>& drawn() const { return drawn_; }
    // Writes the last frame kept as a BMP, and how big it was.
    bool shot(const std::string& path) const;
    int frame_width() const;
    int frame_height() const;

private:
    Window() = default;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Surface* frame_ = nullptr;
    bool keeping_ = false;
    bool keeping_frame_ = false;
    std::string pending_press_;
    bool clicked_ = false;
    float click_x_ = 0.0f;
    float click_y_ = 0.0f;
    std::vector<std::string> drawn_;
};

} // namespace glideslope::server

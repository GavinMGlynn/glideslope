// The server's window in a build that has none: `-DGLIDESLOPE_SERVER_ONLY=ON`,
// which is what deploy/Dockerfile builds, leaves SDL out altogether. `--window`
// is then refused, saying why, as it is on a machine with no display.

#include "frontend/server/window.hpp"

namespace glideslope::server {

std::unique_ptr<Window> Window::open(std::string& why) {
    why = "this server was built without one (GLIDESLOPE_SERVER_ONLY)";
    return nullptr;
}

Window::~Window() = default;

bool Window::pump() {
    return false;
}

std::optional<std::string> Window::draw(const Dashboard&) {
    return std::nullopt;
}

int Window::frame_width() const {
    return 0;
}

int Window::frame_height() const {
    return 0;
}

bool Window::shot(const std::string&) const {
    return false;
}

} // namespace glideslope::server

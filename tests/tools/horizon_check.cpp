// horizon_check - the HUD's horizon line lies on the horizon drawn.
//
//   glideslope_horizon_check FRAME.bmp SAID.txt TOLERANCE_PX
//
// FRAME is a `glideslope --shot` of a flight over level ground, in clear air,
// with --imagery off, so that the sky is blue and the ground green; SAID is
// what the client printed, which names the HUD's horizon line it drew:
// "glideslope: the HUD's horizon runs from X0,Y0 to X1,Y1".
//
// **The line is what was drawn**: of points along its centre on the frame,
// every one within a pixel of a pixel of the HUD's colour, and at least 20.
//
// **The horizon drawn is where the sky ends.** In every column of the frame
// outside the line's own columns (and six pixels either side), from 8 pixels
// in from either edge, the line is carried on across the frame; within 60
// pixels of it, the place where a sky pixel - blue at least 60 above red -
// meets one that is not sky is found, the HUD's own pixels (its text, its
// line) left out; the one nearest the line is the horizon drawn there.
// Above the credits' strip only: the bottom 40 rows are left out. Its distance
// from the carried line, square to the line, is what is held.
//
// **What is held**: in at least a tenth of the frame's columns the horizon
// drawn is found (banked 60 degrees, the line crosses a third of them), and in every one it is within TOLERANCE_PX of the line.
//
// Exits 0 when they agree, 1 when they do not, and 2 when it cannot read what
// it was given.

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // top row first, four bytes a pixel

    const std::uint8_t* at(int x, int y) const {
        return rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(x)) *
                                 4;
    }
};

bool load(const std::string& path, Image& out) {
    SDL_Surface* loaded = SDL_LoadBMP(path.c_str());
    if (loaded == nullptr) {
        std::fprintf(stderr, "horizon_check: cannot read %s: %s\n", path.c_str(),
                     SDL_GetError());
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr) {
        std::fprintf(stderr, "horizon_check: cannot convert %s: %s\n", path.c_str(),
                     SDL_GetError());
        return false;
    }
    out.width = rgba->w;
    out.height = rgba->h;
    out.rgba.resize(static_cast<std::size_t>(rgba->w) * static_cast<std::size_t>(rgba->h) * 4);
    for (int y = 0; y < rgba->h; ++y) {
        const std::uint8_t* row = static_cast<const std::uint8_t*>(rgba->pixels) +
                                  static_cast<std::size_t>(y) *
                                      static_cast<std::size_t>(rgba->pitch);
        std::copy(row, row + static_cast<std::ptrdiff_t>(rgba->w) * 4,
                  out.rgba.begin() + static_cast<std::ptrdiff_t>(y) *
                                         static_cast<std::ptrdiff_t>(rgba->w) * 4);
    }
    SDL_DestroySurface(rgba);
    return true;
}

// The HUD's colour, gfx::hud_colour: green far above red and blue.
bool hud(const std::uint8_t* p) {
    return p[1] >= 200 && p[0] <= 120 && p[2] <= 160;
}

bool sky(const std::uint8_t* p) {
    return !hud(p) && static_cast<int>(p[2]) >= static_cast<int>(p[0]) + 60;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fputs("usage: glideslope_horizon_check FRAME.bmp SAID.txt TOLERANCE_PX\n", stderr);
        return 2;
    }
    Image image;
    if (!load(argv[1], image)) {
        return 2;
    }
    const double tolerance_px = std::strtod(argv[3], nullptr);
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    bool said = false;
    {
        std::ifstream in(argv[2]);
        std::string line;
        // Read with strtod, not sscanf, which MSVC refuses as unsafe.
        const std::string prefix = "glideslope: the HUD's horizon runs from ";
        while (!said && std::getline(in, line)) {
            if (line.rfind(prefix, 0) != 0) {
                continue;
            }
            const char* p = line.c_str() + prefix.size();
            char* end = nullptr;
            x0 = std::strtod(p, &end);
            bool ok = end != p && *end == ',';
            p = end + 1;
            y0 = std::strtod(p, &end);
            ok = ok && end != p && std::string(end).rfind(" to ", 0) == 0;
            p = end + 4;
            x1 = std::strtod(p, &end);
            ok = ok && end != p && *end == ',';
            p = end + 1;
            y1 = std::strtod(p, &end);
            said = ok && end != p;
        }
    }
    if (!said) {
        std::fprintf(stderr, "horizon_check: %s names no HUD horizon\n", argv[2]);
        return 2;
    }

    // **The line is what was drawn.**
    int on_frame = 0;
    int lit = 0;
    for (int i = 0; i <= 100; ++i) {
        const double t = i / 100.0;
        const auto x = static_cast<int>(std::lround(x0 + (x1 - x0) * t));
        const auto y = static_cast<int>(std::lround(y0 + (y1 - y0) * t));
        if (x < 1 || y < 1 || x >= image.width - 1 || y >= image.height - 41) {
            continue;
        }
        ++on_frame;
        bool found = false;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                found = found || hud(image.at(x + dx, y + dy));
            }
        }
        lit += found ? 1 : 0;
    }
    std::printf("the HUD's horizon from %.1f,%.1f to %.1f,%.1f: %d of %d points on the frame "
                "drawn\n",
                x0, y0, x1, y1, lit, on_frame);
    if (on_frame < 20 || lit != on_frame) {
        std::printf("horizon_check: the line named is not the line drawn\n");
        return 1;
    }

    // **The horizon drawn, column by column.**
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    if (std::abs(dx) < 1.0) {
        std::printf("horizon_check: the line is upright, and no column crosses it\n");
        return 2;
    }
    const double square = std::abs(dx) / std::hypot(dx, dy);
    const double left = std::min(x0, x1) - 6.0;
    const double right = std::max(x0, x1) + 6.0;
    const int bottom = image.height - 40;
    int columns = 0;
    double worst = 0.0;
    int worst_x = 0;
    double total = 0.0;
    for (int x = 8; x < image.width - 8; ++x) {
        if (x >= left && x <= right) {
            continue;
        }
        const double line_y = y0 + (x - x0) * dy / dx;
        double best = 1e9;
        const int from = std::max(0, static_cast<int>(std::floor(line_y)) - 60);
        const int to = std::min(bottom - 1, static_cast<int>(std::ceil(line_y)) + 60);
        for (int y = from; y < to; ++y) {
            const std::uint8_t* a = image.at(x, y);
            const std::uint8_t* b = image.at(x, y + 1);
            if (hud(a) || hud(b) || sky(a) == sky(b)) {
                continue;
            }
            const double off = (y + 1.0) - line_y; // the edge between the two
            if (std::abs(off) < std::abs(best)) {
                best = off;
            }
        }
        if (best == 1e9) {
            continue;
        }
        ++columns;
        const double square_off = std::abs(best) * square;
        total += square_off;
        if (square_off > worst) {
            worst = square_off;
            worst_x = x;
        }
    }
    std::printf("the horizon drawn found in %d of %d columns; from the HUD's line %.2f px at "
                "worst (column %d), %.2f px on average; held to %.2f px\n",
                columns, image.width, worst, worst_x, columns > 0 ? total / columns : 0.0,
                tolerance_px);
    if (columns * 10 < image.width) {
        std::printf("horizon_check: the horizon drawn was found in too few columns\n");
        return 1;
    }
    if (worst > tolerance_px) {
        std::printf("horizon_check: the HUD's horizon is not on the horizon drawn\n");
        return 1;
    }
    return 0;
}

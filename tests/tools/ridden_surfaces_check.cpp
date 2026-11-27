// ridden_surfaces_check - another aircraft, ridden along in, is drawn with its
// control surfaces where the updates put them.
//
//   glideslope_ridden_surfaces_check MOVING.bmp STILL.bmp WITHOUT.bmp
//
// Three drawings of one frame by the client with the window
// (`--shot-ridden`), riding along in another player's aircraft from behind:
// the aircraft as the updates have it, the same with its surfaces still, and
// not drawn at all.
//
// **What is the aircraft**: the pixels where either drawing of it differs
// from the one without it. There must be a hundred at least - it is in view.
//
// **What its surfaces moved**: the pixels where the drawing as the updates
// have it differs from the one with its surfaces still. There must be ten at
// least; with the surfaces a state update carries sent as nought, the two
// drawings are the same.
//
// Exits 0 when both hold, 1 when one does not, and 2 when it cannot read
// what it was given.

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // top row first, four bytes a pixel
};

bool load(const std::string& path, Image& out) {
    SDL_Surface* loaded = SDL_LoadBMP(path.c_str());
    if (loaded == nullptr) {
        std::fprintf(stderr, "ridden_surfaces_check: cannot read %s: %s\n", path.c_str(),
                     SDL_GetError());
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr) {
        std::fprintf(stderr, "ridden_surfaces_check: cannot convert %s: %s\n", path.c_str(),
                     SDL_GetError());
        return false;
    }
    out.width = rgba->w;
    out.height = rgba->h;
    out.rgba.resize(static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height) *
                    4);
    for (int y = 0; y < out.height; ++y) {
        const auto* row = static_cast<const std::uint8_t*>(rgba->pixels) +
                          static_cast<std::ptrdiff_t>(y) * rgba->pitch;
        std::copy(row, row + static_cast<std::ptrdiff_t>(out.width) * 4,
                  out.rgba.begin() + static_cast<std::ptrdiff_t>(y) * out.width * 4);
    }
    SDL_DestroySurface(rgba);
    return true;
}

bool differs(const Image& a, const Image& b, std::size_t pixel) {
    for (std::size_t c = 0; c < 3; ++c) {
        if (a.rgba[pixel * 4 + c] != b.rgba[pixel * 4 + c]) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: glideslope_ridden_surfaces_check MOVING.bmp STILL.bmp "
                             "WITHOUT.bmp\n");
        return 2;
    }
    Image moving;
    Image still;
    Image without;
    if (!load(argv[1], moving) || !load(argv[2], still) || !load(argv[3], without)) {
        return 2;
    }
    if (moving.width != still.width || moving.height != still.height ||
        moving.width != without.width || moving.height != without.height) {
        std::fprintf(stderr, "ridden_surfaces_check: the three drawings are not one size\n");
        return 2;
    }
    const std::size_t pixels =
        static_cast<std::size_t>(moving.width) * static_cast<std::size_t>(moving.height);
    std::size_t aircraft = 0;
    std::size_t moved = 0;
    for (std::size_t p = 0; p < pixels; ++p) {
        if (differs(moving, without, p) || differs(still, without, p)) {
            ++aircraft;
        }
        if (differs(moving, still, p)) {
            ++moved;
        }
    }
    std::printf("the aircraft ridden in: %zu pixels; %zu moved by its surfaces\n", aircraft,
                moved);
    if (aircraft < 100) {
        std::fprintf(stderr, "the aircraft ridden in is not in view: %zu pixels\n", aircraft);
        return 1;
    }
    if (moved < 10) {
        std::fprintf(stderr, "its surfaces were drawn still: %zu pixels moved by them\n", moved);
        return 1;
    }
    return 0;
}

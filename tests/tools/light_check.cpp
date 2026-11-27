// light_check - the aeroplane's light follows it as it banks.
//
//   glideslope_light_check LEVEL.bmp LEVEL_WITHOUT.bmp BANKED.bmp BANKED_WITHOUT.bmp
//                          LEVEL_SUN_Z BANKED_SUN_Z
//
// Two shots of the aeroplane from above it, in the body frame - the view that
// sees her tops, wings and tailplane, and not her sides - one level and one
// banked, each with a shot of the same moment with the aeroplane not drawn
// (--draw-aircraft off). LEVEL_SUN_Z and BANKED_SUN_Z are the body-frame z of
// the unit vector towards the sun at each shot, as the client says it: +z is
// down, so the light on a face whose normal is straight up out of her top,
// (0, 0, -1), is gfx::light_on's 0.35 + 0.65 max(0, -z) / sqrt(0.5).
//
// **What is the aeroplane**: the pixels where the shot with it differs from
// the shot without, as surfaces_check finds it, less its rim: only those whose
// four neighbours are the aeroplane too, so that no pixel is half sky.
//
// **Her tops' light**: the mean brightness of those pixels, the three
// channels summed. The same faces are seen in both shots, the camera standing
// in the body frame, so their colours cancel in the ratio of the banked
// shot's to the level's, and what is left is the ratio of their light. It
// must be the ratio the sun says, within TOLERANCE of it, and the bank must
// move it: the sun's ratio is 0.85 or less, so a light that did not follow
// would be seen.
//
// Exits 0 when it holds, 1 when it does not, and 2 when it cannot read what it
// was given or the shots cannot test it.

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// The measured ratio within this share of the sun's: the tops are not one
// flat face, but a wing's camber and dihedral and a fuselage's curve.
constexpr double tolerance = 0.08;

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // top row first, four bytes a pixel
};

bool load(const std::string& path, Image& out) {
    SDL_Surface* loaded = SDL_LoadBMP(path.c_str());
    if (loaded == nullptr) {
        std::fprintf(stderr, "light_check: cannot read %s: %s\n", path.c_str(), SDL_GetError());
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr) {
        std::fprintf(stderr, "light_check: cannot convert %s: %s\n", path.c_str(),
                     SDL_GetError());
        return false;
    }
    out.width = rgba->w;
    out.height = rgba->h;
    out.rgba.resize(static_cast<std::size_t>(rgba->w) * static_cast<std::size_t>(rgba->h) * 4);
    for (int y = 0; y < rgba->h; ++y) {
        const std::uint8_t* row = static_cast<const std::uint8_t*>(rgba->pixels) +
                                  static_cast<std::size_t>(y) * static_cast<std::size_t>(rgba->pitch);
        std::copy(row, row + static_cast<std::ptrdiff_t>(rgba->w) * 4,
                  out.rgba.begin() +
                      static_cast<std::ptrdiff_t>(y) * static_cast<std::ptrdiff_t>(rgba->w) * 4);
    }
    SDL_DestroySurface(rgba);
    return true;
}

int apart(const Image& a, const Image& b, std::size_t pixel) {
    int sum = 0;
    for (std::size_t c = 0; c < 3; ++c) {
        sum += std::abs(static_cast<int>(a.rgba[pixel * 4 + c]) -
                        static_cast<int>(b.rgba[pixel * 4 + c]));
    }
    return sum;
}

// A difference smaller than this is a driver's rounding, not something drawn.
constexpr int drawn = 30;

struct Tops {
    long pixels = 0;
    double brightness = 0.0; // mean of the three channels summed, 0..765
};

// The aeroplane's pixels, less its rim, and their mean brightness.
Tops tops(const Image& with, const Image& without) {
    const int w = without.width;
    const int h = without.height;
    std::vector<bool> plane(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (std::size_t p = 0; p < plane.size(); ++p) {
        plane[p] = apart(with, without, p) > drawn;
    }
    Tops out;
    double sum = 0.0;
    for (int y = 1; y + 1 < h; ++y) {
        for (int x = 1; x + 1 < w; ++x) {
            const auto at = [&](int dx, int dy) {
                return plane[static_cast<std::size_t>(y + dy) * static_cast<std::size_t>(w) +
                             static_cast<std::size_t>(x + dx)];
            };
            if (!(at(0, 0) && at(-1, 0) && at(1, 0) && at(0, -1) && at(0, 1))) {
                continue;
            }
            const std::size_t p =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            sum += with.rgba[p * 4] + with.rgba[p * 4 + 1] + with.rgba[p * 4 + 2];
            ++out.pixels;
        }
    }
    out.brightness = out.pixels > 0 ? sum / static_cast<double>(out.pixels) : 0.0;
    return out;
}

// gfx::light_on for a face straight up out of the aeroplane's top.
double light_on_top(double sun_z) {
    return 0.35 + 0.65 * std::max(0.0, -sun_z) / std::sqrt(0.5);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 7) {
        std::fprintf(stderr, "usage: glideslope_light_check LEVEL.bmp LEVEL_WITHOUT.bmp "
                             "BANKED.bmp BANKED_WITHOUT.bmp LEVEL_SUN_Z BANKED_SUN_Z\n");
        return 2;
    }
    Image level;
    Image level_without;
    Image banked;
    Image banked_without;
    if (!load(argv[1], level) || !load(argv[2], level_without) || !load(argv[3], banked) ||
        !load(argv[4], banked_without)) {
        return 2;
    }
    for (const Image* i : {&level, &banked, &banked_without}) {
        if (i->width != level_without.width || i->height != level_without.height) {
            std::fprintf(stderr, "light_check: the four shots are not one size\n");
            return 2;
        }
    }
    const double level_light = light_on_top(std::atof(argv[5]));
    const double banked_light = light_on_top(std::atof(argv[6]));
    const double want = banked_light / level_light;
    std::printf("the sun: her tops lit %.3f level and %.3f banked, a ratio of %.3f\n",
                level_light, banked_light, want);
    // A light over 1 is clamped where the colour is white, and says nothing.
    if (level_light > 1.01 || banked_light > 1.01) {
        std::fprintf(stderr, "light_check: a light over 1 cannot be measured off a white "
                             "face; bank the other way\n");
        return 2;
    }
    if (want > 0.85) {
        std::fprintf(stderr, "light_check: the bank moves the sun's light on her tops by "
                             "too little to see (%.3f); bank further\n",
                     want);
        return 2;
    }

    const Tops a = tops(level, level_without);
    const Tops b = tops(banked, banked_without);
    std::printf("her tops: %ld pixels level, mean %.1f; %ld banked, mean %.1f\n", a.pixels,
                a.brightness, b.pixels, b.brightness);
    if (a.pixels < 200 || b.pixels < 200) {
        std::fprintf(stderr, "too little of the aeroplane drawn to measure: %ld and %ld "
                             "pixels\n",
                     a.pixels, b.pixels);
        return 1;
    }
    // The same faces in both: not much more or less of her seen.
    const double seen = static_cast<double>(b.pixels) / static_cast<double>(a.pixels);
    if (seen < 0.75 || seen > 1.33) {
        std::fprintf(stderr, "not the same faces seen: %ld pixels of her level and %ld "
                             "banked\n",
                     a.pixels, b.pixels);
        return 1;
    }
    const double got = b.brightness / a.brightness;
    std::printf("the light drawn: a ratio of %.3f, where the sun's is %.3f\n", got, want);
    if (std::abs(got / want - 1.0) > tolerance) {
        std::fprintf(stderr, "her light does not follow the bank: banked she is drawn %.3f "
                             "as bright as level, where the sun says %.3f\n",
                     got, want);
        return 1;
    }
    return 0;
}

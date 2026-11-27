// surfaces_check - the aeroplane drawn wears its livery, and its ailerons
// follow the stick.
//
//   glideslope_surfaces_check LEFT.bmp RIGHT.bmp WITHOUT.bmp R G B
//
// Three shots of one frame, from behind: the aeroplane with the stick held
// full left, with it held full right, and not drawn at all (--draw-aircraft
// off). Shot two ticks in, before the stick has banked her, so the stick
// moves nothing but her surfaces.
//
// **What is the aeroplane**: the pixels where either shot with it differs from
// the shot without - its outline and nothing else's, as view_check finds it.
//
// **The livery**: of those, the pixels whose colour is the livery's trim,
// R G B - lit, so darker or lighter, but of the same hue: the angle between the
// pixel's colour and the trim's, as vectors, is small, and the pixel is not
// black. The model's own materials are white, greys and black, nowhere near.
// At least a hundredth of the aeroplane must be the trim.
//
// **The ailerons**: the pixels of the aeroplane where the two stick shots
// differ. There must be some; they must lie out on the wings - nine in ten of
// them further from the aeroplane's middle than a third of its half-width,
// where the fuselage, fin and tail are not - and on both wings, a quarter of
// them at least on each side.
//
// Exits 0 when all three hold, 1 when one does not, and 2 when it cannot
// read what it was given.

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
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
        std::fprintf(stderr, "surfaces_check: cannot read %s: %s\n", path.c_str(),
                     SDL_GetError());
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr) {
        std::fprintf(stderr, "surfaces_check: cannot convert %s: %s\n", path.c_str(),
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

// How far apart two pixels' colours are: the sum of the channels' differences.
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

} // namespace

int main(int argc, char** argv) {
    if (argc != 7) {
        std::fprintf(stderr, "usage: glideslope_surfaces_check LEFT.bmp RIGHT.bmp "
                             "WITHOUT.bmp R G B\n");
        return 2;
    }
    Image left;
    Image right;
    Image without;
    if (!load(argv[1], left) || !load(argv[2], right) || !load(argv[3], without)) {
        return 2;
    }
    if (left.width != without.width || left.height != without.height ||
        right.width != without.width || right.height != without.height) {
        std::fprintf(stderr, "surfaces_check: the three shots are not one size\n");
        return 2;
    }
    const double trim[3] = {std::atof(argv[4]), std::atof(argv[5]), std::atof(argv[6])};
    const double trim_length = std::sqrt(trim[0] * trim[0] + trim[1] * trim[1] + trim[2] * trim[2]);
    if (!(trim_length > 0.0)) {
        std::fprintf(stderr, "surfaces_check: the trim's colour is black\n");
        return 2;
    }

    const std::size_t pixels =
        static_cast<std::size_t>(without.width) * static_cast<std::size_t>(without.height);
    std::vector<bool> aeroplane(pixels, false);
    long count = 0;
    double sum_x = 0.0;
    int least_x = without.width;
    int most_x = -1;
    for (std::size_t p = 0; p < pixels; ++p) {
        if (apart(left, without, p) > drawn || apart(right, without, p) > drawn) {
            aeroplane[p] = true;
            ++count;
            const int x = static_cast<int>(p % static_cast<std::size_t>(without.width));
            sum_x += x;
            least_x = std::min(least_x, x);
            most_x = std::max(most_x, x);
        }
    }
    if (count == 0) {
        std::fprintf(stderr, "no aeroplane drawn: the shots with it and without it agree\n");
        return 1;
    }
    const double middle = sum_x / static_cast<double>(count);
    const double half_width = 0.5 * (most_x - least_x);
    std::printf("the aeroplane: %ld pixels, x %d..%d, its middle at %.1f\n", count, least_x,
                most_x, middle);

    // The livery's trim, among the aeroplane's pixels of the stick-left shot.
    long trimmed = 0;
    for (std::size_t p = 0; p < pixels; ++p) {
        if (!aeroplane[p]) {
            continue;
        }
        double c[3];
        for (std::size_t i = 0; i < 3; ++i) {
            c[i] = left.rgba[p * 4 + i];
        }
        const double length = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
        if (length < 20.0) {
            continue; // black, which is every hue
        }
        const double cosine = (c[0] * trim[0] + c[1] * trim[1] + c[2] * trim[2]) /
                              (length * trim_length);
        if (cosine > 0.985) {
            ++trimmed;
        }
    }
    std::printf("the livery: %ld of its pixels in the trim's colour\n", trimmed);

    // What the stick moved.
    long moved = 0;
    long outboard = 0;
    long on_left = 0;
    long on_right = 0;
    for (std::size_t p = 0; p < pixels; ++p) {
        if (!aeroplane[p] || apart(left, right, p) <= drawn) {
            continue;
        }
        ++moved;
        const double x = static_cast<double>(p % static_cast<std::size_t>(without.width));
        if (std::abs(x - middle) > half_width / 3.0) {
            ++outboard;
        }
        (x < middle ? on_left : on_right) += 1;
    }
    std::printf("the stick moved %ld of its pixels: %ld out on the wings, %ld left of its "
                "middle and %ld right\n",
                moved, outboard, on_left, on_right);

    bool ok = true;
    if (trimmed * 100 < count) {
        std::fprintf(stderr, "no livery: %ld of the aeroplane's %ld pixels are its trim, "
                             "under a hundredth\n",
                     trimmed, count);
        ok = false;
    }
    if (moved < 10) {
        std::fprintf(stderr, "the ailerons did not follow the stick: %ld pixels moved\n", moved);
        ok = false;
    } else {
        if (outboard * 10 < moved * 9) {
            std::fprintf(stderr, "what the stick moved is not out on the wings: %ld of %ld\n",
                         outboard, moved);
            ok = false;
        }
        if (on_left * 4 < moved || on_right * 4 < moved) {
            std::fprintf(stderr, "the stick did not move both wings' ailerons: %ld left, "
                                 "%ld right\n",
                         on_left, on_right);
            ok = false;
        }
    }
    return ok ? 0 : 1;
}

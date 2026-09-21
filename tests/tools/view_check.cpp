// view_check - the aeroplane is drawn where the view's camera puts it.
//
//   glideslope_view_check WITH.bmp WITHOUT.bmp OUT.txt MODEL.mesh ALIGNMENT.txt
//                         [TOLERANCE_PX]
//
// **What was drawn.** The same frame is shot twice, once with the aeroplane
// and once with --draw-aircraft off. Every pixel that differs between them is
// one the aeroplane covered and nothing else did, so the two together give
// its outline exactly, with no guessing at which pixels are aeroplane and
// which are sky or ground.
//
// **Where it should be.** The client prints the camera it saw the frame from,
// and where it put the model, on its standard output at the shot. This reads
// those, reads the model from the same file the client drew, and projects
// every one of its vertices itself - the projection in gfx/scene.hpp worked
// through by hand, nothing shared with the renderer but the numbers. What
// comes out is where the outline should be.
//
// **What is held.** The two outlines' edges - leftmost, rightmost, topmost,
// bottommost - and their centres are held within a stated number of pixels.
// Projecting the vertices gives the outline of the whole model, which is what
// a solid aeroplane covers; it says nothing about which parts of it face the
// camera, and this does not pretend otherwise.
//
// Exits 0 when they agree, 1 when they do not, and 2 when it cannot read what
// it was given.

#include "gfx/model.hpp"
#include "gfx/scene.hpp"
#include "world/geodesy.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
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
        std::fprintf(stderr, "view_check: cannot read %s: %s\n", path.c_str(),
                     SDL_GetError());
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr) {
        std::fprintf(stderr, "view_check: cannot convert %s: %s\n", path.c_str(),
                     SDL_GetError());
        return false;
    }
    out.width = rgba->w;
    out.height = rgba->h;
    out.rgba.resize(static_cast<std::size_t>(rgba->w) *
                    static_cast<std::size_t>(rgba->h) * 4);
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

// The edges of a set of pixels, and where its middle is.
struct Outline {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    double centre_x = 0.0;
    double centre_y = 0.0;
    long count = 0;

    std::string say() const {
        char text[160];
        std::snprintf(text, sizeof text,
                      "x %d..%d, y %d..%d, centre %.1f,%.1f, %ld pixels", left,
                      right, top, bottom, centre_x, centre_y, count);
        return text;
    }
};

// The pixels of `with` that differ from `without`: what the aeroplane covered.
// A channel apart by more than `apart` counts, which lets a driver round a
// pixel differently without being taken for an aeroplane.
//
// **A patch of a handful of pixels is not part of an aeroplane.** Two runs of
// the same flight draw the same terrain - the shot waits for every tile - but
// they are two runs, and on some drivers a few pixels elsewhere come out
// differently: on Metal about 190 of them, scattered right across the frame,
// which dragged the outline from 95..225 out to 71..319 and failed a test
// that was measuring the right thing.
//
// So the pixels that differ are grouped into connected patches, and only
// those of four pixels or more are kept. An aeroplane seen from above is
// several patches - a wing here, a tailplane there, with ground between them
// - so taking the largest alone is wrong; but every one of them is a patch of
// substance, and a tile that landed a shade differently is not.
Outline drawn_outline(const Image& with, const Image& without, int apart) {
    const std::size_t wide = static_cast<std::size_t>(with.width);
    const std::size_t high = static_cast<std::size_t>(with.height);
    std::vector<char> differs(wide * high, 0);
    for (std::size_t i = 0; i < wide * high; ++i) {
        int most = 0;
        for (std::size_t c = 0; c < 3; ++c) {
            most = std::max(most, std::abs(static_cast<int>(with.rgba[i * 4 + c]) -
                                           static_cast<int>(without.rgba[i * 4 + c])));
        }
        differs[i] = most > apart ? 1 : 0;
    }

    // The biggest patch, found by walking each one from its first pixel.
    std::vector<char> seen(wide * high, 0);
    Outline best;
    std::vector<std::size_t> todo;
    for (std::size_t start = 0; start < wide * high; ++start) {
        if (differs[start] == 0 || seen[start] != 0) {
            continue;
        }
        Outline patch;
        patch.left = with.width;
        patch.top = with.height;
        patch.right = -1;
        patch.bottom = -1;
        double sum_x = 0.0;
        double sum_y = 0.0;
        todo.clear();
        todo.push_back(start);
        seen[start] = 1;
        while (!todo.empty()) {
            const std::size_t at = todo.back();
            todo.pop_back();
            const int x = static_cast<int>(at % wide);
            const int y = static_cast<int>(at / wide);
            patch.left = std::min(patch.left, x);
            patch.right = std::max(patch.right, x);
            patch.top = std::min(patch.top, y);
            patch.bottom = std::max(patch.bottom, y);
            sum_x += x;
            sum_y += y;
            ++patch.count;
            // Its four neighbours, and diagonally too: a thin wing drawn one
            // pixel at a time is still one wing.
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= with.width || ny >= with.height) {
                        continue;
                    }
                    const std::size_t next =
                        static_cast<std::size_t>(ny) * wide +
                        static_cast<std::size_t>(nx);
                    if (differs[next] != 0 && seen[next] == 0) {
                        seen[next] = 1;
                        todo.push_back(next);
                    }
                }
            }
        }
        constexpr long substance = 4;
        if (patch.count < substance) {
            continue;
        }
        if (best.count == 0) {
            best = patch;
            best.centre_x = sum_x;
            best.centre_y = sum_y;
            continue;
        }
        best.left = std::min(best.left, patch.left);
        best.right = std::max(best.right, patch.right);
        best.top = std::min(best.top, patch.top);
        best.bottom = std::max(best.bottom, patch.bottom);
        best.centre_x += sum_x;
        best.centre_y += sum_y;
        best.count += patch.count;
    }
    if (best.count == 0) {
        best.left = with.width;
        best.top = with.height;
        best.right = -1;
        best.bottom = -1;
    } else {
        best.centre_x /= static_cast<double>(best.count);
        best.centre_y /= static_cast<double>(best.count);
    }
    return best;
}

// The projection of gfx/scene.hpp, worked through by hand: a point in ECEF to
// the pixel it falls on, or nothing if it is behind the camera.
bool to_pixel(const glideslope::gfx::Camera& camera, int width, int height,
              const glideslope::world::Ecef& point, double& x, double& y) {
    const glideslope::world::Ecef from{point.x - camera.position.x,
                                       point.y - camera.position.y,
                                       point.z - camera.position.z};
    // The camera's axes are its columns: right, up and back. Its own
    // coordinates are the point's components along them.
    const auto& m = camera.world_from_camera.m;
    const double right = m[0] * from.x + m[1] * from.y + m[2] * from.z;
    const double up = m[3] * from.x + m[4] * from.y + m[5] * from.z;
    const double back = m[6] * from.x + m[7] * from.y + m[8] * from.z;
    const double ahead = -back;
    if (ahead <= 1e-6) {
        return false;
    }
    const double tan_half = std::tan(camera.vertical_fov_rad / 2.0);
    const double aspect = static_cast<double>(width) / static_cast<double>(height);
    x = (right / ahead / (tan_half * aspect) + 1.0) * 0.5 * width;
    y = (1.0 - up / ahead / tan_half) * 0.5 * height;
    return true;
}

// One "glideslope: <what> <numbers>" line of the client's output.
bool numbers_after(const std::string& text, const std::string& what,
                   std::vector<double>& out) {
    std::istringstream lines(text);
    std::string line;
    const std::string head = "glideslope: " + what + " ";
    while (std::getline(lines, line)) {
        if (line.rfind(head, 0) != 0) {
            continue;
        }
        std::istringstream words(line.substr(head.size()));
        out.clear();
        double v = 0.0;
        while (words >> v) {
            out.push_back(v);
        }
        return true;
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fputs("usage: glideslope_view_check WITH.bmp WITHOUT.bmp OUT.txt "
                   "MODEL.mesh ALIGNMENT.txt [TOLERANCE_PX]\n",
                   stderr);
        return 2;
    }
    const double tolerance = argc > 6 ? std::atof(argv[6]) : 2.0;

    Image with;
    Image without;
    if (!load(argv[1], with) || !load(argv[2], without)) {
        return 2;
    }
    if (with.width != without.width || with.height != without.height) {
        std::fputs("view_check: the two shots are not the same size\n", stderr);
        return 2;
    }

    std::ifstream said(argv[3], std::ios::binary);
    if (!said) {
        std::fprintf(stderr, "view_check: cannot read %s\n", argv[3]);
        return 2;
    }
    const std::string text((std::istreambuf_iterator<char>(said)),
                           std::istreambuf_iterator<char>());

    std::vector<double> camera_said;
    std::vector<double> aeroplane_said;
    if (!numbers_after(text, "camera", camera_said) || camera_said.size() != 15) {
        std::fputs("view_check: the shot said no camera\n", stderr);
        return 2;
    }
    if (!numbers_after(text, "aeroplane", aeroplane_said) ||
        aeroplane_said.size() != 12) {
        std::fputs("view_check: the shot said where no aeroplane was\n", stderr);
        return 2;
    }

    glideslope::gfx::Camera camera;
    camera.position = {camera_said[0], camera_said[1], camera_said[2]};
    for (std::size_t i = 0; i < 9; ++i) {
        camera.world_from_camera.m[i] = camera_said[3 + i];
    }
    camera.vertical_fov_rad = camera_said[12];
    const int width = static_cast<int>(camera_said[13]);
    const int height = static_cast<int>(camera_said[14]);
    if (width != with.width || height != with.height) {
        std::fprintf(stderr,
                     "view_check: the shot is %dx%d and the camera says %dx%d\n",
                     with.width, with.height, width, height);
        return 2;
    }

    glideslope::gfx::Placement placement;
    placement.origin = {aeroplane_said[0], aeroplane_said[1], aeroplane_said[2]};
    for (std::size_t i = 0; i < 9; ++i) {
        placement.world_from_local.m[i] = aeroplane_said[3 + i];
    }

    glideslope::gfx::Model model;
    try {
        model = glideslope::gfx::read_model(argv[4]);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "view_check: %s\n", e.what());
        return 2;
    }

    // Where the model should be: every vertex projected, and the edges of
    // what comes out. The alignment is already in the placement the client
    // printed, so the model's own coordinates are all that is left to move.
    Outline want;
    want.left = with.width;
    want.top = with.height;
    want.right = -1;
    want.bottom = -1;
    double sum_x = 0.0;
    double sum_y = 0.0;
    long behind = 0;
    for (const glideslope::gfx::ModelVertex& v : model.vertices) {
        const glideslope::world::Ecef local{static_cast<double>(v.position[0]),
                                            static_cast<double>(v.position[1]),
                                            static_cast<double>(v.position[2])};
        const glideslope::world::Ecef world =
            [&] {
                const glideslope::world::Ecef turned =
                    placement.world_from_local * local;
                return glideslope::world::Ecef{placement.origin.x + turned.x,
                                               placement.origin.y + turned.y,
                                               placement.origin.z + turned.z};
            }();
        double x = 0.0;
        double y = 0.0;
        if (!to_pixel(camera, with.width, with.height, world, x, y)) {
            ++behind;
            continue;
        }
        want.left = std::min(want.left, static_cast<int>(std::floor(x)));
        want.right = std::max(want.right, static_cast<int>(std::ceil(x)));
        want.top = std::min(want.top, static_cast<int>(std::floor(y)));
        want.bottom = std::max(want.bottom, static_cast<int>(std::ceil(y)));
        sum_x += x;
        sum_y += y;
        ++want.count;
    }
    if (want.count == 0) {
        std::fputs("view_check: none of the model is in front of the camera\n",
                   stderr);
        return 1;
    }
    want.centre_x = sum_x / static_cast<double>(want.count);
    want.centre_y = sum_y / static_cast<double>(want.count);

    const Outline drew = drawn_outline(with, without, 1);
    std::printf("drawn:     %s\n", drew.say().c_str());
    std::printf("projected: %s%s\n", want.say().c_str(),
                behind > 0 ? " (some of it behind the camera)" : "");

    if (drew.count == 0) {
        std::fputs("view_check: the aeroplane was not drawn at all\n", stderr);
        return 1;
    }

    // Only the part of the model in front of the camera can be drawn, and
    // only the part on screen can be seen, so the edges are held where both
    // agree there is one: inside the frame.
    struct Edge {
        const char* what;
        int drew;
        int want;
    };
    const Edge edges[] = {{"left", drew.left, want.left},
                          {"right", drew.right, want.right},
                          {"top", drew.top, want.top},
                          {"bottom", drew.bottom, want.bottom}};
    bool ok = true;
    for (const Edge& e : edges) {
        const bool clipped =
            e.want < 0 || e.want >= (std::string(e.what) == "left" ||
                                             std::string(e.what) == "right"
                                         ? with.width
                                         : with.height);
        if (clipped) {
            std::printf("  %-6s not held: the model reaches past the frame\n",
                        e.what);
            continue;
        }
        const int off = std::abs(e.drew - e.want);
        std::printf("  %-6s drawn %4d, projected %4d, %d px apart\n", e.what,
                    e.drew, e.want, off);
        if (static_cast<double>(off) > tolerance) {
            ok = false;
        }
    }
    const double centre_off =
        std::hypot(drew.centre_x - want.centre_x, drew.centre_y - want.centre_y);
    std::printf("  centre drawn %.1f,%.1f, projected %.1f,%.1f, %.1f px apart\n",
                drew.centre_x, drew.centre_y, want.centre_x, want.centre_y,
                centre_off);
    (void)0;
    // The centre of the pixels drawn is not the centre of the vertices
    // projected, and they are not meant to be the same: a model's vertices
    // crowd where it has detail, and only the side facing the camera is
    // drawn, so on a side view of the Cessna they sit 13 px apart on a
    // 320 px frame. It is held to a tenth of the frame, which catches an
    // aeroplane drawn somewhere else without pretending the two are one
    // number. The edges above are what pin it.
    const double centre_allowed = with.width / 10.0;
    if (centre_off > centre_allowed) {
        ok = false;
    }
    if (!ok) {
        std::fprintf(stderr,
                     "view_check: the aeroplane is not where the camera puts "
                     "it, beyond %.1f px\n",
                     tolerance);
        return 1;
    }
    return 0;
}

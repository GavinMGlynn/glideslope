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
    std::vector<int> where_x; // every pixel of it, for asking where they lie
    std::vector<int> where_y;

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
//
// **An edge is where the pixels are, not where the last one is.** Even within
// a patch, a sliver a pixel wide can run a long way: on Metal the aeroplane
// seen from ahead has one reaching the right of the frame, which put its
// right edge 94 px from where the model projects while its left edge was 1 px
// away and it had the same number of pixels as elsewhere. So an edge is taken
// at the hundredth of the pixels nearest it rather than at the outermost one.
// A wingtip is hundreds of pixels and moves an edge; a sliver is not and does
// not.
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
    std::vector<int> kept_x;
    std::vector<int> kept_y;
    std::vector<int> patch_x;
    std::vector<int> patch_y;
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
        patch_x.clear();
        patch_y.clear();
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
            patch_x.push_back(x);
            patch_y.push_back(y);
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
        kept_x.insert(kept_x.end(), patch_x.begin(), patch_x.end());
        kept_y.insert(kept_y.end(), patch_y.begin(), patch_y.end());
        best.centre_x += sum_x;
        best.centre_y += sum_y;
        best.count += patch.count;
    }
    if (best.count == 0) {
        best.left = with.width;
        best.top = with.height;
        best.right = -1;
        best.bottom = -1;
        return best;
    }
    best.centre_x /= static_cast<double>(best.count);
    best.centre_y /= static_cast<double>(best.count);
    best.left = *std::min_element(kept_x.begin(), kept_x.end());
    best.right = *std::max_element(kept_x.begin(), kept_x.end());
    best.top = *std::min_element(kept_y.begin(), kept_y.end());
    best.bottom = *std::max_element(kept_y.begin(), kept_y.end());
    best.where_x = kept_x;
    best.where_y = kept_y;
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
    std::vector<int> at_x;
    std::vector<int> at_y;
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
        at_x.push_back(static_cast<int>(std::lround(x)));
        at_y.push_back(static_cast<int>(std::lround(y)));
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
    want.left = *std::min_element(at_x.begin(), at_x.end());
    want.right = *std::max_element(at_x.begin(), at_x.end());
    want.top = *std::min_element(at_y.begin(), at_y.end());
    want.bottom = *std::max_element(at_y.begin(), at_y.end());

    const Outline drew = drawn_outline(with, without, 1);
    std::printf("drawn:     %s\n", drew.say().c_str());
    std::printf("projected: %s%s\n", want.say().c_str(),
                behind > 0 ? " (some of it behind the camera)" : "");

    if (drew.count == 0) {
        std::fputs("view_check: the aeroplane was not drawn at all\n", stderr);
        return 1;
    }

    // **Where it is, not where its last pixel is.** Comparing the two
    // outlines edge by edge sounds right and is not: one sliver a pixel wide
    // moves an edge as far as the frame is wide, and on Metal the aeroplane
    // seen from ahead has exactly that - the same number of pixels as
    // anywhere else, one thread of them running to the right of the frame,
    // and a right edge 94 px from where the model projects. Insetting both
    // outlines by a share of themselves was tried and is no better: a share
    // of the pixels drawn and a share of the vertices projected are not the
    // same distance, because a model's vertices crowd where it has detail.
    //
    // So what is held is that the aeroplane's pixels lie where the model
    // projects: nineteen in twenty of them inside the projected outline, with
    // `tolerance` of room around it for a rendered edge and a projected
    // vertex to land in different pixels. Every view here puts all of them
    // inside; the twentieth is room for the thread, which on Metal is nine
    // pixels of eight hundred. That catches an aeroplane drawn in the wrong
    // place, at the wrong size, or not at all, while a thread of stray pixels
    // costs a percent rather than a hundred pixels of edge.
    const int room = static_cast<int>(std::lround(tolerance));
    long inside = 0;
    for (std::size_t i = 0; i < drew.where_x.size(); ++i) {
        if (drew.where_x[i] >= want.left - room &&
            drew.where_x[i] <= want.right + room &&
            drew.where_y[i] >= want.top - room &&
            drew.where_y[i] <= want.bottom + room) {
            ++inside;
        }
    }
    const double share = static_cast<double>(inside) /
                         static_cast<double>(std::max<long>(drew.count, 1));
    std::printf("  %ld of %ld drawn pixels lie in the projected outline with "
                "%d px of room: %.2f%%\n",
                inside, drew.count, room, share * 100.0);
    bool ok = share >= 0.95;

    // ...and that it fills that outline, so that a speck in the right place
    // is not mistaken for an aeroplane. Half of each side, which the
    // narrowest of the six views - the Cessna seen from ahead - clears.
    const double wide = static_cast<double>(drew.right - drew.left + 1) /
                        static_cast<double>(std::max(want.right - want.left + 1, 1));
    const double high = static_cast<double>(drew.bottom - drew.top + 1) /
                        static_cast<double>(std::max(want.bottom - want.top + 1, 1));
    std::printf("  it fills %.0f%% of that outline across and %.0f%% down\n",
                wide * 100.0, high * 100.0);
    if (wide < 0.5 || high < 0.5) {
        ok = false;
    }

    const double centre_off =
        std::hypot(drew.centre_x - want.centre_x, drew.centre_y - want.centre_y);
    std::printf("  centre drawn %.1f,%.1f, projected %.1f,%.1f, %.1f px apart\n",
                drew.centre_x, drew.centre_y, want.centre_x, want.centre_y,
                centre_off);
    // The centre of the pixels drawn is not the centre of the vertices
    // projected, and they are not meant to be: a model's vertices crowd where
    // it has detail, and only the side facing the camera is drawn, which on a
    // side view of the Cessna is 13 px apart on a 320 px frame. A tenth of
    // the frame catches an aeroplane drawn somewhere else.
    if (centre_off > with.width / 10.0) {
        ok = false;
    }
    if (!ok) {
        std::fprintf(stderr,
                     "view_check: the aeroplane is not where the camera puts "
                     "it, with %.1f px of room\n",
                     tolerance);
        return 1;
    }
    return 0;
}

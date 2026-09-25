// interpolation_check - how far from where they were other aircraft were drawn.
//
//   glideslope_interpolation_check TRUTH SHOWN WORST_M [CONTROLS_WORST]
//
// TRUTH is the `--track` file of a client that heard every update, straight
// from the server; SHOWN is that of a client that predicted, through whatever
// the network did to it. Each frame SHOWN drew of an aircraft is held against
// where TRUTH heard that aircraft was at that session time, between the two
// updates either side of it; the distance between the two is that frame's
// error.
//
// It fails when any frame is WORST_M or more wrong, and when fewer than 95% of
// the frames drawn could be judged. Two kinds are left out, and each is
// counted and said:
//
// - **before the aircraft's first update reached SHOWN**. For its first 100 ms
//   an aircraft is held where it was first heard to be, rather than guessed
//   backwards (net/interpolation.cpp), so that it appears and then moves: a
//   choice, not an error of interpolating, and judged as one it would be a
//   hundred milliseconds of flight - four metres for a Cessna.
// - **where TRUTH has no update either side** no more than two updates apart:
//   before it started hearing, after it stopped, or where it lost one itself.
//
// With CONTROLS_WORST, **the watched aircraft's controls** are judged the same
// way: each frame SHOWN drew of them against TRUTH's, both watching the same
// aircraft, at that session time. It fails when any control is CONTROLS_WORST
// or more out, or fewer than 95% of the frames could be judged, or none were
// drawn.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct At {
    double t = 0.0;
    double x = 0.0, y = 0.0, z = 0.0;
};

// The watched controls at a moment: aileron, elevator, rudder, throttle, flaps.
struct Controls {
    double t = 0.0;
    std::array<double, 5> v{};
};

std::vector<Controls> read_controls(const std::string& file, const std::string& kind) {
    std::vector<Controls> out;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string what;
        unsigned index = 0;
        Controls c;
        if (words >> what >> c.t >> index >> c.v[0] >> c.v[1] >> c.v[2] >> c.v[3] >> c.v[4] &&
            what == kind) {
            out.push_back(c);
        }
    }
    std::sort(out.begin(), out.end(),
              [](const Controls& a, const Controls& b) { return a.t < b.t; });
    return out;
}

std::map<unsigned, std::vector<At>> read(const std::string& file, const std::string& kind) {
    std::map<unsigned, std::vector<At>> out;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string what;
        unsigned index = 0;
        At a;
        if (words >> what >> a.t >> index >> a.x >> a.y >> a.z && what == kind) {
            out[index].push_back(a);
        }
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4 && argc != 5) {
        std::fprintf(stderr, "usage: glideslope_interpolation_check TRUTH SHOWN WORST_M "
                             "[CONTROLS_WORST]\n");
        return 2;
    }
    const auto truth = read(argv[1], "heard");
    const auto shown = read(argv[2], "shown");
    const auto first_heard = read(argv[2], "heard");
    const double bound_m = std::strtod(argv[3], nullptr);
    // Two updates at 25 a second: one lost by the client that heard
    // everything is let through, and more is not judged across.
    constexpr double widest_s = 2.5 / 25.0;

    std::size_t drawn = 0, judged = 0, over = 0, appearing = 0;
    double worst = 0.0, sum = 0.0, worst_t = 0.0;
    unsigned worst_index = 0;
    for (const auto& [index, frames] : shown) {
        const auto path = truth.find(index);
        const auto first = first_heard.find(index);
        for (const At& f : frames) {
            ++drawn;
            if (first == first_heard.end() || f.t < first->second.front().t) {
                ++appearing;
                continue;
            }
            if (path == truth.end()) continue;
            const std::vector<At>& p = path->second;
            for (std::size_t i = 1; i < p.size(); ++i) {
                if (p[i - 1].t <= f.t && f.t <= p[i].t) {
                    const double span = p[i].t - p[i - 1].t;
                    if (span > widest_s) break;
                    const double k = span > 0.0 ? (f.t - p[i - 1].t) / span : 0.0;
                    const double x = p[i - 1].x + k * (p[i].x - p[i - 1].x);
                    const double y = p[i - 1].y + k * (p[i].y - p[i - 1].y);
                    const double z = p[i - 1].z + k * (p[i].z - p[i - 1].z);
                    const double error = std::hypot(f.x - x, f.y - y, f.z - z);
                    if (error > worst) {
                        worst = error;
                        worst_t = f.t;
                        worst_index = index;
                    }
                    sum += error;
                    if (error >= bound_m) ++over;
                    ++judged;
                    break;
                }
            }
        }
    }
    std::printf("interpolation: %zu of %zu frames drawn were judged (%zu before the "
                "aircraft's first update, held still by design; %zu with no update either side "
                "of them), the mean %.3f m, the worst %.3f m (aircraft %u at %.3f s), "
                "%zu at %.1f m or more\n",
                judged, drawn, appearing, drawn - judged - appearing, judged ? sum / static_cast<double>(judged) : 0.0,
                worst, worst_index, worst_t, over, bound_m);
    if (drawn == 0 || static_cast<double>(judged) < 0.95 * static_cast<double>(drawn)) {
        std::printf("interpolation: too few frames judged to say anything\n");
        return 1;
    }
    if (argc == 5) {
        const double controls_bound = std::strtod(argv[4], nullptr);
        const std::vector<Controls> heard = read_controls(argv[1], "watched");
        const std::vector<Controls> shown_controls = read_controls(argv[2], "showncontrols");
        std::size_t c_judged = 0, c_over = 0;
        double c_worst = 0.0;
        for (const Controls& f : shown_controls) {
            for (std::size_t i = 1; i < heard.size(); ++i) {
                if (heard[i - 1].t <= f.t && f.t <= heard[i].t) {
                    const double span = heard[i].t - heard[i - 1].t;
                    if (span > widest_s) break;
                    const double k = span > 0.0 ? (f.t - heard[i - 1].t) / span : 0.0;
                    double worst_here = 0.0;
                    for (std::size_t j = 0; j < 5; ++j) {
                        const double then =
                            heard[i - 1].v[j] + k * (heard[i].v[j] - heard[i - 1].v[j]);
                        worst_here = std::max(worst_here, std::abs(f.v[j] - then));
                    }
                    c_worst = std::max(c_worst, worst_here);
                    if (worst_here >= controls_bound) ++c_over;
                    ++c_judged;
                    break;
                }
            }
        }
        std::printf("controls: %zu of %zu frames drawn were judged, the worst %.4f, %zu at "
                    "%.3f or more\n",
                    c_judged, shown_controls.size(), c_worst, c_over, controls_bound);
        if (shown_controls.empty() ||
            static_cast<double>(c_judged) < 0.95 * static_cast<double>(shown_controls.size())) {
            std::printf("controls: too few frames judged to say anything\n");
            return 1;
        }
        if (c_over > 0) {
            return 1;
        }
    }
    return over == 0 ? 0 : 1;
}

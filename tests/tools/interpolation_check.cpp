// interpolation_check - how far from where they were other aircraft were drawn.
//
//   glideslope_interpolation_check TRUTH SHOWN WORST_M
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
    if (argc != 4) {
        std::fprintf(stderr, "usage: glideslope_interpolation_check TRUTH SHOWN WORST_M\n");
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
    return over == 0 ? 0 : 1;
}

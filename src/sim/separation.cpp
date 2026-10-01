#include "sim/separation.hpp"

#include "sim/plan.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace glideslope::sim {

namespace {

constexpr double metres_per_foot = 0.3048;
constexpr double radians = std::numbers::pi / 180.0;
// The rate the autopilot climbs and descends at unless asked otherwise
// (AutopilotModes::vertical_speed_fpm): the least an aircraft's heights over
// the lookahead are taken to move by.
constexpr double least_rate_fpm = 700.0;

// **The heights an aircraft may be at over the lookahead**: from where it is
// to the height it is flying to, but no further than it would climb or
// descend in that time; or, with no height to fly to, along its climb.
struct Band {
    double low_ft;
    double high_ft;
};

Band band_of(const Traffic& t, std::optional<double> held_ft) {
    const double reach_ft =
        std::max(std::abs(t.climb_fpm), least_rate_fpm) * Separation::lookahead_s / 60.0;
    const double end = held_ft ? std::clamp(*held_ft, t.altitude_ft - reach_ft,
                                            t.altitude_ft + reach_ft)
                               : t.altitude_ft + t.climb_fpm * Separation::lookahead_s / 60.0;
    return {std::min(t.altitude_ft, end), std::max(t.altitude_ft, end)};
}

std::optional<double> limited(std::optional<double> held_ft, const HeightLimit& limit) {
    if (!held_ft) {
        return held_ft;
    }
    double h = *held_ft;
    if (limit.floor_ft) {
        h = std::max(h, *limit.floor_ft);
    }
    if (limit.ceiling_ft) {
        h = std::min(h, *limit.ceiling_ft);
    }
    return h;
}

} // namespace

double horizontal_m(const Traffic& a, const Traffic& b) {
    return distance_m(a.latitude_deg, a.longitude_deg, b.latitude_deg, b.longitude_deg);
}

bool near(const Traffic& a, const Traffic& b) {
    const double d = horizontal_m(a, b);
    if (d < Separation::guard_m) {
        return true;
    }
    // Where b is from a, north and east, and how it moves from a.
    const double towards = bearing_deg(a.latitude_deg, a.longitude_deg, b.latitude_deg,
                                       b.longitude_deg) *
                           radians;
    const double rn = d * std::cos(towards);
    const double re = d * std::sin(towards);
    const double vn = (b.north_fps - a.north_fps) * metres_per_foot;
    const double ve = (b.east_fps - a.east_fps) * metres_per_foot;
    const double v2 = vn * vn + ve * ve;
    if (v2 <= 0.0) {
        return false;
    }
    const double t = std::clamp(-(rn * vn + re * ve) / v2, 0.0, Separation::lookahead_s);
    return std::hypot(rn + vn * t, re + ve * t) < Separation::guard_m;
}

std::vector<HeightLimit> separate(const std::vector<Traffic>& traffic) {
    const std::size_t n = traffic.size();
    std::vector<HeightLimit> limits(n);
    // The height each is flying to, as limited so far.
    std::vector<std::optional<double>> held(n);
    for (std::size_t i = 0; i < n; ++i) {
        held[i] = traffic[i].held_ft;
    }
    const double apart_ft = Separation::minimum_ft + Separation::margin_ft;
    for (std::size_t i = 0; i < n; ++i) {
        const Traffic& g = traffic[i];
        if (!g.gives_way) {
            continue;
        }
        HeightLimit& limit = limits[i];
        std::optional<std::size_t> floor_for;
        std::optional<std::size_t> ceiling_for;
        for (std::size_t j = 0; j < n; ++j) {
            const Traffic& p = traffic[j];
            // Only to those it gives way to: every one that does not, and
            // every one before it that does.
            if (j == i || (p.gives_way && j > i) || !near(g, p)) {
                continue;
            }
            const Band band = band_of(p, held[j]);
            // Which side it keeps to: the side it is on - or, within 50 ft
            // of level with the other, the side of it the height it is
            // flying to is on, and below if that is level too. Not which
            // way it is going: held off, it goes one way and then the
            // other, and the side with it.
            const double dh = g.altitude_ft - p.altitude_ft;
            bool above = dh > 0.0;
            if (std::abs(dh) < 50.0) {
                above = g.held_ft && *g.held_ft > p.altitude_ft;
            }
            const double ceiling = band.low_ft - apart_ft;
            if (!above && ceiling < g.ground_ft + Separation::least_above_ground_ft) {
                above = true; // not pushed into the ground: over it instead
            }
            if (above) {
                const double floor = band.high_ft + apart_ft;
                if (!limit.floor_ft || floor > *limit.floor_ft) {
                    limit.floor_ft = floor;
                    floor_for = j;
                }
            } else if (!limit.ceiling_ft || ceiling < *limit.ceiling_ft) {
                limit.ceiling_ft = ceiling;
                ceiling_for = j;
            }
        }
        // Held between two that leave no room: the side that asks less of it.
        if (limit.floor_ft && limit.ceiling_ft && *limit.floor_ft > *limit.ceiling_ft) {
            if (*limit.floor_ft - g.altitude_ft <= g.altitude_ft - *limit.ceiling_ft) {
                limit.ceiling_ft.reset();
                ceiling_for.reset();
            } else {
                limit.floor_ft.reset();
                floor_for.reset();
            }
        }
        limit.clear_of = ceiling_for ? ceiling_for : floor_for;
        held[i] = limited(held[i], limit);
    }
    return limits;
}

} // namespace glideslope::sim

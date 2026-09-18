#include "gfx/terrain_colour.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace glideslope::gfx {

namespace {

constexpr double radians = 3.14159265358979323846 / 180.0;

struct Stop {
    double height_m;
    std::array<double, 3> colour;
};

// Hypsometric tints, lowest first.
constexpr std::array<Stop, 6> tints{{
    {0.0, {0.28, 0.45, 0.22}},
    {300.0, {0.42, 0.52, 0.28}},
    {1000.0, {0.55, 0.47, 0.33}},
    {2000.0, {0.52, 0.48, 0.45}},
    {2800.0, {0.70, 0.70, 0.70}},
    {3500.0, {0.95, 0.95, 0.97}},
}};

constexpr std::array<double, 3> sea{0.12, 0.26, 0.45};

} // namespace

world::Ecef up_at(double latitude_deg, double longitude_deg) {
    const double phi = latitude_deg * radians;
    const double lambda = longitude_deg * radians;
    return {std::cos(phi) * std::cos(lambda), std::cos(phi) * std::sin(lambda),
            std::sin(phi)};
}

std::array<float, 4> terrain_colour(double height_above_sea_level_m,
                                    const world::Ecef& normal, const world::Ecef& up) {
    if (height_above_sea_level_m <= 0.0) {
        return {static_cast<float>(sea[0]), static_cast<float>(sea[1]),
                static_cast<float>(sea[2]), 1.0f};
    }
    std::array<double, 3> tint = tints.back().colour;
    for (std::size_t i = 1; i < tints.size(); ++i) {
        if (height_above_sea_level_m < tints[i].height_m) {
            const Stop& a = tints[i - 1];
            const Stop& b = tints[i];
            const double t =
                (height_above_sea_level_m - a.height_m) / (b.height_m - a.height_m);
            for (std::size_t c = 0; c < 3; ++c) {
                tint[c] = a.colour[c] + t * (b.colour[c] - a.colour[c]);
            }
            break;
        }
    }

    // The sun, in the local east-north-up frame: from azimuth 315 (north-west),
    // 45 degrees up. East is up x the pole, and north completes the frame; at a
    // pole, where that is undefined, east is taken along x.
    world::Ecef east{-up.y, up.x, 0.0};
    double east_length = std::sqrt(east.x * east.x + east.y * east.y);
    if (east_length < 1e-9) {
        east = {1.0, 0.0, 0.0};
        east_length = 1.0;
    }
    east = {east.x / east_length, east.y / east_length, 0.0};
    const world::Ecef north{up.y * east.z - up.z * east.y,
                            up.z * east.x - up.x * east.z,
                            up.x * east.y - up.y * east.x};
    const double s = std::sqrt(0.5);
    const double sun_east = -s * s;
    const double sun_north = s * s;
    const double sun_up = s;
    const double lit = std::max(
        0.0, normal.x * (sun_east * east.x + sun_north * north.x + sun_up * up.x) +
                 normal.y * (sun_east * east.y + sun_north * north.y + sun_up * up.y) +
                 normal.z * (sun_east * east.z + sun_north * north.z + sun_up * up.z));
    const double light = 0.35 + 0.65 * lit / sun_up; // level ground at full light
    const auto channel = [&](double c) {
        return static_cast<float>(std::clamp(c * light, 0.0, 1.0));
    };
    return {channel(tint[0]), channel(tint[1]), channel(tint[2]), 1.0f};
}

} // namespace glideslope::gfx

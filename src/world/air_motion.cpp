#include "world/air_motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace glideslope::world {

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double metres_per_foot = 0.3048;

std::uint64_t mix(std::uint64_t z) {
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// Value noise: at every multiple of `period` a number from the seed, and
// between them a smooth step from one to the next.
double noise(std::uint64_t seed, std::uint64_t octave, double s, double period) {
    const double x = s / period;
    const double cell = std::floor(x);
    const double f = x - cell;
    const auto i = static_cast<std::uint64_t>(static_cast<std::int64_t>(cell));
    const double a = seeded_uniform(seed, octave, i);
    const double b = seeded_uniform(seed, octave, i + 1);
    const double t = f * f * (3.0 - 2.0 * f);
    return a + (b - a) * t;
}

// MIL-F-8785C, figure 7: the turbulence intensity above 2,000 ft, in ft/s, for
// each curve of the probability of exceedance (rows 1 to 7) at each altitude.
constexpr std::array<double, 12> poe_altitudes_ft{500.0,   1750.0,  3750.0,  7500.0,
                                                  15000.0, 25000.0, 35000.0, 45000.0,
                                                  55000.0, 65000.0, 75000.0, 80000.0};
constexpr std::array<std::array<double, 12>, 7> poe_sigma_fps{{
    {3.2, 2.2, 1.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {4.2, 3.6, 3.3, 1.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {6.6, 6.9, 7.4, 6.7, 4.6, 2.7, 0.4, 0.0, 0.0, 0.0, 0.0, 0.0},
    {8.6, 9.6, 10.6, 10.1, 8.0, 6.6, 5.0, 4.2, 2.7, 0.0, 0.0, 0.0},
    {11.8, 13.0, 16.0, 15.1, 11.6, 9.7, 8.1, 8.2, 7.9, 4.9, 3.2, 2.1},
    {15.6, 17.6, 23.0, 23.6, 22.1, 20.0, 16.0, 15.1, 12.1, 7.9, 6.2, 5.1},
    {18.7, 21.5, 28.4, 30.2, 30.7, 31.0, 25.2, 23.1, 17.5, 10.7, 8.4, 7.2},
}};

// The table's intensity at an altitude, linear between its columns and held
// at its ends.
double poe_sigma_mps(int severity, double altitude_ft) {
    const auto& row = poe_sigma_fps[static_cast<std::size_t>(severity - 1)];
    if (altitude_ft <= poe_altitudes_ft.front()) {
        return row.front() * metres_per_foot;
    }
    for (std::size_t i = 1; i < poe_altitudes_ft.size(); ++i) {
        if (altitude_ft <= poe_altitudes_ft[i]) {
            const double t = (altitude_ft - poe_altitudes_ft[i - 1]) /
                             (poe_altitudes_ft[i] - poe_altitudes_ft[i - 1]);
            return (row[i - 1] + t * (row[i] - row[i - 1])) * metres_per_foot;
        }
    }
    return row.back() * metres_per_foot;
}

// The turbulence's waves: fixed wavelengths from 50 km down to 2 m, the same
// at every height so the pattern changes smoothly with height, and only their
// amplitudes follow the spectrum there.
constexpr int waves = 48;
constexpr double shortest_m = 2.0;
constexpr double longest_m = 50000.0;

// Dryden's one-sided spectra, in rad/m, each integrating to sigma squared: the
// first-order form along the wind, the second-order form across it and
// vertically.
double dryden_first(double omega, double length, double sigma) {
    const double x = length * omega;
    return sigma * sigma * (2.0 * length / pi) / (1.0 + x * x);
}

double dryden_second(double omega, double length, double sigma) {
    const double x = length * omega;
    const double d = 1.0 + x * x;
    return sigma * sigma * (length / pi) * (1.0 + 3.0 * x * x) / (d * d);
}

// Waves in every direction cross a line at an angle, so along it they are
// longer than they are, and the turbulence stays correlated further than
// Dryden's length. Each wave is shortened by the factor that, averaged over
// seeds and lines, brings the distance at which its correlation falls to 1/e
// back to Dryden's length: measured, 2.47 for the first-order form and 1.40
// for the second (tests/unit/test_air_motion.cpp holds it there).
constexpr double first_order_stretch = 2.47;
constexpr double second_order_stretch = 1.40;

// One component of the turbulence at a point: its waves summed, scaled so that
// its variance at every point is sigma squared.
double component(std::uint64_t seed, std::uint64_t which, bool first_order,
                 double length, double sigma, const Enu& p) {
    if (sigma <= 0.0 || length <= 0.0) {
        return 0.0;
    }
    const double lowest = 2.0 * pi / longest_m;
    const double ratio = std::pow(longest_m / shortest_m, 1.0 / waves);
    double sum = 0.0;
    double variance = 0.0;
    double edge = lowest;
    for (int i = 0; i < waves; ++i) {
        const double next = edge * ratio;
        const auto index = which * 1000 + static_cast<std::uint64_t>(i);
        const double omega = edge + (next - edge) * seeded_uniform(seed, index, 1);
        const double power = first_order ? dryden_first(omega, length, sigma)
                                         : dryden_second(omega, length, sigma);
        const double amplitude_squared = 2.0 * power * (next - edge);
        // A direction uniform over the sphere, and a phase.
        const double z = 2.0 * seeded_uniform(seed, index, 2) - 1.0;
        const double azimuth = 2.0 * pi * seeded_uniform(seed, index, 3);
        const double across = std::sqrt(std::max(0.0, 1.0 - z * z));
        const double along = across * std::cos(azimuth) * p.east +
                             across * std::sin(azimuth) * p.north + z * p.up;
        const double phase = 2.0 * pi * seeded_uniform(seed, index, 4);
        const double stretch = first_order ? first_order_stretch : second_order_stretch;
        sum += std::sqrt(amplitude_squared) * std::cos(stretch * omega * along + phase);
        variance += amplitude_squared / 2.0;
        edge = next;
    }
    return variance > 0.0 ? sum * sigma / std::sqrt(variance) : 0.0;
}

} // namespace

double seeded_uniform(std::uint64_t seed, std::uint64_t a, std::uint64_t b) {
    const std::uint64_t z = mix(mix(mix(seed) ^ a) ^ (b * 0xd6e8feb86659fd93ULL));
    // The top 53 bits, exactly as a double.
    return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
}

double gust_factor(std::uint64_t seed, double pattern_s) {
    const double n = 0.5 * noise(seed, 1, pattern_s, 16.0) +
                     0.3 * noise(seed, 2, pattern_s, 8.0) +
                     0.2 * noise(seed, 3, pattern_s, 4.0);
    // Spread the middle half of the noise over the whole range, so the wind
    // spends time at its mean and at its gust, not only between.
    return std::clamp((n - 0.25) / 0.5, 0.0, 1.0);
}

DrydenScales dryden_scales(int severity, double height_above_ground_m,
                           double wind_at_20ft_mps) {
    DrydenScales s;
    if (severity <= 0) {
        return s;
    }
    severity = std::min(severity, 7);
    // MIL-F-8785C's model is in feet; below 10 ft it is held at 10.
    const double h = std::max(10.0, height_above_ground_m / metres_per_foot);
    const double low_sigma_w = 0.1 * wind_at_20ft_mps;
    if (h <= 1000.0) {
        const double k = 0.177 + 0.000823 * h;
        s.length_u_m = h / std::pow(k, 1.2) * metres_per_foot;
        s.length_w_m = h * metres_per_foot;
        s.sigma_w_mps = low_sigma_w;
        s.sigma_u_mps = low_sigma_w / std::pow(k, 0.4);
    } else if (h <= 2000.0) {
        const double t = (h - 1000.0) / 1000.0;
        s.length_u_m = s.length_w_m = (1000.0 + t * 750.0) * metres_per_foot;
        s.sigma_u_mps = s.sigma_w_mps =
            low_sigma_w + t * (poe_sigma_mps(severity, h) - low_sigma_w);
    } else {
        s.length_u_m = s.length_w_m = 1750.0 * metres_per_foot;
        s.sigma_u_mps = s.sigma_w_mps = poe_sigma_mps(severity, h);
    }
    return s;
}

Enu turbulence(std::uint64_t seed, const DrydenScales& scales, const Enu& position,
               const Enu& along) {
    const double u =
        component(seed, 1, true, scales.length_u_m, scales.sigma_u_mps, position);
    const double v =
        component(seed, 2, false, scales.length_u_m, scales.sigma_u_mps, position);
    const double w =
        component(seed, 3, false, scales.length_w_m, scales.sigma_w_mps, position);
    // Along the wind, across it to its left, and up.
    return {u * along.east - v * along.north, u * along.north + v * along.east, w};
}

int severity_from_gust_spread(double spread_kt) {
    if (spread_kt >= 30.0) {
        return 5;
    }
    if (spread_kt >= 20.0) {
        return 4;
    }
    if (spread_kt >= 15.0) {
        return 3;
    }
    if (spread_kt >= 10.0) {
        return 2;
    }
    if (spread_kt >= 5.0) {
        return 1;
    }
    return 0;
}

} // namespace glideslope::world

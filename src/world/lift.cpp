#include "world/lift.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace glideslope::world {

namespace {

constexpr double pi = 3.14159265358979323846;

// A thermal's life, and how long it takes to grow and to fade.
constexpr double thermal_life_s = 20.0 * 60.0;
constexpr double thermal_ramp = 3.0 / 20.0; // of its life, at each end
// How alive a thermal is on average over its life: its prime, and half of
// each ramp.
constexpr double mean_life = 1.0 - thermal_ramp;
// How much stronger or weaker than Allen's mean a thermal may be.
constexpr double least_gain = 0.7;
constexpr double most_gain = 1.3;
// Salted so the thermals are drawn apart from the gusts and turbulence the
// same seed draws.
constexpr std::uint64_t thermal_salt = 0x7468726d616c73ULL; // "thermals"

// Allen's shape constants for his bell (his table 3, as his code has them):
// r1/r2, then k1, k2, k3 and the linear term his code takes from its fourth
// column.
struct Shape {
    double r1r2;
    double ka;
    double kb;
    double kc;
    double kd;
};
constexpr std::array<Shape, 7> shapes{{
    {0.14, 1.5352, 2.5826, -0.0113, -0.1950},
    {0.25, 1.5265, 3.6054, -0.0176, -0.1265},
    {0.36, 1.4866, 4.8356, -0.0320, -0.0818},
    {0.47, 1.2042, 7.7904, 0.0848, -0.0445},
    {0.58, 0.8816, 13.9720, 0.3404, -0.0216},
    {0.69, 0.7067, 23.9940, 0.5689, -0.0099},
    {0.80, 0.6189, 42.7965, 0.7157, -0.0033},
}};

// The shape whose r1/r2 is nearest, as Allen's code picks it.
const Shape& shape_for(double r1r2) {
    for (std::size_t i = 0; i + 1 < shapes.size(); ++i) {
        if (r1r2 < 0.5 * (shapes[i].r1r2 + shapes[i + 1].r1r2)) {
            return shapes[i];
        }
    }
    return shapes.back();
}

// Allen's mean radius before its 10 m floor.
double mean_radius(const Convection& c, double height_m) {
    const double zzi = height_m / c.depth_m;
    return 0.102 * std::cbrt(zzi) * (1.0 - 0.25 * zzi) * c.depth_m;
}

double smoothstep(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// The index a cell's thermals are drawn with.
std::uint64_t cell_index(std::int64_t column, std::int64_t row) {
    return static_cast<std::uint64_t>(column) * 0x9e3779b97f4a7c15ULL ^
           static_cast<std::uint64_t>(row);
}

// The thermal of one cell at a time.
Thermal cell_thermal(std::uint64_t seed, double spacing, std::int64_t column,
                     std::int64_t row, double time_s) {
    const std::uint64_t s = seed ^ thermal_salt;
    const std::uint64_t cell = cell_index(column, row);
    // Each cell's generations are out of step with its neighbours'.
    const double age = time_s / thermal_life_s + seeded_uniform(s, cell, 0);
    const double generation = std::floor(age);
    const double f = age - generation;
    const auto g = static_cast<std::uint64_t>(static_cast<std::int64_t>(generation));
    Thermal t;
    t.east =
        (static_cast<double>(column) + seeded_uniform(s, cell, 4 * g + 1)) * spacing;
    t.north = (static_cast<double>(row) + seeded_uniform(s, cell, 4 * g + 2)) * spacing;
    t.gain = least_gain + (most_gain - least_gain) * seeded_uniform(s, cell, 4 * g + 3);
    t.life =
        std::min(smoothstep(f / thermal_ramp), smoothstep((1.0 - f) / thermal_ramp));
    return t;
}

// The terrain's transform: its samples, and as many again of level ground
// beyond them, so that the transform's copies of the terrain - it takes it to
// repeat - stand twice the samples' length away rather than once, too far to
// be felt. Where they stood once away, a mountain's own waves, whose slopes
// fall off only as the square of the distance, were a fifth stronger.
constexpr int transform_size = 2 * terrain_samples;

// Its twiddle factors, cos and sin of 2 pi q / transform_size.
struct Twiddles {
    std::array<double, transform_size> cos{};
    std::array<double, transform_size> sin{};
    Twiddles() {
        for (int q = 0; q < transform_size; ++q) {
            const double a = 2.0 * pi * q / transform_size;
            cos[static_cast<std::size_t>(q)] = std::cos(a);
            sin[static_cast<std::size_t>(q)] = std::sin(a);
        }
    }
};

} // namespace

Convection convection(double surface_temperature_c, double ground_m,
                      double surface_wind_mps,
                      const std::function<double(double height_msl_m)>& environment_c) {
    constexpr double excess_c = 1.0;
    constexpr double dry_lapse_c_per_m = 0.0098;
    constexpr double step_m = 10.0;
    constexpr double highest_m = 4000.0;
    constexpr double shallowest_m = 300.0;
    constexpr double windiest_mps = 25.0 * 1852.0 / 3600.0;

    double warmer = surface_temperature_c + excess_c - environment_c(ground_m);
    if (warmer <= 0.0) {
        return {};
    }
    double depth = highest_m;
    for (double h = step_m; h <= highest_m; h += step_m) {
        const double next = surface_temperature_c + excess_c - dry_lapse_c_per_m * h -
                            environment_c(ground_m + h);
        if (next <= 0.0) {
            // Where the parcel is as warm as the air, between the steps.
            depth = h - step_m + step_m * warmer / (warmer - next);
            break;
        }
        warmer = next;
    }
    if (depth < shallowest_m) {
        return {};
    }
    Convection c;
    c.depth_m = depth;
    c.velocity_mps =
        surface_wind_mps > windiest_mps ? 0.0 : 2.0 * std::cbrt(depth / 1500.0);
    return c;
}

double allen_mean_updraught(const Convection& c, double height_m) {
    if (c.depth_m <= 0.0 || height_m <= 0.0) {
        return 0.0;
    }
    const double zzi = height_m / c.depth_m;
    return std::cbrt(zzi) * (1.0 - 1.1 * zzi) * c.velocity_mps;
}

double allen_radius(const Convection& c, double height_m) {
    if (c.depth_m <= 0.0 || height_m <= 0.0) {
        return 10.0;
    }
    return std::max(10.0, mean_radius(c, height_m));
}

double thermal_spacing_m(const Convection& c) {
    // Allen's count, N = 0.6 X Y / (zi r2), is one updraft to zi r2 / 0.6.
    return std::sqrt(c.depth_m * mean_radius(c, 0.4 * c.depth_m) / 0.6);
}

double environment_sink(const Convection& c, double height_m) {
    if (c.depth_m <= 0.0 || c.velocity_mps <= 0.0 || height_m <= 0.0) {
        return 0.0;
    }
    const double zzi = height_m / c.depth_m;
    const double rbar = mean_radius(c, height_m);
    const double wtbar = allen_mean_updraught(c, height_m);
    const double swd = zzi > 0.5 && zzi <= 0.9 ? 2.5 * (zzi - 0.5) : 0.0;
    const double spacing = thermal_spacing_m(c);
    const double area = spacing * spacing;
    const double updrafts = mean_life * pi * rbar * rbar;
    return std::min(0.0, -(updrafts * wtbar * (1.0 - swd)) / (area - updrafts));
}

double allen_updraught(const Convection& c, double height_m, double distance_m,
                       double gain, double sink_mps) {
    if (c.depth_m <= 0.0 || c.velocity_mps <= 0.0 || height_m <= 0.0) {
        return 0.0;
    }
    const double z = height_m;
    const double zi = c.depth_m;
    const double zzi = z / zi;
    const double r2 = allen_radius(c, z);
    const double r1r2 = r2 < 600.0 ? 0.0011 * r2 + 0.14 : 0.8;
    const double r1 = r1r2 * r2;
    const double wt = allen_mean_updraught(c, z) * gain;
    const double wc =
        (3.0 * wt * (r2 * r2 * r2 - r2 * r2 * r1)) / (r2 * r2 * r2 - r1 * r1 * r1);
    const double r = distance_m;
    const double rr2 = r / r2;

    // The bell.
    double ws = 0.0;
    if (z < zi) {
        const Shape& k = shape_for(r1r2);
        ws = 1.0 / (1.0 + std::pow(k.ka * std::abs(rr2 + k.kc), k.kb)) + k.kd * rr2;
        ws = std::max(ws, 0.0);
    }

    // The ring of sinking air at its edge, in the layer's upper half.
    const double w1 = r > r1 && rr2 < 2.0 ? (pi / 6.0) * std::sin(pi * rr2) : 0.0;
    double wd = 0.0;
    if (zzi > 0.5 && zzi <= 0.9) {
        wd = std::min(0.0, 2.5 * (zzi - 0.5) * w1);
    }
    const double w2 = ws * wc + wd * wt;

    // Outside its core, stretched to meet the sink between updrafts.
    if (r > r1) {
        if (wc == 0.0) {
            return sink_mps;
        }
        return w2 * (1.0 - sink_mps / wc) + sink_mps;
    }
    return w2;
}

Thermal thermal_in_cell(std::uint64_t seed, const Convection& c, double east,
                        double north, double time_s) {
    if (c.depth_m <= 0.0 || c.velocity_mps <= 0.0) {
        return {};
    }
    const double spacing = thermal_spacing_m(c);
    const auto column = static_cast<std::int64_t>(std::floor(east / spacing));
    const auto row = static_cast<std::int64_t>(std::floor(north / spacing));
    return cell_thermal(seed, spacing, column, row, time_s);
}

double thermal_updraught(std::uint64_t seed, const Convection& c, const Enu& position,
                         double time_s) {
    if (c.depth_m <= 0.0 || c.velocity_mps <= 0.0 || position.up <= 0.0 ||
        position.up >= c.depth_m) {
        return 0.0;
    }
    const double z = position.up;
    const double sink = environment_sink(c, z);
    const double spacing = thermal_spacing_m(c);
    const auto column = static_cast<std::int64_t>(std::floor(position.east / spacing));
    const auto row = static_cast<std::int64_t>(std::floor(position.north / spacing));
    // An updraft reaches no further than twice its radius, never as far as a
    // cell's width: the thermals of the cells about this one are all that can.
    double w = sink;
    for (std::int64_t dr = -1; dr <= 1; ++dr) {
        for (std::int64_t dc = -1; dc <= 1; ++dc) {
            const Thermal t =
                cell_thermal(seed, spacing, column + dc, row + dr, time_s);
            if (t.life <= 0.0) {
                continue;
            }
            const double distance =
                std::hypot(position.east - t.east, position.north - t.north);
            w += t.life * (allen_updraught(c, z, distance, t.gain, sink) - sink);
        }
    }
    return w;
}

double terrain_updraught(const TerrainAlong& terrain, double height_msl_m,
                         double speed_mps, double buoyancy_frequency) {
    constexpr int n = terrain_samples;
    constexpr int half = n / 2;
    constexpr int size = transform_size;
    constexpr int taper = 32; // samples: 8 km
    constexpr double ground_fade_m = 500.0;
    if (speed_mps < 1.0) {
        return 0.0;
    }
    static const Twiddles twiddles;

    // The samples, from the farthest upwind to the farthest downwind less one;
    // the aircraft is over the middle one.
    std::array<double, n> h{};
    for (int j = 0; j < n; ++j) {
        h[static_cast<std::size_t>(j)] = terrain((j - half) * terrain_sample_spacing_m);
    }
    const double beneath = h[half];
    std::array<double, n> sorted = h;
    std::nth_element(sorted.begin(), sorted.begin() + half, sorted.end());
    const double reference = sorted[half];
    for (int j = 0; j < n; ++j) {
        const int from_end = std::min(j, n - 1 - j);
        const double weight =
            from_end >= taper ? 1.0 : 0.5 * (1.0 - std::cos(pi * from_end / taper));
        h[static_cast<std::size_t>(j)] =
            (h[static_cast<std::size_t>(j)] - reference) * weight;
    }

    // The height in the waves: above the median, but near the ground above the
    // ground beneath.
    const double above_ground = std::max(0.0, height_msl_m - beneath);
    const double base =
        reference + (beneath - reference) * std::exp(-above_ground / ground_fade_m);
    const double z = std::max(0.0, height_msl_m - base);
    const double l = buoyancy_frequency > 0.0 ? buoyancy_frequency / speed_mps : 0.0;

    // The slope of the displaced air under the aircraft, summed over the
    // terrain's wavenumbers - each positive one with its negative, the
    // complex conjugate - except the mean, which has none, and the highest,
    // which is not seen. In the transform the aircraft is at its start: the
    // samples downwind follow it, and those upwind wrap round to its end.
    double slope = 0.0;
    for (int k = 1; k < size / 2; ++k) {
        double re = 0.0;
        double im = 0.0;
        for (int j = 0; j < n; ++j) {
            const int at = (j - half + size) & (size - 1);
            const auto q = static_cast<std::size_t>((at * k) & (size - 1));
            re += h[static_cast<std::size_t>(j)] * twiddles.cos[q];
            im -= h[static_cast<std::size_t>(j)] * twiddles.sin[q];
        }
        const double kappa = 2.0 * pi * k / (size * terrain_sample_spacing_m);
        double er = 0.0;
        double ei = 0.0;
        if (kappa < l) {
            const double m = std::sqrt(l * l - kappa * kappa);
            er = std::cos(m * z);
            ei = std::sin(m * z);
        } else {
            er = std::exp(-std::sqrt(kappa * kappa - l * l) * z);
        }
        // The real part of i kappa h(k) e^(i m z).
        slope -= kappa * (re * ei + im * er);
    }
    return speed_mps * slope * 2.0 / size;
}

} // namespace glideslope::world

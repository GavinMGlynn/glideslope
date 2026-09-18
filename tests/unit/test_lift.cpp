#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/test_pilot.hpp"
#include "sim/weather.hpp"

#include "world/lift.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::world::Convection;
using glideslope::world::Enu;

namespace {

constexpr double pi = 3.14159265358979323846;

// Allen's example: his mean convective scales from Desert Rock, Nevada (his
// table 1), with which his figures 10 to 12 were drawn.
constexpr Convection allens{1401.0, 2.56};

// Queney's ridge: a bell, h0 a^2 / (a^2 + x^2).
double ridge(double h0, double a, double x) {
    return h0 * a * a / (a * a + x * x);
}

} // namespace

GLIDESLOPE_TEST(allens_updrafts_have_the_size_and_speed_his_paper_gives) {
    using glideslope::world::allen_mean_updraught;
    using glideslope::world::allen_radius;
    using glideslope::world::allen_updraught;

    // His worked example: 280 m up, an outer radius of 79.4 m.
    check(std::abs(allen_radius(allens, 280.0) - 79.4) < 0.05,
          "an updraft 280 m up in his example is 79.4 m across its outer radius");

    // His figure 5: the mean updraft is fastest, 0.45 w*, about a quarter of
    // the way up, and sinks above nine-tenths of the way.
    double fastest = 0.0;
    double fastest_at = 0.0;
    for (double f = 0.001; f < 1.0; f += 0.001) {
        const double w = allen_mean_updraught(allens, f * allens.depth_m);
        if (w > fastest) {
            fastest = w;
            fastest_at = f;
        }
    }
    check(
        std::abs(fastest / allens.velocity_mps - 0.45) < 0.01 &&
            std::abs(fastest_at - 0.25) < 0.03,
        "the mean updraft is fastest, 0.45 w*, about a quarter of the way up; it is " +
            std::to_string(fastest / allens.velocity_mps) + " w* at " +
            std::to_string(fastest_at));
    check(allen_mean_updraught(allens, 0.89 * allens.depth_m) > 0.0 &&
              allen_mean_updraught(allens, 0.92 * allens.depth_m) < 0.0,
          "the mean updraft sinks above nine-tenths of the way up");

    // His figure 10, as read from it: an updraft's speed at its centre at four
    // heights, and the sinking ring about it 0.8 of the way up, deepest about
    // 160 m out. Drawn without the sink between updrafts.
    const struct {
        double height_ratio;
        double distance_m;
        double speed_mps;
    } figure[] = {{0.2, 0.0, 2.73}, {0.4, 0.0, 2.42},    {0.6, 0.0, 1.68},
                  {0.8, 0.0, 0.65}, {0.8, 160.0, -0.11}, {0.2, 110.0, 0.0},
                  {0.4, 120.0, 0.0}};
    for (const auto& p : figure) {
        const double w = allen_updraught(allens, p.height_ratio * allens.depth_m,
                                         p.distance_m, 1.0, 0.0);
        check(std::abs(w - p.speed_mps) < 0.05,
              std::to_string(p.distance_m) + " m from an updraft's centre " +
                  std::to_string(p.height_ratio) +
                  " of the way up: " + std::to_string(w) +
                  " m/s, where his figure 10 has " + std::to_string(p.speed_mps));
    }
}

GLIDESLOPE_TEST(the_convective_layer_is_as_deep_as_a_warmer_parcel_rises) {
    using glideslope::world::convection;
    const double ground = 500.0;
    const double surface = 30.0;

    // Under the standard atmosphere's lapse, 6.5 C a kilometre, a parcel 1 C
    // warmer cooling at 9.8 C a kilometre is as cool as the air 1/3.3 km up.
    const auto standard = [&](double h) { return surface - 0.0065 * (h - ground); };
    const Convection shallow = convection(surface, ground, 3.0, standard);
    check(std::abs(shallow.depth_m - 1000.0 / 3.3) < 1e-6,
          "under the standard lapse the layer is 303 m deep; it is " +
              std::to_string(shallow.depth_m));
    check(std::abs(shallow.velocity_mps - 2.0 * std::cbrt(shallow.depth_m / 1500.0)) <
              1e-12,
          "its convective velocity grows with the cube root of its depth");

    // Air 2 C cooler than the parcel's adiabat up to 1,200 m, and warming at
    // 5 C a kilometre above: the parcel meets it 1,402.7 m up.
    const auto capped = [&](double h) {
        const double up = (h - ground) / 1000.0;
        const double top = surface - 2.0 - 9.8 * 1.2;
        return up <= 1.2 ? surface - 2.0 - 9.8 * up : top + 5.0 * (up - 1.2);
    };
    const Convection deep = convection(surface, ground, 3.0, capped);
    check(std::abs(deep.depth_m - 1000.0 * 20.76 / 14.8) < 1e-6,
          "under an inversion 1,200 m up the layer is 1,402.7 m deep; it is " +
              std::to_string(deep.depth_m));

    // A surface cooler than the air above has none; a wind over 25 kt stops the
    // thermals but not the layer; and none rises past 4 km.
    check(convection(
              surface, ground, 3.0,
              [&](double h) {
                  return surface + 2.0 + 0.0 * h;
              }).depth_m == 0.0,
          "a surface under warmer air has no convective layer");
    const Convection windy =
        convection(surface, ground, 26.0 * 1852.0 / 3600.0, capped);
    check(windy.velocity_mps == 0.0 && windy.depth_m == deep.depth_m,
          "a surface wind over 25 kt stops the thermals");
    const Convection unstable = convection(
        surface, ground, 3.0, [&](double h) { return surface - 0.02 * (h - ground); });
    check(unstable.depth_m == 4000.0, "no convective layer is deeper than 4 km");
    check(convection(
              surface, ground, 3.0,
              [&](double h) {
                  return surface - 0.0050 * (h - ground);
              }).velocity_mps == 0.0,
          "a layer under 300 m deep has no thermals");
}

GLIDESLOPE_TEST(
    each_thermal_in_its_prime_is_allens_updraft_and_between_them_the_air_sinks) {
    using glideslope::world::allen_radius;
    using glideslope::world::allen_updraught;
    using glideslope::world::environment_sink;
    using glideslope::world::thermal_in_cell;
    using glideslope::world::thermal_spacing_m;
    using glideslope::world::thermal_updraught;
    const std::uint64_t seed = 0x5eed;
    const double spacing = thermal_spacing_m(allens);
    check(std::abs(spacing - std::sqrt(allens.depth_m * 0.102 * std::cbrt(0.4) * 0.9 *
                                       allens.depth_m / 0.6)) < 1e-9,
          "one thermal to zi r2 / 0.6, r2 at 0.4 of the way up, as Allen counts them");

    // Every thermal in a block of cells at a moment, for finding which are
    // near a point.
    const auto thermals_about = [&](double east, double north, double time_s) {
        std::vector<glideslope::world::Thermal> out;
        for (int dr = -2; dr <= 2; ++dr) {
            for (int dc = -2; dc <= 2; ++dc) {
                out.push_back(thermal_in_cell(seed, allens, east + dc * spacing,
                                              north + dr * spacing, time_s));
            }
        }
        return out;
    };

    int primes = 0;
    int probes = 0;
    int between = 0;
    for (int cell = 0; cell < 200 && primes < 40; ++cell) {
        const double east = (cell % 20) * spacing + 0.5 * spacing;
        const double north = (cell / 20) * spacing + 0.5 * spacing;
        // A moment in the thermal's prime.
        double time_s = 0.0;
        glideslope::world::Thermal t;
        for (time_s = 0.0; time_s < 1200.0; time_s += 30.0) {
            t = thermal_in_cell(seed, allens, east, north, time_s);
            if (t.life == 1.0) {
                break;
            }
        }
        check(t.life == 1.0, "every thermal has a prime in its twenty minutes");
        check(t.gain >= 0.7 && t.gain <= 1.3,
              "a thermal is 0.7 to 1.3 times Allen's mean");
        const auto others = thermals_about(east, north, time_s);
        for (const double ratio : {0.1, 0.3, 0.5, 0.7, 0.85}) {
            const double z = ratio * allens.depth_m;
            const double reach = 2.0 * allen_radius(allens, z);
            const double sink = environment_sink(allens, z);
            for (double d = 0.0; d <= 3.0 * reach; d += reach / 8.0) {
                const Enu p{t.east + d * 0.6, t.north + d * 0.8, z};
                bool alone = true;
                for (const auto& o : others) {
                    const bool itself = o.east == t.east && o.north == t.north;
                    if (!itself && o.life > 0.0 &&
                        std::hypot(p.east - o.east, p.north - o.north) < reach) {
                        alone = false;
                    }
                }
                if (!alone) {
                    continue;
                }
                ++probes;
                const double w = thermal_updraught(seed, allens, p, time_s);
                const double expected = allen_updraught(allens, z, d, t.gain, sink);
                check(std::abs(w - expected) < 1e-12,
                      "at " + std::to_string(d) + " m from a thermal in its prime, " +
                          std::to_string(ratio) + " of the way up, Allen's updraft: " +
                          std::to_string(w) + " against " + std::to_string(expected));
                if (d >= reach) {
                    ++between;
                    check(std::abs(w - sink) < 1e-12 && sink < 0.0,
                          "beyond a thermal's reach the air sinks as Allen's sink");
                }
            }
        }
        ++primes;
    }
    check(primes == 40 && probes > 1000 && between > 200,
          "forty thermals were found in their prime, and probed about and between: " +
              std::to_string(primes) + ", " + std::to_string(probes) + ", " +
              std::to_string(between));

    // Over the whole pattern and a thermal's life, the air sinks about as much
    // as it rises.
    for (const double ratio : {0.2, 0.4, 0.6}) {
        const double z = ratio * allens.depth_m;
        double sum = 0.0;
        double rising = 0.0;
        int count = 0;
        for (double time_s = 0.0; time_s < 1200.0; time_s += 300.0) {
            for (double north = 0.0; north < 8.0 * spacing; north += 10.0) {
                for (double east = 0.0; east < 8.0 * spacing; east += 10.0) {
                    const double w =
                        thermal_updraught(seed, allens, {east, north, z}, time_s);
                    sum += w;
                    rising += std::max(0.0, w);
                    ++count;
                }
            }
        }
        check(std::abs(sum) < 0.15 * rising,
              "the air sinks about as much as it rises " + std::to_string(ratio) +
                  " of the way up: on average " + std::to_string(sum / count) +
                  " m/s, where the rising air alone is " +
                  std::to_string(rising / count));
    }
}

GLIDESLOPE_TEST(
    a_thermal_grows_lives_and_fades_and_the_next_rises_elsewhere_in_its_cell) {
    using glideslope::world::thermal_in_cell;
    using glideslope::world::thermal_updraught;
    const std::uint64_t seed = 42;
    const double east = 1234.0;
    const double north = -5678.0;
    glideslope::world::Thermal before = thermal_in_cell(seed, allens, east, north, 0.0);
    int generations = 0;
    double most_change = 0.0;
    for (double time_s = 1.0; time_s < 3.0 * 1200.0; time_s += 1.0) {
        const glideslope::world::Thermal now =
            thermal_in_cell(seed, allens, east, north, time_s);
        if (now.east != before.east || now.north != before.north) {
            ++generations;
            check(before.life < 1e-4 && now.life < 1e-4,
                  "a thermal is gone before the next rises in its cell");
        }
        most_change = std::max(most_change, std::abs(now.life - before.life));
        before = now;
    }
    check(generations == 3, "a cell has a thermal every twenty minutes: " +
                                std::to_string(generations) + " in an hour");
    check(most_change < 0.01, "a thermal grows and fades smoothly: " +
                                  std::to_string(most_change) + " a second at most");

    // The air changes smoothly with place and time.
    double most_step = 0.0;
    for (double x = 0.0; x < 3000.0; x += 1.0) {
        const Enu a{x, 0.3 * x, 400.0};
        const Enu b{x + 1.0, 0.3 * (x + 1.0), 400.0};
        most_step =
            std::max(most_step, std::abs(thermal_updraught(seed, allens, a, 600.0) -
                                         thermal_updraught(seed, allens, b, 600.0)));
    }
    check(most_step < 0.1, "the thermals' air changes by under 0.1 m/s a metre: " +
                               std::to_string(most_step));
}

GLIDESLOPE_TEST(a_ridge_in_stable_air_makes_the_mountain_waves_of_linear_theory) {
    using glideslope::world::terrain_updraught;
    // Linear theory's vertical wind over Queney's ridge, whose transform is
    // pi h0 a e^(-|k| a):
    //   w = -U h0 a [ int_0^l k e^(-ka) sin(kx + sqrt(l^2 - k^2) z) dk
    //               + int_l^inf k e^(-ka) e^(-sqrt(k^2 - l^2) z) sin(kx) dk ],
    // integrated here over k = l sin t and k = l cosh u, which keep the square
    // roots smooth. Where every wave is much longer than l, it is Queney's
    // hydrostatic solution.
    const auto simpson = [](const auto& f, double lo, double hi) {
        constexpr int n = 2000;
        const double step = (hi - lo) / n;
        double sum = f(lo) + f(hi);
        for (int i = 1; i < n; ++i) {
            sum += f(lo + i * step) * (i % 2 != 0 ? 4.0 : 2.0);
        }
        return sum * step / 3.0;
    };
    const auto linear = [&](double h0, double a, double speed, double l, double x,
                            double z) {
        const double top = 60.0 / a;
        const double waves = simpson(
            [&](double t) {
                const double k = l * std::sin(t);
                return k * std::exp(-k * a) * std::sin(k * x + l * std::cos(t) * z) *
                       l * std::cos(t);
            },
            0.0, pi / 2.0);
        const double dying = simpson(
            [&](double u) {
                const double k = l * std::cosh(u);
                return k * std::exp(-k * a) * std::exp(-l * std::sinh(u) * z) *
                       std::sin(k * x) * l * std::sinh(u);
            },
            0.0, std::acosh(top / l));
        return -speed * h0 * a * (waves + dying);
    };

    // A wide ridge, whose waves are all but hydrostatic - N a / U is 20 - and
    // a narrow one, where they are not: N a / U is 2.
    const struct {
        double h0;
        double a;
        double speed;
        double n;
    } ridges[] = {{50.0, 5000.0, 5.0, 0.02}, {100.0, 2000.0, 10.0, 0.01}};
    for (const auto& r : ridges) {
        double most = 0.0;
        double worst = 0.0;
        for (const double z : {2000.0, 3000.0, 4000.0, 6000.0}) {
            for (double x = -15000.0; x <= 15000.0; x += 1000.0) {
                const double w = terrain_updraught(
                    [&](double along) { return ridge(r.h0, r.a, x + along); }, z,
                    r.speed, r.n);
                const double expected = linear(r.h0, r.a, r.speed, r.n / r.speed, x, z);
                most = std::max(most, std::abs(expected));
                worst = std::max(worst, std::abs(w - expected));
            }
        }
        check(worst < 0.04 * most,
              "the waves over a ridge " + std::to_string(r.a) +
                  " m wide are linear theory's, within 4% of the strongest: at worst " +
                  std::to_string(worst) + " m/s off, the strongest " +
                  std::to_string(most));
    }
}

GLIDESLOPE_TEST(without_stability_the_wind_over_a_ridge_is_potential_flow) {
    using glideslope::world::terrain_updraught;
    const double h0 = 100.0;
    const double a = 1000.0;
    const double speed = 10.0;
    double most = 0.0;
    double worst_ground = 0.0;
    double worst_aloft = 0.0;
    for (double x = -5000.0; x <= 5000.0; x += 100.0) {
        const auto terrain = [&](double along) { return ridge(h0, a, x + along); };
        // At the ground, the terrain's slope.
        const double slope = -2.0 * h0 * a * a * x / std::pow(a * a + x * x, 2.0);
        const double ground = terrain_updraught(terrain, ridge(h0, a, x), speed, 0.0);
        most = std::max(most, std::abs(speed * slope));
        worst_ground = std::max(worst_ground, std::abs(ground - speed * slope));
        // Aloft: eta = h0 a (a + z) / (x^2 + (a + z)^2).
        for (const double z : {2000.0, 3000.0, 5000.0}) {
            const double b = a + z;
            const double expected =
                speed * -2.0 * h0 * a * b * x / std::pow(x * x + b * b, 2.0);
            worst_aloft = std::max(
                worst_aloft,
                std::abs(terrain_updraught(terrain, z, speed, 0.0) - expected));
        }
    }
    check(worst_ground < 0.02 * most,
          "at the ground the wind rises and falls with the ridge's slope: at worst " +
              std::to_string(worst_ground) + " m/s off, the most " +
              std::to_string(most));
    check(worst_aloft < 0.02 * most, "aloft it is potential flow's: at worst " +
                                         std::to_string(worst_aloft) + " m/s off");

    // Flat ground lifts nothing, and no wind lifts nothing.
    check(terrain_updraught([](double) { return 300.0; }, 1000.0, speed, 0.01) == 0.0,
          "flat ground makes no lift");
    check(terrain_updraught([&](double along) { return ridge(h0, a, along - 500.0); },
                            200.0, 0.5, 0.01) == 0.0,
          "a wind under 1 m/s makes no lift");
}

GLIDESLOPE_TEST(
    a_warm_afternoons_report_lifts_the_air_in_thermals_and_over_its_ridges) {
    namespace world = glideslope::world;
    world::WeatherReport report;
    report.surface.metar =
        world::parse_metar("KXYZ 182100Z 27008KT 9999 SKC 32/04 A3002");
    report.surface.latitude_deg = 35.0;
    report.surface.longitude_deg = -106.0;
    report.surface.elevation_m = 1600.0;
    report.air_seed = world::air_seed_of(report.surface.metar);
    // A forecast profile: warm and well mixed to 3 km above the station, then
    // stable, and a westerly strengthening with height.
    world::WindsAloft aloft;
    aloft.latitude_deg = 35.0;
    aloft.longitude_deg = -106.0;
    const struct {
        double pressure;
        double height;
        double temperature;
        double east;
    } levels[] = {{800.0, 2000.0, 25.0, 6.0},
                  {700.0, 3100.0, 14.2, 9.0},
                  {600.0, 4300.0, 8.0, 12.0},
                  {500.0, 5700.0, -2.0, 16.0},
                  {400.0, 7300.0, -14.0, 20.0}};
    for (const auto& l : levels) {
        world::AloftLevel level;
        level.pressure_hpa = l.pressure;
        level.height_m = l.height;
        level.temperature_c = l.temperature;
        level.wind_east_mps = l.east;
        aloft.levels.push_back(level);
    }
    report.aloft = aloft;

    const world::Lift lift = world::lift_of(report);

    // A parcel from 33 C at 1,600 m, cooling at 9.8 C a kilometre, meets the
    // forecast's air between its 700 and 600 hPa levels, where that cools
    // from 14.2 C at 3,100 m to 8.0 C at 4,300 m.
    const double top = (33.0 + 0.0098 * 1600.0 - 14.2 - 3100.0 * 6.2 / 1200.0) /
                       (0.0098 - 6.2 / 1200.0);
    check(std::abs(lift.convection.depth_m - (top - 1600.0)) < 1e-6,
          "the afternoon's convective layer reaches where the parcel meets the "
          "forecast's air: " +
              std::to_string(lift.convection.depth_m) + " m deep, against " +
              std::to_string(top - 1600.0));
    check(std::abs(lift.convection.velocity_mps -
                   2.0 * std::cbrt(lift.convection.depth_m / 1500.0)) < 1e-12,
          "its thermals are as strong as its depth makes them");
    // The waves' air: between 2,600 and 5,600 m, from 19.1 C to -1.3 C.
    const double low = 25.0 + (14.2 - 25.0) * 600.0 / 1100.0;
    const double high = 8.0 + (-2.0 - 8.0) * 1300.0 / 1400.0;
    const double n2 =
        9.80665 / (0.5 * (low + high) + 273.15) * ((high - low) / 3000.0 + 0.0098);
    check(
        std::abs(lift.buoyancy_frequency - std::sqrt(n2)) < 1e-12,
        "the waves' buoyancy frequency is from the lapse 1 to 4 km above the station");
    check(lift.drift_east_mps ==
              world::conditions_at(report, 1600.0 + 0.5 * lift.convection.depth_m)
                  .wind_east_mps,
          "the thermals drift with the wind halfway up their layer");

    // A ridge running north and south 9 km east of the station, 400 m high and
    // 2 km wide, in the westerly: over its windward slope the air rises, in
    // its lee it sinks, and with the thermals that is all the air does.
    constexpr double metres_per_degree = 111319.49;
    const double cosine = std::cos(35.0 * pi / 180.0);
    const double crest = -106.0 + 9000.0 / (metres_per_degree * cosine);
    const world::GroundAt ground = [&](double, double lon) {
        return 1600.0 +
               ridge(400.0, 2000.0, (lon - crest) * metres_per_degree * cosine);
    };
    const double wind = world::conditions_at(report, 2600.0).wind_east_mps;
    for (const double x : {-1500.0, -700.0, 700.0, 1500.0}) {
        const double lat = 35.0;
        const double lon = crest + x / (metres_per_degree * cosine);
        const double height = ground(lat, lon) + 150.0;
        const double time_s = 1800.0;
        const glideslope::sim::Conditions c = world::with_air_motion(
            report, lift, ground, world::conditions_at(report, height), lat, lon,
            height, time_s);
        const double lifted = world::terrain_updraught(
            [&](double along) {
                return ground(lat, lon + along / (metres_per_degree * cosine));
            },
            height, wind, lift.buoyancy_frequency);
        const Enu drifted{(lon + 106.0) * metres_per_degree * cosine -
                              lift.drift_east_mps * time_s,
                          -lift.drift_north_mps * time_s, 150.0};
        const double rising =
            world::thermal_updraught(report.air_seed, lift.convection, drifted, time_s);
        check(std::abs(-c.wind_down_mps - (lifted + rising)) < 1e-9,
              "over the ridge the air rises with its thermals and the ridge's lift");
        check(x < 0.0 ? lifted > 0.25 : lifted < -0.25,
              std::to_string(x) + " m from the crest, 150 m up, the ridge's air " +
                  (x < 0.0 ? "rises" : "sinks") + ": " + std::to_string(lifted) +
                  " m/s");
    }
}

namespace {

// The weather it wraps, with its air neither rising nor sinking.
class StillAir : public glideslope::sim::Weather {
public:
    explicit StillAir(std::shared_ptr<glideslope::sim::Weather> weather)
        : weather_(std::move(weather)) {}
    glideslope::sim::Conditions at(double latitude_deg, double longitude_deg,
                                   double height_m, double time_s) override {
        glideslope::sim::Conditions c =
            weather_->at(latitude_deg, longitude_deg, height_m, time_s);
        c.wind_down_mps = 0.0;
        return c;
    }

private:
    std::shared_ptr<glideslope::sim::Weather> weather_;
};

} // namespace

GLIDESLOPE_TEST(
    an_aircraft_circling_in_a_thermal_climbs_by_as_much_as_the_model_lifts_it) {
    namespace world = glideslope::world;
    namespace sim = glideslope::sim;
    // A hot, calm afternoon by the sea under a layer mixed nearly dry-
    // adiabatically to 4 km: the deepest the model makes, its thermals the
    // widest, a C172's circle well inside one.
    world::WeatherReport report;
    report.surface.metar =
        world::parse_metar("KXYZ 181900Z 00000KT 9999 SKC 35/00 A2992");
    report.surface.latitude_deg = 35.0;
    report.surface.longitude_deg = -106.0;
    report.surface.elevation_m = 0.0;
    report.air_seed = world::air_seed_of(report.surface.metar);
    world::WindsAloft aloft;
    aloft.latitude_deg = 35.0;
    aloft.longitude_deg = -106.0;
    const struct {
        double pressure;
        double height;
        double temperature;
    } levels[] = {{1000.0, 100.0, 34.0},  {850.0, 1500.0, 20.5},
                  {700.0, 3100.0, 5.0},   {600.0, 4300.0, -6.5},
                  {500.0, 5700.0, -15.0}, {400.0, 7300.0, -28.0}};
    for (const auto& l : levels) {
        world::AloftLevel level;
        level.pressure_hpa = l.pressure;
        level.height_m = l.height;
        level.temperature_c = l.temperature;
        aloft.levels.push_back(level);
    }
    report.aloft = aloft;
    const world::Lift lift = world::lift_of(report);
    const Convection& layer = lift.convection;
    check(layer.depth_m == 4000.0 && layer.velocity_mps > 2.5,
          "the afternoon's layer is 4 km deep, its thermals strong: " +
              std::to_string(layer.depth_m) + " m, " +
              std::to_string(layer.velocity_mps) + " m/s");

    // A thermal in its prime for the whole flight.
    const double spacing = world::thermal_spacing_m(layer);
    const double flight_s = 180.0;
    world::Thermal thermal;
    for (int cell = 0; cell < 100; ++cell) {
        const double east = (cell % 10 + 0.5) * spacing;
        const double north = (cell / 10 + 0.5) * spacing;
        const world::Thermal first =
            world::thermal_in_cell(report.air_seed, layer, east, north, 0.0);
        const world::Thermal last =
            world::thermal_in_cell(report.air_seed, layer, east, north, flight_s);
        if (first.life == 1.0 && last.life == 1.0 && first.east == last.east) {
            thermal = first;
            break;
        }
    }
    check(thermal.life == 1.0, "a thermal is found in its prime for three minutes");

    // Gliding at 65 KCAS in a 45-degree bank, from 0.45 of the way up the
    // layer, from a place: each step after the first minute, where the
    // aircraft is, how fast it climbs, and the model's updraught there.
    constexpr double metres_per_degree = 111319.49;
    const double cosine = std::cos(35.0 * pi / 180.0);
    struct Sample {
        double east;
        double north;
        double height;
        double climb;
        double updraught;
    };
    const auto fly = [&](double east, double north, bool rising) {
        auto reported = std::make_shared<world::ReportedWeather>(report, nullptr, 0.0);
        sim::InitialConditions ic;
        ic.latitude_deg = 35.0 + north / metres_per_degree;
        ic.longitude_deg = -106.0 + east / (metres_per_degree * cosine);
        ic.altitude_ft = 0.45 * layer.depth_m / 0.3048;
        ic.airspeed_kts = 65.0;
        ic.engine_running = false;
        sim::Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "c172p");
        a.initialize(ic);
        if (rising) {
            a.set_weather(reported);
        } else {
            a.set_weather(std::make_shared<StillAir>(reported));
        }
        sim::TestPilot pilot(a);
        sim::Controls c;
        c.mixture = 0.0;
        std::vector<Sample> out;
        for (int i = 0; i < static_cast<int>(flight_s) * 120; ++i) {
            c.elevator = pilot.pitch_to(pilot.pitch_for_speed(65.0));
            c.aileron = pilot.roll_to(45.0);
            c.rudder = pilot.coordinate();
            a.set_controls(c);
            a.step();
            if (i >= 60 * 120) {
                Sample p;
                p.east = (a.property("position/long-gc-deg") + 106.0) *
                         metres_per_degree * cosine;
                p.north =
                    (a.property("position/lat-geod-deg") - 35.0) * metres_per_degree;
                p.height = a.property("position/geod-alt-ft") * 0.3048;
                p.climb = a.property("velocities/h-dot-fps") * 0.3048;
                p.updraught = world::thermal_updraught(
                    report.air_seed, layer, {p.east, p.north, p.height},
                    a.property("simulation/sim-time-sec"));
                out.push_back(p);
            }
        }
        return out;
    };
    const auto mean = [](const std::vector<Sample>& v, double Sample::* field) {
        double sum = 0.0;
        for (const Sample& p : v) {
            sum += p.*field;
        }
        return sum / static_cast<double>(v.size());
    };

    // In still air, to find where the circle is from where it starts, and how
    // fast the aircraft sinks at each height - thinner air, higher up, makes it
    // fly faster and sink faster - as a straight line fitted through it.
    const std::vector<Sample> still = fly(0.0, 0.0, false);
    const double still_east = mean(still, &Sample::east);
    const double still_north = mean(still, &Sample::north);
    const double h0 = mean(still, &Sample::height);
    const double c0 = mean(still, &Sample::climb);
    double hh = 0.0;
    double hc = 0.0;
    for (const Sample& p : still) {
        hh += (p.height - h0) * (p.height - h0);
        hc += (p.height - h0) * (p.climb - c0);
    }
    const double per_metre = hc / hh;

    // Then round the thermal: each moment it should climb as it would in still
    // air at that height, and as fast again as the air rises there.
    const std::vector<Sample> lifted =
        fly(thermal.east - still_east, thermal.north - still_north, true);
    double expected = 0.0;
    for (const Sample& p : lifted) {
        expected += c0 + per_metre * (p.height - h0) + p.updraught;
    }
    expected /= static_cast<double>(lifted.size());
    const double climb = mean(lifted, &Sample::climb);
    const double updraught = mean(lifted, &Sample::updraught);
    check(std::hypot(mean(lifted, &Sample::east) - thermal.east,
                     mean(lifted, &Sample::north) - thermal.north) < 30.0,
          "the aircraft circles round the thermal's centre");
    check(updraught > 1.0, "the thermal raises the air it circles in by over 1 m/s: " +
                               std::to_string(updraught) + " m/s");
    check(std::abs(climb - expected) < 0.02 * updraught,
          "round the thermal it climbs at " + std::to_string(climb) +
              " m/s, where its still-air sink and the model's updraught make " +
              std::to_string(expected) + " m/s; the updraught is " +
              std::to_string(updraught) + " m/s");
}

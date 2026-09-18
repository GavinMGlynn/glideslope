#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "world/air_motion.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::DrydenScales;
using glideslope::world::Enu;
using glideslope::world::WeatherReport;

namespace {

constexpr double mps_per_knot = 1852.0 / 3600.0;

std::vector<glideslope::world::SurfaceReport> gusty_reports() {
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                         "data/weather/aviationweather-gusts-2026-09-18T0800Z.json",
                     std::ios::binary);
    check(static_cast<bool>(in), "can read the recorded gusty METARs");
    return glideslope::world::parse_aviationweather(
        std::string(std::istreambuf_iterator<char>(in), {}));
}

WeatherReport report_for(const glideslope::world::SurfaceReport& surface) {
    WeatherReport r;
    r.surface = surface;
    r.air_seed = glideslope::world::air_seed_of(surface.metar);
    return r;
}

// Places and times around a station: within 20 km, up to 1.5 km above it -
// where even light turbulence is not zero - over an hour, from a fixed
// sequence.
struct Probe {
    double latitude_deg;
    double longitude_deg;
    double height_m;
    double time_s;
};

std::vector<Probe> probes(const glideslope::world::SurfaceReport& at, int count) {
    std::vector<Probe> out;
    std::uint64_t state = 12345;
    const auto next = [&] {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(state >> 11) / 9007199254740992.0;
    };
    for (int i = 0; i < count; ++i) {
        out.push_back({at.latitude_deg + (next() - 0.5) * 0.36,
                       at.longitude_deg + (next() - 0.5) * 0.36,
                       at.elevation_m + next() * 1500.0, next() * 3600.0});
    }
    return out;
}

} // namespace

GLIDESLOPE_TEST(the_air_is_the_same_wherever_and_whenever_it_is_asked_for) {
    for (const auto& surface : gusty_reports()) {
        const std::string name = surface.metar.station;
        glideslope::world::ReportedWeather one(report_for(surface), nullptr, 0.0);
        glideslope::world::ReportedWeather two(report_for(surface), nullptr, 0.0);
        const auto places = probes(surface, 1000);
        std::vector<glideslope::sim::Conditions> first;
        for (const auto& p : places) {
            first.push_back(
                one.at(p.latitude_deg, p.longitude_deg, p.height_m, p.time_s));
        }
        // The other weather asked in the other order, and the first asked again.
        int moving = 0;
        for (std::size_t k = places.size(); k-- > 0;) {
            const auto& p = places[k];
            const auto b =
                two.at(p.latitude_deg, p.longitude_deg, p.height_m, p.time_s);
            const auto c =
                one.at(p.latitude_deg, p.longitude_deg, p.height_m, p.time_s);
            for (const auto& other : {b, c}) {
                check(other.wind_north_mps == first[k].wind_north_mps &&
                          other.wind_east_mps == first[k].wind_east_mps &&
                          other.wind_down_mps == first[k].wind_down_mps,
                      name + ": the same air at probe " + std::to_string(k));
            }
            moving += first[k].wind_down_mps != 0.0 ? 1 : 0;
        }
        check(moving > 900, name + ": and the air moves: vertical wind at " +
                                std::to_string(moving) + " of 1000 probes");

        // Another seed is other air.
        WeatherReport other = report_for(surface);
        other.air_seed += 1;
        glideslope::world::ReportedWeather three(other, nullptr, 0.0);
        int different = 0;
        for (std::size_t k = 0; k < places.size(); ++k) {
            const auto& p = places[k];
            different += three.at(p.latitude_deg, p.longitude_deg, p.height_m, p.time_s)
                                     .wind_down_mps != first[k].wind_down_mps
                             ? 1
                             : 0;
        }
        check(different > 900, name + ": another seed is other air");
    }
}

GLIDESLOPE_TEST(an_aircraft_restored_in_a_gust_flies_on_in_the_same_air) {
    const auto surface = gusty_reports().at(1); // Albuquerque, 27 gusting 37 kt
    auto weather = std::make_shared<glideslope::world::ReportedWeather>(
        report_for(surface), nullptr, 0.0);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = surface.latitude_deg;
    ic.longitude_deg = surface.longitude_deg;
    ic.altitude_ft = (surface.elevation_m + 300.0) / 0.3048;
    ic.terrain_elevation_ft = surface.elevation_m / 0.3048;
    ic.airspeed_kts = 100.0;
    ic.engine_running = true;
    glideslope::sim::Aircraft flown(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    flown.initialize(ic);
    flown.set_weather(weather);
    glideslope::sim::Controls c;
    c.throttle = 0.7;
    for (int i = 0; i < 15 * 120; ++i) {
        flown.set_controls(c);
        flown.step();
    }
    const auto snapshot = flown.capture();
    glideslope::sim::Aircraft restored(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    restored.set_weather(std::make_shared<glideslope::world::ReportedWeather>(
        report_for(surface), nullptr, 0.0));
    restored.restore(snapshot);

    // The wind each aircraft is given is the weather at its own place and time,
    // and nothing else: before each step, what the weather is where each is.
    const auto expected = [&](glideslope::sim::Aircraft& a) {
        return weather->at(a.property("position/lat-geod-deg"),
                           a.property("position/long-gc-deg"),
                           a.property("position/geod-alt-ft") * 0.3048,
                           a.property("simulation/sim-time-sec"));
    };
    double first_steps = 0.0;
    double unexplained = 0.0;
    double apart = 0.0;
    for (int i = 0; i < 15 * 120; ++i) {
        const auto for_flown = expected(flown);
        const auto for_restored = expected(restored);
        flown.set_controls(c);
        restored.set_controls(c);
        flown.step();
        restored.step();
        const double gap = std::abs(flown.property("atmosphere/wind-north-fps") -
                                    restored.property("atmosphere/wind-north-fps"));
        if (i == 0) {
            first_steps = gap;
        }
        apart = std::max(apart, gap);
        const double explained =
            std::abs(for_flown.wind_north_mps - for_restored.wind_north_mps) / 0.3048;
        unexplained = std::max(unexplained, std::abs(gap - explained));
    }
    check(first_steps < 1e-6,
          "the first step after the restore, from the same place at the same time, "
          "meets the same wind: " +
              std::to_string(first_steps) + " ft/s apart");
    check(unexplained < 1e-9,
          "every difference after that is the weather's between where the two are: " +
              std::to_string(unexplained) + " ft/s unexplained, " +
              std::to_string(apart) + " ft/s apart at most");
}

GLIDESLOPE_TEST(a_recorded_gusty_metar_gives_winds_between_its_mean_and_gust_speeds) {
    for (const auto& surface : gusty_reports()) {
        const auto& m = surface.metar;
        const std::string name = m.station;
        check(m.gust_kt.has_value(), name + ": a gust");
        // The gusts alone: no turbulence.
        WeatherReport report = report_for(surface);
        report.turbulence_severity = 0;
        glideslope::world::ReportedWeather weather(report, nullptr, 0.0);
        const double mean = *m.wind_speed_kt;
        const double gust = *m.gust_kt;
        double lowest = 1e9;
        double highest = -1e9;
        // Ten minutes 10 m above the station, where a METAR's wind is measured.
        for (double t = 0.0; t < 600.0; t += 0.1) {
            const auto c = weather.at(surface.latitude_deg, surface.longitude_deg,
                                      surface.elevation_m + 10.0, t);
            const double kt =
                std::hypot(c.wind_north_mps, c.wind_east_mps) / mps_per_knot;
            lowest = std::min(lowest, kt);
            highest = std::max(highest, kt);
        }
        check(lowest >= mean - 1e-6 && highest <= gust + 1e-6,
              name + ": between " + std::to_string(mean) + " and " +
                  std::to_string(gust) + " kt: " + std::to_string(lowest) + " to " +
                  std::to_string(highest));
        check(highest >= gust - 0.5 && lowest <= mean + 0.5,
              name + ": reaching both within half a knot in ten minutes: " +
                  std::to_string(lowest) + " to " + std::to_string(highest));
        // And 700 m up, above the gusts, the mean alone.
        const auto up = weather.at(surface.latitude_deg, surface.longitude_deg,
                                   surface.elevation_m + 700.0, 123.0);
        const auto steady =
            glideslope::world::conditions_at(report, surface.elevation_m + 700.0);
        check(up.wind_north_mps == steady.wind_north_mps &&
                  up.wind_east_mps == steady.wind_east_mps,
              name + ": no gust 700 m up");
    }

    // A report without gusts has none, and no turbulence.
    auto calm = gusty_reports().at(0);
    calm.metar.gust_kt.reset();
    glideslope::world::ReportedWeather steady(report_for(calm), nullptr, 0.0);
    const auto first =
        steady.at(calm.latitude_deg, calm.longitude_deg, calm.elevation_m + 10.0, 0.0);
    for (double t = 0.0; t < 60.0; t += 0.5) {
        const auto c = steady.at(calm.latitude_deg, calm.longitude_deg,
                                 calm.elevation_m + 10.0, t);
        check(c.wind_north_mps == first.wind_north_mps &&
                  c.wind_east_mps == first.wind_east_mps && c.wind_down_mps == 0.0,
              "no gusts, no turbulence: a steady wind");
    }

    // The same flight in the gusts twice is the same flight: a minute at
    // Albuquerque, 300 m up, with the turbulence the gusts imply.
    const auto fly = [&] {
        const auto surface = gusty_reports().at(1);
        glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = surface.latitude_deg;
        ic.longitude_deg = surface.longitude_deg;
        ic.altitude_ft = (surface.elevation_m + 300.0) / 0.3048;
        ic.terrain_elevation_ft = surface.elevation_m / 0.3048;
        ic.airspeed_kts = 100.0;
        ic.engine_running = true;
        aircraft.initialize(ic);
        aircraft.set_weather(std::make_shared<glideslope::world::ReportedWeather>(
            report_for(surface), nullptr, 0.0));
        glideslope::sim::Controls c;
        c.throttle = 0.7;
        for (int i = 0; i < 60 * 120; ++i) {
            aircraft.set_controls(c);
            aircraft.step();
        }
        return aircraft.state();
    };
    const auto a = fly();
    const auto b = fly();
    check(a.latitude_deg == b.latitude_deg && a.longitude_deg == b.longitude_deg &&
              a.altitude_ft == b.altitude_ft && a.roll_deg == b.roll_deg,
          "the same flight in the gusts twice ends in the same place");

    // The turbulence the gusts imply.
    check(glideslope::world::severity_from_gust_spread(4.9) == 0 &&
              glideslope::world::severity_from_gust_spread(5.0) == 1 &&
              glideslope::world::severity_from_gust_spread(10.0) == 2 &&
              glideslope::world::severity_from_gust_spread(15.0) == 3 &&
              glideslope::world::severity_from_gust_spread(20.0) == 4 &&
              glideslope::world::severity_from_gust_spread(30.0) == 5,
          "severity from the gusts' spread");
}

GLIDESLOPE_TEST(turbulence_has_drydens_intensity_at_each_height_and_severity) {
    constexpr double ft = 0.3048;
    // MIL-F-8785C's scales: low down set by the wind 20 ft up, high up by the
    // severity's curve (7.4 ft/s at 3,750 ft and 6.7 at 7,500 for moderate).
    const DrydenScales low = glideslope::world::dryden_scales(3, 100.0 * ft, 15.0);
    const double k = 0.177 + 0.000823 * 100.0;
    check(std::abs(low.length_w_m - 100.0 * ft) < 1e-9 &&
              std::abs(low.length_u_m - 100.0 / std::pow(k, 1.2) * ft) < 1e-9 &&
              std::abs(low.sigma_w_mps - 1.5) < 1e-12 &&
              std::abs(low.sigma_u_mps - 1.5 / std::pow(k, 0.4)) < 1e-12,
          "100 ft: MIL-F-8785C's low-altitude lengths and intensities");
    const DrydenScales high = glideslope::world::dryden_scales(3, 5000.0 * ft, 15.0);
    check(std::abs(high.length_u_m - 1750.0 * ft) < 1e-9 &&
              std::abs(high.sigma_u_mps - (7.4 - 1250.0 / 3750.0 * 0.7) * ft) < 1e-12,
          "5,000 ft: 1,750 ft lengths and the moderate curve's intensity");
    const DrydenScales none = glideslope::world::dryden_scales(0, 5000.0 * ft, 15.0);
    check(none.sigma_u_mps == 0.0 && none.sigma_w_mps == 0.0, "severity 0 is none");

    // The field itself: along 200 km of straight line at each height, each
    // component's RMS within 12% of its intensity.
    for (const double height_ft : {100.0, 1500.0, 5000.0, 20000.0}) {
        const DrydenScales s =
            glideslope::world::dryden_scales(4, height_ft * ft, 20.0);
        double sums[3] = {0.0, 0.0, 0.0};
        int n = 0;
        for (double x = 0.0; x < 200000.0; x += 10.0) {
            const Enu t = glideslope::world::turbulence(
                7, s, {x * 0.6, x * 0.8, height_ft * ft}, {0.0, 1.0, 0.0});
            // With the wind towards the north, u is north, v west, w up.
            sums[0] += t.north * t.north;
            sums[1] += t.east * t.east;
            sums[2] += t.up * t.up;
            ++n;
        }
        const double expected[3] = {s.sigma_u_mps, s.sigma_u_mps, s.sigma_w_mps};
        for (int c = 0; c < 3; ++c) {
            const double rms = std::sqrt(sums[c] / n);
            check(std::abs(rms - expected[c]) < 0.12 * expected[c],
                  std::to_string(height_ft) + " ft, component " + std::to_string(c) +
                      ": RMS " + std::to_string(rms) + " against " +
                      std::to_string(expected[c]));
        }
    }

    // And its length: along lines in three directions, at two heights, for four
    // seeds, the distance at which each component's correlation falls to 1/e,
    // over Dryden's length, is 1 on average, within 15%. One line alone
    // varies far more: 48 waves are a sample of the spectrum, not all of it.
    double ratios[3] = {0.0, 0.0, 0.0};
    int lines = 0;
    for (const std::uint64_t seed : {7ULL, 11ULL, 1234ULL, 99ULL}) {
        for (const double direction : {0.0, 1.0, 2.2}) {
            for (const double height_ft : {100.0, 5000.0}) {
                const DrydenScales s =
                    glideslope::world::dryden_scales(4, height_ft * ft, 20.0);
                const double step = s.length_w_m / 20.0;
                constexpr int n = 20000;
                std::vector<double> values[3];
                for (auto& v : values) {
                    v.resize(n);
                }
                for (int i = 0; i < n; ++i) {
                    const Enu t = glideslope::world::turbulence(
                        seed, s,
                        {i * step * std::cos(direction), i * step * std::sin(direction),
                         height_ft * ft},
                        {0.0, 1.0, 0.0});
                    values[0][static_cast<std::size_t>(i)] = t.north;
                    values[1][static_cast<std::size_t>(i)] = t.east;
                    values[2][static_cast<std::size_t>(i)] = t.up;
                }
                const double lengths[3] = {s.length_u_m, s.length_u_m, s.length_w_m};
                for (int c = 0; c < 3; ++c) {
                    const auto& x = values[c];
                    double variance = 0.0;
                    for (const double a : x) {
                        variance += a * a;
                    }
                    variance /= n;
                    double reach = 0.0;
                    for (int lag = 1; lag < n / 4; ++lag) {
                        double sum = 0.0;
                        for (int i = 0; i + lag < n; ++i) {
                            sum += x[static_cast<std::size_t>(i)] *
                                   x[static_cast<std::size_t>(i + lag)];
                        }
                        if (sum / (n - lag) < variance / std::exp(1.0)) {
                            reach = lag * step;
                            break;
                        }
                    }
                    ratios[c] += reach / lengths[c];
                }
                ++lines;
            }
        }
    }
    for (int c = 0; c < 3; ++c) {
        const double mean = ratios[c] / lines;
        check(std::abs(mean - 1.0) < 0.15,
              "component " + std::to_string(c) + ": correlated to " +
                  std::to_string(mean) + " of Dryden's length on average");
    }
}
